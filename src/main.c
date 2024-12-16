#include "common.h"
#include "utils.h"
#include "comands.h"

void init_queue(IntQueue *q, int capacity)
{
    q->data = (int *)malloc(capacity * sizeof(int));
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
}

void enqueue(IntQueue *q, int val)
{
    if (q->rear < q->capacity)
    {
        q->data[q->rear++] = val;
    }
}

int dequeue(IntQueue *q)
{
    if (q->front < q->rear)
        return q->data[q->front++];
    return -1;
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
        main_server(world_size, argv[1]);
    }
    else
    {
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

void receive_worker_result(int world_size, int *worker_free, FILE *log, int *commands_received, FILE *f)
{
    MPI_Status status;
    char header[1024];
    MPI_Recv(header, 1024, MPI_CHAR, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);

    int cmd_index = dequeue(&waiting_commands);
    if (cmd_index < 0)
    {
        fprintf(log, "ERROR: Received a result but no command is waiting.\n");
        fflush(log);
        worker_free[status.MPI_SOURCE] = 1;
        return;
    }

    char client_id[64] = "UNKNOWN";
    {
        char *space = strchr(header, ' ');
        if (space != NULL)
        {
            int len = (int)(space - header);
            if (len > 63)
                len = 63;
            strncpy(client_id, header, len);
            client_id[len] = '\0';
        }
    }

    char filename[256];
    sprintf(filename, "output/%s_result.txt", client_id);
    FILE *cf = fopen(filename, "a");
    if (!cf)
    {
        fprintf(log, "ERROR: Could not open %s for writing result.\n", filename);
        fflush(log);
        worker_free[status.MPI_SOURCE] = 1;
        (*commands_received)++;
        return;
    }

    if (strstr(header, "MATRIXRESULT") != NULL)
    {
        char dummy[64];
        int N, start_row, end_row;
        if (sscanf(header, "%s MATRIXRESULT %d %d %d", dummy, &N, &start_row, &end_row) == 4)
        {
            int rows = end_row - start_row;
            float *C_data = (float *)malloc(rows * N * sizeof(float));
            if (!C_data)
            {
                fprintf(log, "ERROR: Memory allocation failed for receiving matrix data.\n");
                fflush(log);
                fclose(cf);
                worker_free[status.MPI_SOURCE] = 1;
                (*commands_received)++;
                return;
            }

            MPI_Status mat_status;
            MPI_Recv(C_data, rows * N, MPI_FLOAT, status.MPI_SOURCE, TAG_MATRIX_RESULT, MPI_COMM_WORLD, &mat_status);

            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < N; j++)
                {
                    fprintf(cf, "%f%c", C_data[i * N + j], (j == N - 1) ? '\n' : ' ');
                }
            }

            free(C_data);
        }
        else
        {
            fprintf(log, "ERROR: Malformed matrix result header: %s\n", header);
            fflush(log);
        }
    }
    else
    {
        fprintf(cf, "%s\n", header);
    }

    fclose(cf);

    double completion_time = MPI_Wtime();
    tasks[cmd_index].completion_time = completion_time;
    fprintf(log, "COMPLETED: %s TIME: %f\n", client_id, completion_time);
    fflush(log);

    worker_free[status.MPI_SOURCE] = 1;
    (*commands_received)++;
}

