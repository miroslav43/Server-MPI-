#include "common.h"
#include "utils.h"
#include "comands.h"

static void send_error_message(const char *client_id, const char *error_msg)
{
    char buf[1024];
    if (client_id && strlen(client_id) > 0)
        sprintf(buf, "%s ERROR: %s", client_id, error_msg);
    else
        sprintf(buf, "ERROR: %s", error_msg);

    MPI_Send(buf, (int)strlen(buf) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
}

static void send_matrix_result(const char *client_id, int N, int start_row, int end_row, float *data)
{
    char header[256];
    sprintf(header, "%s MATRIXRESULT %d %d %d", client_id, N, start_row, end_row);

    MPI_Send(header, (int)strlen(header) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    int rows = end_row - start_row;
    MPI_Send(data, rows * N, MPI_FLOAT, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD);
}

static void send_full_matrix_result(const char *client_id, int N, float *data)
{
    char header[256];
    sprintf(header, "%s MATRIXRESULT %d 0 %d", client_id, N, N);

    MPI_Send(header, (int)strlen(header) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    MPI_Send(data, N * N, MPI_FLOAT, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD);
}

static void process_matrix_subtask(const char *cmd)
{
    char client_id[64], command[64];
    int N, start_row, end_row;
    if (sscanf(cmd, "%s %s %d %d %d", client_id, command, &N, &start_row, &end_row) != 5)
    {
        send_error_message("", "Malformed matrix subtask command");
        return;
    }

    if (N <= 0 || start_row < 0 || end_row <= start_row)
    {
        send_error_message(client_id, "Invalid matrix dimensions for subtask");
        return;
    }

    int rows = end_row - start_row;

    // Receive matrix segments from master
    float *A_data = (float *)malloc(rows * N * sizeof(float));
    float *B_data = (float *)malloc(rows * N * sizeof(float));
    if (!A_data || !B_data)
    {
        send_error_message(client_id, "Memory allocation failed in worker for matrix subtask");
        free(A_data);
        free(B_data);
        return;
    }

    MPI_Status status;
    MPI_Recv(A_data, rows * N, MPI_FLOAT, 0, TAG_MATRIX_TASK, MPI_COMM_WORLD, &status);
    MPI_Recv(B_data, rows * N, MPI_FLOAT, 0, TAG_MATRIX_TASK, MPI_COMM_WORLD, &status);

    float **A_sub = alloc_matrix(rows);
    float **B_sub = alloc_matrix(rows);
    float **C_sub = alloc_matrix(rows);
    if (!A_sub || !B_sub || !C_sub)
    {
        send_error_message(client_id, "Matrix allocation failed in worker");
        if (A_sub)
            free_matrix(A_sub, rows);
        if (B_sub)
            free_matrix(B_sub, rows);
        if (C_sub)
            free_matrix(C_sub, rows);
        free(A_data);
        free(B_data);
        return;
    }

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < N; j++)
        {
            A_sub[i][j] = A_data[i * N + j];
            B_sub[i][j] = B_data[i * N + j];
        }
    }

    if (strcmp(command, "MATRIXADD") == 0)
    {
        matrix_add(A_sub, B_sub, C_sub, 0, rows, N);
    }
    else if (strcmp(command, "MATRIXMULT") == 0)
    {
        matrix_mult(A_sub, B_sub, C_sub, 0, rows, N);
    }
    else
    {
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < N; j++)
                C_sub[i][j] = 0.0f;
    }

    float *C_data = (float *)malloc(rows * N * sizeof(float));
    if (!C_data)
    {
        send_error_message(client_id, "Memory allocation for C_data failed");
        free_matrix(A_sub, rows);
        free_matrix(B_sub, rows);
        free_matrix(C_sub, rows);
        free(A_data);
        free(B_data);
        return;
    }

    for (int i = 0; i < rows; i++)
        for (int j = 0; j < N; j++)
            C_data[i * N + j] = C_sub[i][j];

    send_matrix_result(client_id, N, start_row, end_row, C_data);

    free(A_data);
    free(B_data);
    free(C_data);
    free_matrix(A_sub, rows);
    free_matrix(B_sub, rows);
    free_matrix(C_sub, rows);
}

