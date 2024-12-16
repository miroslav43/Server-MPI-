#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TAG_WORK 1
#define TAG_RESULT 2
#define TAG_STOP 3
#define TAG_MATRIX_TASK 4
#define TAG_MATRIX_RESULT 5

#define CMD_LEN 1024

#define MATRIX_THRESHOLD 1024

typedef struct
{
    char client_id[64];
    char command[64];
    char arg[512];
    double arrival_time;
    double dispatch_time;
    double completion_time;
} CommandInfo;

void worker_process(int rank);

#endif