static void handle_parallel_matrix(FILE *log, const char *client_id, const char *command, int N,
                                   const char *f1, const char *f2, int world_size, int *worker_free,
                                   int *commands_received, FILE *f, int cmd_index)
{
    float **A = read_matrix(f1, N);
    float **B = read_matrix(f2, N);
    if (!A || !B)
    {
        fprintf(log, "ERROR: Could not read matrix files %s or %s\n", f1, f2);
        fflush(log);
        if (A)
            free_matrix(A, N);
        if (B)
            free_matrix(B, N);
        return;
    }

    int num_workers = world_size - 1;
    if (num_workers <= 0)
    {
        fprintf(log, "ERROR: No workers available for parallel matrix.\n");
        fflush(log);
        free_matrix(A, N);
        free_matrix(B, N);
        return;
    }

    int rows_per_worker = N / num_workers;
    int remainder = N % num_workers;

    double dispatch_time = MPI_Wtime();
    tasks[cmd_index].dispatch_time = dispatch_time;

    int start_row = 0;
    for (int w = 1; w <= num_workers; w++)
    {
        int end_row = start_row + rows_per_worker + (w == num_workers ? remainder : 0);
        if (end_row > N)
            end_row = N;

        int free_worker = -1;
        while (free_worker == -1)
        {
            free_worker = find_free_worker(world_size, worker_free);
            if (free_worker == -1)
            {
                while (poll_for_result())
                {
                    receive_worker_result(world_size, worker_free, log, commands_received, f);
                }
            }
        }

        worker_free[free_worker] = 0;

        char sub_cmd[CMD_LEN];
        sprintf(sub_cmd, "%s %s %d %d %d", client_id, command, N, start_row, end_row);
        MPI_Send(sub_cmd, (int)strlen(sub_cmd) + 1, MPI_CHAR, free_worker, TAG_MATRIX_TASK, MPI_COMM_WORLD);

        int chunk_rows = end_row - start_row;
        float *A_data = (float *)malloc(chunk_rows * N * sizeof(float));
        float *B_data = (float *)malloc(chunk_rows * N * sizeof(float));

        if (!A_data || !B_data)
        {
            fprintf(log, "ERROR: Memory allocation failed for parallel matrix data.\n");
            fflush(log);
            if (A_data)
                free(A_data);
            if (B_data)
                free(B_data);
            free_matrix(A, N);
            free_matrix(B, N);
            return;
        }

        for (int i = 0; i < chunk_rows; i++)
        {
            for (int j = 0; j < N; j++)
            {
                A_data[i * N + j] = A[start_row + i][j];
                B_data[i * N + j] = B[start_row + i][j];
            }
        }

        MPI_Send(A_data, chunk_rows * N, MPI_FLOAT, free_worker, TAG_MATRIX_TASK, MPI_COMM_WORLD);
        MPI_Send(B_data, chunk_rows * N, MPI_FLOAT, free_worker, TAG_MATRIX_TASK, MPI_COMM_WORLD);

        free(A_data);
        free(B_data);

        enqueue(&waiting_commands, cmd_index);

        start_row = end_row;
    }

    free_matrix(A, N);
    free_matrix(B, N);
}

static void handle_single_worker_matrix(FILE *log, const char *client_id, const char *command, int N,
                                        const char *f1, const char *f2, int world_size, int *worker_free,
                                        int *commands_received, FILE *f, int cmd_index)
{
    float **A = read_matrix(f1, N);
    float **B = read_matrix(f2, N);
    if (!A || !B)
    {
        fprintf(log, "ERROR: Could not read matrix files %s or %s\n", f1, f2);
        fflush(log);
        if (A)
            free_matrix(A, N);
        if (B)
            free_matrix(B, N);
        return;
    }

    int free_worker = -1;
    while (free_worker == -1)
    {
        free_worker = find_free_worker(world_size, worker_free);
        if (free_worker == -1)
        {
            while (poll_for_result())
            {
                receive_worker_result(world_size, worker_free, log, commands_received, f);
            }
        }
    }
    worker_free[free_worker] = 0;

    char fake_line[CMD_LEN];
    sprintf(fake_line, "%s %s %d %s %s", client_id, command, N, f1, f2);
    double dispatch_time = MPI_Wtime();
    tasks[cmd_index].dispatch_time = dispatch_time;
    MPI_Send(fake_line, (int)strlen(fake_line) + 1, MPI_CHAR, free_worker, TAG_WORK, MPI_COMM_WORLD);

    float *A_data = (float *)malloc(N * N * sizeof(float));
    float *B_data = (float *)malloc(N * N * sizeof(float));
    if (!A_data || !B_data)
    {
        fprintf(log, "ERROR: Memory allocation failed for single-worker matrix data.\n");
        fflush(log);
        if (A_data)
            free(A_data);
        if (B_data)
            free(B_data);
        free_matrix(A, N);
        free_matrix(B, N);
        return;
    }

    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            A_data[i * N + j] = A[i][j];
            B_data[i * N + j] = B[i][j];
        }
    }
    MPI_Send(A_data, N * N, MPI_FLOAT, free_worker, TAG_WORK, MPI_COMM_WORLD);
    MPI_Send(B_data, N * N, MPI_FLOAT, free_worker, TAG_WORK, MPI_COMM_WORLD);

    free(A_data);
    free(B_data);
    free_matrix(A, N);
    free_matrix(B, N);

    enqueue(&waiting_commands, cmd_index);
}

