//FILE main.c
#include "common.h"
#include "utils.h"
#include "comands.h"

// Function prototypes
void log_event(FILE *logf, const char *event);
void main_server(int size, const char *cmd_file);
int find_free_worker(int world_size, int *worker_free);
int poll_for_result();
void receive_worker_result(int world_size, int *worker_free, FILE *log, int *commands_received, FILE *f);
void write_csv(const char *filename, CommandInfo *tasks, int total_commands);

// We'll maintain a queue of indices for commands that have been dispatched and are waiting for results
// A simple dynamic array can work as a queue here.
typedef struct
{
    int *data;
    int front;
    int rear;
    int capacity;
} IntQueue;

void init_queue(IntQueue *q, int capacity)
{
    q->data = (int *)malloc(capacity * sizeof(int));
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
}

void enqueue(IntQueue *q, int val)
{
    q->data[q->rear++] = val;
}

int dequeue(IntQueue *q)
{
    return q->data[q->front++];
}

int queue_empty(IntQueue *q)
{
    return q->front == q->rear;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int world_size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc < 2 && rank == 0)
    {
        fprintf(stderr, "Usage: %s command_file\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (rank == 0)
    {
        // This is the main server
        main_server(world_size, argv[1]); // Pass command_file as argument
    }
    else
    {
        // This is a worker
        worker_process(rank);
    }

    MPI_Finalize();
    return 0;
}

void log_event(FILE *logf, const char *event)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(logf, "[%02d:%02d:%02d] %s\n", t->tm_hour, t->tm_min, t->tm_sec, event);
    fflush(logf);
}

int find_free_worker(int world_size, int *worker_free)
{
    for (int i = 1; i < world_size; i++)
    {
        if (worker_free[i] == 1)
            return i;
    }
    return -1;
}

int poll_for_result()
{
    int flag;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
    return flag;
}

// We'll define a global pointer to tasks and related data here for simplicity.
// Alternatively, you can pass these as parameters.
static CommandInfo *tasks = NULL;
static IntQueue waiting_commands;

void receive_worker_result(int world_size, int *worker_free, FILE *log, int *commands_received, FILE *f)
{
    MPI_Status status;
    char result[1024];
    MPI_Recv(result, 1024, MPI_CHAR, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);

    // Get the index of the command that just completed from the queue
    int cmd_index = dequeue(&waiting_commands);

    // Parse client_id from the result to confirm correctness
    char client_id[64];
    char *space = strchr(result, ' ');
    if (space != NULL)
    {
        int len = (int)(space - result);
        strncpy(client_id, result, len);
        client_id[len] = '\0';
    }
    else
    {
        // Malformed result or error line, still proceed
        strcpy(client_id, "UNKNOWN");
    }

    // Write to client's result file
    char filename[256];
    sprintf(filename, "output/%s_result.txt", client_id);
    FILE *cf = fopen(filename, "a");
    if (cf)
    {
        fprintf(cf, "%s\n", result);
        fclose(cf);
    }

    double completion_time = MPI_Wtime();
    tasks[cmd_index].completion_time = completion_time;
    fprintf(log, "COMPLETED: %s TIME: %f\n", client_id, completion_time);

    // Mark worker as free
    worker_free[status.MPI_SOURCE] = 1;
    (*commands_received)++;
}

