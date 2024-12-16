#ifndef UTILS_H
#define UTILS_H

int count_primes_up_to(long N);
int count_prime_divisors(long N);
long anagram_count(const char *name);

float **alloc_matrix(int N);
void free_matrix(float **mat, int N);
float **read_matrix(const char *filename, int N);
void write_matrix(const char *filename, float **mat, int N);
void matrix_add(float **A, float **B, float **C, int start_row, int end_row, int N);
void matrix_mult(float **A, float **B, float **C, int start_row, int end_row, int N);

typedef struct
{
    int *data;
    int front;
    int rear;
    int capacity;
} IntQueue;

void log_event(FILE *logf, const char *event);
void main_server(int size, const char *cmd_file);
int find_free_worker(int world_size, int *worker_free);
int poll_for_result();
void receive_worker_result(int world_size, int *worker_free, FILE *log, int *commands_received, FILE *f);
void write_csv(const char *filename, CommandInfo *tasks, int total_commands);

static CommandInfo *tasks = NULL;
static IntQueue waiting_commands;

#endif