static void process_work_command(const char *cmd)
{
    char client_id[64], command[64], arg[512];
    if (parse_command_line(cmd, client_id, command, arg) != 0)
    {
        send_error_message("", "Malformed command");
        return;
    }

    if (strcmp(command, "PRIMES") == 0)
    {
        long N = atol(arg);
        if (N <= 0)
        {
            send_error_message(client_id, "Invalid number for PRIMES");
            return;
        }
        int prime_count = count_primes_up_to(N);
        char result[1024];
        sprintf(result, "%s %d", client_id, prime_count);
        MPI_Send(result, (int)strlen(result) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
    else if (strcmp(command, "PRIMEDIVISORS") == 0)
    {
        long N = atol(arg);
        if (N <= 0)
        {
            send_error_message(client_id, "Invalid number for PRIMEDIVISORS");
            return;
        }
        int pd = count_prime_divisors(N);
        char result[1024];
        sprintf(result, "%s %d", client_id, pd);
        MPI_Send(result, (int)strlen(result) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
    else if (strcmp(command, "ANAGRAMS") == 0)
    {
        long cnt = anagram_count(arg);
        char result[1024];
        sprintf(result, "%s Total anagrams: %ld", client_id, cnt);
        MPI_Send(result, (int)strlen(result) + 1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
    else if (strcmp(command, "WAIT") == 0)
    {
        send_error_message("", "Worker received WAIT command");
    }
    else if (strncmp(command, "MATRIX", 6) == 0)
    {
        int N;
        char f1[256], f2[256];
        if (sscanf(arg, "%d %s %s", &N, f1, f2) != 3)
        {
            send_error_message(client_id, "Malformed MATRIX arguments");
            return;
        }

        if (N <= 0)
        {
            send_error_message(client_id, "Invalid matrix size");
            return;
        }

        MPI_Status status;
        float *A_data = (float *)malloc(N * N * sizeof(float));
        float *B_data = (float *)malloc(N * N * sizeof(float));
        if (!A_data || !B_data)
        {
            send_error_message(client_id, "Memory allocation failed for single MATRIX operation");
            free(A_data);
            free(B_data);
            return;
        }

        MPI_Recv(A_data, N * N, MPI_FLOAT, 0, TAG_WORK, MPI_COMM_WORLD, &status);
        MPI_Recv(B_data, N * N, MPI_FLOAT, 0, TAG_WORK, MPI_COMM_WORLD, &status);

        float **A = alloc_matrix(N);
        float **B = alloc_matrix(N);
        float **C = alloc_matrix(N);
        if (!A || !B || !C)
        {
            send_error_message(client_id, "Matrix allocation failed");
            if (A)
                free_matrix(A, N);
            if (B)
                free_matrix(B, N);
            if (C)
                free_matrix(C, N);
            free(A_data);
            free(B_data);
            return;
        }

        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                A[i][j] = A_data[i * N + j];
                B[i][j] = B_data[i * N + j];
            }
        }

        if (strcmp(command, "MATRIXADD") == 0)
        {
            matrix_add(A, B, C, 0, N, N);
        }
        else if (strcmp(command, "MATRIXMULT") == 0)
        {
            matrix_mult(A, B, C, 0, N, N);
        }
        else
        {
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)
                    C[i][j] = 0.0f;
        }

        float *C_data = (float *)malloc(N * N * sizeof(float));
        if (!C_data)
        {
            send_error_message(client_id, "Memory allocation for result matrix failed");
            free_matrix(A, N);
            free_matrix(B, N);
            free_matrix(C, N);
            free(A_data);
            free(B_data);
            return;
        }

        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                C_data[i * N + j] = C[i][j];
            }
        }

        send_full_matrix_result(client_id, N, C_data);

        free_matrix(A, N);
        free_matrix(B, N);
        free_matrix(C, N);
        free(A_data);
        free(B_data);
        free(C_data);
    }
    else
    {
        send_error_message(client_id, "Unknown command");
    }
}

void worker_process(int rank)
{
    MPI_Status status;
    char cmd[CMD_LEN];

    while (1)
    {
        MPI_Recv(cmd, CMD_LEN, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_STOP)
        {
            break;
        }
        else if (status.MPI_TAG == TAG_MATRIX_TASK)
        {
            process_matrix_subtask(cmd);
        }
        else if (status.MPI_TAG == TAG_WORK)
        {
            process_work_command(cmd);
        }
        else
        {
            char error_msg[256];
            sprintf(error_msg, "Unknown MPI tag %d received by worker %d", status.MPI_TAG, rank);
            send_error_message("", error_msg);
        }
    }
}