void write_csv(const char *filename, CommandInfo *tasks, int total_commands)
{
    FILE *csv = fopen(filename, "w");
    if (!csv)
    {
        fprintf(stderr, "Error: Could not open %s for writing CSV.\n", filename);
        return;
    }
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

void main_server(int world_size, const char *command_file)
{
    mkdir("output", 0777);

    FILE *f = fopen(command_file, "r");
    if (!f)
    {
        fprintf(stderr, "Error opening command file %s\n", command_file);
        return;
    }

    FILE *log = fopen("output/server_log.txt", "w");
    if (!log)
    {
        fprintf(stderr, "Error opening log file\n");
        fclose(f);
        return;
    }

    int worker_free[world_size];
    for (int i = 1; i < world_size; i++)
    {
        worker_free[i] = 1;
    }

    int total_commands = 0;
    char line[1024];
    fseek(f, 0, SEEK_SET);
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "CLI", 3) == 0)
            total_commands++;
    }
    fseek(f, 0, SEEK_SET);

    if (total_commands > 0)
    {
        tasks = (CommandInfo *)malloc(total_commands * sizeof(CommandInfo));
        if (!tasks)
        {
            fprintf(stderr, "Error allocating memory for tasks.\n");
            fclose(f);
            fclose(log);
            return;
        }
    }

    init_queue(&waiting_commands, total_commands);

    int commands_sent = 0;
    int commands_received = 0;
    int cmd_index = 0;

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
            if (parse_command_line(line, client_id, command, arg) == 0)
            {
                double arrival_time = MPI_Wtime();
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
                fflush(log);

                if (strncmp(command, "MATRIX", 6) == 0)
                {
                    int N;
                    char f1[256], f2[256];
                    if (sscanf(arg, "%d %s %s", &N, f1, f2) != 3)
                    {
                        fprintf(log, "ERROR: Malformed MATRIX args: %s\n", arg);
                        fflush(log);
                    }
                    else
                    {
                        if (N > MATRIX_THRESHOLD)
                        {
                            handle_parallel_matrix(log, client_id, command, N, f1, f2, world_size, worker_free, &commands_received, f, cmd_index);
                        }
                        else
                        {
                            handle_single_worker_matrix(log, client_id, command, N, f1, f2, world_size, worker_free, &commands_received, f, cmd_index);
                        }
                        commands_sent++;
                        cmd_index++;
                    }
                }
                else
                {
                    int free_worker = -1;
                    while (free_worker == -1)
                    {
                        free_worker = find_free_worker(world_size, worker_free);
                        if (free_worker == -1)
                        {
                            while (poll_for_result())
                            {
                                receive_worker_result(world_size, worker_free, log, &commands_received, f);
                            }
                        }
                    }

                    worker_free[free_worker] = 0;
                    double dispatch_time = MPI_Wtime();
                    if (cmd_index < total_commands)
                        tasks[cmd_index].dispatch_time = dispatch_time;
                    MPI_Send(line, (int)strlen(line) + 1, MPI_CHAR, free_worker, TAG_WORK, MPI_COMM_WORLD);

                    fprintf(log, "DISPATCHED: %s TO: %d TIME: %f\n", client_id, free_worker, dispatch_time);
                    fflush(log);

                    enqueue(&waiting_commands, cmd_index);
                    commands_sent++;
                    cmd_index++;
                }
            }
            else
            {
                fprintf(log, "ERROR: Malformed command: %s\n", line);
                fflush(log);
            }
        }

        while (poll_for_result())
        {
            receive_worker_result(world_size, worker_free, log, &commands_received, f);
        }
    }

    while (commands_received < total_commands)
    {
        while (poll_for_result())
        {
            receive_worker_result(world_size, worker_free, log, &commands_received, f);
        }
    }

    for (int i = 1; i < world_size; i++)
    {
        MPI_Send(NULL, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
    }

    fclose(f);
    fclose(log);

    if (total_commands > 0)
    {
        write_csv("output/tasks.csv", tasks, total_commands);
        free(tasks);
    }
    free(waiting_commands.data);
}