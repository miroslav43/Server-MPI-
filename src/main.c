#include "common.h"
#include "utils.h"
#include "comands.h"
#include <unistd.h>
#include <errno.h>

// Prototypes
void log_event(FILE *logf, const char *event);
void main_server(int size, const char *cmd_file);

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (size < 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "Need at least 2 processes\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == 0)
    {
        if (argc < 2)
        {
            fprintf(stderr, "Usage: %s command_file\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        main_server(size, argv[1]);
    }
    else
    {
        extern void worker_process(int rank);
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

void main_server(int size, const char *cmd_file)
{
    FILE *f = fopen(cmd_file, "r");
    if (!f)
    {
        perror("Could not open command file");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    FILE *logf = fopen("server_log.txt", "w");
    if (!logf)
    {
        perror("Cannot open log file");
        fclose(f);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Create the output folder if it doesn't exist
    if (mkdir("output", 0777) && errno != EEXIST)
    {
        perror("Error creating output directory");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int *workers_free = malloc((size - 1) * sizeof(int));
    for (int i = 0; i < size - 1; i++)
        workers_free[i] = 1;

    int active_jobs = 0;
    char line[CMD_LEN];
    MPI_Status status;

    // Track which CLI commands have been processed
    int processed_cli[100] = {0}; // Assume at most 100 CLI commands

    while (fgets(line, CMD_LEN, f))
    {
        line[strcspn(line, "\n")] = '\0'; // Strip newline

        char client_id[64], command[64], arg[512];
        if (parse_command_line(line, client_id, command, arg) != 0)
        {
            // Malformed line or unknown format
            continue;
        }

        if (strncmp(client_id, "CLI", 3) == 0)
        {
            // Mark CLI command as seen
            int cli_id = atoi(client_id + 3);
            processed_cli[cli_id] = 1;
        }

        if (strcmp(command, "WAIT") == 0)
        {
            log_event(logf, "Main server received WAIT command");
            int wait_time = atoi(arg);
            sleep(wait_time);
            continue;
        }

        // Log received command
        char event_msg[256];
        snprintf(event_msg, sizeof(event_msg), "Received command from %s: %s %s", client_id, command, arg);
        log_event(logf, event_msg);

        // Check if any worker finished before dispatching a new one
        while (1)
        {
            int flag;
            MPI_Iprobe(MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
            if (!flag)
                break;

            char result[1024];
            MPI_Recv(result, 1024, MPI_CHAR, status.MPI_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
            workers_free[status.MPI_SOURCE - 1] = 1;
            active_jobs--;

            snprintf(event_msg, sizeof(event_msg), "Result received from worker %d: %s", status.MPI_SOURCE, result);
            log_event(logf, event_msg);

            char res_client_id[64];
            char res_output[512];
            sscanf(result, "%s %[^\n]", res_client_id, res_output);

            char res_filename[128];
            snprintf(res_filename, sizeof(res_filename), "output/%s_result.txt", res_client_id);
            FILE *rf = fopen(res_filename, "a");
            if (rf)
            {
                fprintf(rf, "%s\n", res_output);
                fclose(rf);
            }
        }

        int free_worker = -1;
        for (int i = 0; i < size - 1; i++)
        {
            if (workers_free[i])
            {
                free_worker = i + 1;
                break;
            }
        }

        if (free_worker == -1)
        {
            char result[1024];
            MPI_Recv(result, 1024, MPI_CHAR, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
            workers_free[status.MPI_SOURCE - 1] = 1;
            active_jobs--;

            snprintf(event_msg, sizeof(event_msg), "A job finished, freeing worker %d. Result: %s", status.MPI_SOURCE, result);
            log_event(logf, event_msg);
        }

        workers_free[free_worker - 1] = 0;
        MPI_Send(line, (int)strlen(line) + 1, MPI_CHAR, free_worker, TAG_CMD, MPI_COMM_WORLD);
        active_jobs++;

        snprintf(event_msg, sizeof(event_msg), "Dispatched job to worker %d", free_worker);
        log_event(logf, event_msg);
    }

    // Wait for all jobs to finish
    while (active_jobs > 0)
    {
        char result[1024];
        MPI_Recv(result, 1024, MPI_CHAR, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
        workers_free[status.MPI_SOURCE - 1] = 1;
        active_jobs--;

        char event_msg[256];
        snprintf(event_msg, sizeof(event_msg), "Final cleanup: job finished from worker %d: %s", status.MPI_SOURCE, result);
        log_event(logf, event_msg);

        char res_client_id[64];
        char res_output[512];
        sscanf(result, "%s %[^\n]", res_client_id, res_output);

        char res_filename[128];
        snprintf(res_filename, sizeof(res_filename), "output/%s_result.txt", res_client_id);
        FILE *rf = fopen(res_filename, "a");
        if (rf)
        {
            fprintf(rf, "%s\n", res_output);
            fclose(rf);
        }
    }

    // Ensure all CLI commands generate result files
    for (int i = 0; i < 100; i++)
    {
        if (processed_cli[i])
        {
            char res_filename[128];
            snprintf(res_filename, sizeof(res_filename), "output/CLI%d_result.txt", i);
            FILE *rf = fopen(res_filename, "a");
            if (rf)
            {
                fclose(rf); // Create empty file if it doesn't exist
            }
        }
    }

    // Stop workers
    for (int i = 1; i < size; i++)
    {
        MPI_Send(NULL, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
    }

    fclose(f);
    fclose(logf);
    free(workers_free);
}