void main_server(int world_size, const char *command_file)
{
    // Create output directory if not exists
    mkdir("output", 0777);

    // Open command file
    FILE *f = fopen(command_file, "r");
    if (!f)
    {
        fprintf(stderr, "Error opening command file\n");
        return;
    }

    // Open server_log file
    FILE *log = fopen("output/server_log.txt", "w");
    if (!log)
    {
        fprintf(stderr, "Error opening log file\n");
        fclose(f);
        return;
    }

    // Track worker availability
    int worker_free[world_size];
    for (int i = 1; i < world_size; i++)
    {
        worker_free[i] = 1;
    }

    char line[1024];
    int commands_sent = 0;
    int commands_received = 0;
    int total_commands = 0;

    // Count total commands
    fseek(f, 0, SEEK_SET);
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "CLI", 3) == 0)
            total_commands++;
    }
    fseek(f, 0, SEEK_SET);

    // Allocate memory for tasks
    if (total_commands > 0)
    {
        tasks = (CommandInfo *)malloc(total_commands * sizeof(CommandInfo));
    }

    init_queue(&waiting_commands, total_commands);

    int cmd_index = 0; // Index for storing command info

    // Main loop to read commands
    while (fgets(line, sizeof(line), f))
    {
        char client_id[64], command[64], arg[512];
        if (strncmp(line, "WAIT", 4) == 0)
        {
            int wait_time;
            if (sscanf(line, "WAIT %d", &wait_time) == 1)
            {
                sleep(wait_time);
            }
        }
        else
        {
            // Parse command line
            if (parse_command_line(line, client_id, command, arg) == 0)
            {
                // Record arrival time
                double arrival_time = MPI_Wtime();

                // Store command info
                if (cmd_index < total_commands)
                {
                    strncpy(tasks[cmd_index].client_id, client_id, sizeof(tasks[cmd_index].client_id));
                    strncpy(tasks[cmd_index].command, command, sizeof(tasks[cmd_index].command));
                    strncpy(tasks[cmd_index].arg, arg, sizeof(tasks[cmd_index].arg));
                    tasks[cmd_index].arrival_time = arrival_time;
                    tasks[cmd_index].dispatch_time = 0.0;
                    tasks[cmd_index].completion_time = 0.0;
                }

                fprintf(log, "ARRIVED: %s COMMAND: %s ARG: %s TIME: %f\n", client_id, command, arg, arrival_time);

                // Ensure we have a free worker
                int free_worker = -1;
                while (free_worker == -1)
                {
                    free_worker = find_free_worker(world_size, worker_free);
                    if (free_worker == -1)
                    {
                        // Wait for a result to free a worker
                        receive_worker_result(world_size, worker_free, log, &commands_received, f);
                    }
                }

                // We now have a free worker
                worker_free[free_worker] = 0; // Mark as busy

                // Send command to worker
                double dispatch_time = MPI_Wtime();
                tasks[cmd_index].dispatch_time = dispatch_time;
                MPI_Send(line, (int)strlen(line) + 1, MPI_CHAR, free_worker, TAG_WORK, MPI_COMM_WORLD);
                fprintf(log, "DISPATCHED: %s TO: %d TIME: %f\n", client_id, free_worker, dispatch_time);

                // Add this command to the queue of waiting results
                enqueue(&waiting_commands, cmd_index);

                cmd_index++;
                commands_sent++;
            }
            else
            {
                // Malformed command
                fprintf(log, "ERROR: Malformed command: %s\n", line);
            }
        }

        // Continuously check for results
        while (poll_for_result())
        {
            receive_worker_result(world_size, worker_free, log, &commands_received, f);
        }
    }

    // After finishing reading the file, wait for all outstanding results
    while (commands_received < total_commands)
    {
        receive_worker_result(world_size, worker_free, log, &commands_received, f);
    }

    // Send stop signal to workers
    for (int i = 1; i < world_size; i++)
    {
        MPI_Send(NULL, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
    }

    fclose(f);
    fclose(log);

    // Write CSV file with measurements
    if (total_commands > 0)
    {
        write_csv("output/tasks.csv", tasks, total_commands);
        free(tasks);
    }
    free(waiting_commands.data);
}

void write_csv(const char *filename, CommandInfo *tasks, int total_commands)
{
    FILE *csv = fopen(filename, "w");
    if (!csv)
    {
        fprintf(stderr, "Error: Could not open %s for writing CSV.\n", filename);
        return;
    }
    // CSV Header
    fprintf(csv, "client_id,command,arg,arrival_time,dispatch_time,completion_time,total_time\n");
    for (int i = 0; i < total_commands; i++)
    {
        double total_time = tasks[i].completion_time - tasks[i].arrival_time;
        fprintf(csv, "%s,%s,%s,%f,%f,%f,%f\n",
                tasks[i].client_id,
                tasks[i].command,
                tasks[i].arg,
                tasks[i].arrival_time,
                tasks[i].dispatch_time,
                tasks[i].completion_time,
                total_time);
    }
    fclose(csv);
}