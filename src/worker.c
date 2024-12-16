#include "common.h"
#include "utils.h"
#include "comands.h"

// Worker process logic
void worker_process(int rank) {
    MPI_Status status;
    char cmd[CMD_LEN];

    while (1) {
        MPI_Recv(cmd, CMD_LEN, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_STOP) {
            break; // stop worker
        }

        char client_id[64], command[64], arg[512];
        if (parse_command_line(cmd, client_id, command, arg) != 0) {
            char result[1024];
            sprintf(result, "ERROR: Malformed command");
            MPI_Send(result, (int)strlen(result)+1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
            continue;
        }

        char result[1024];
        if (strcmp(command, "PRIMES") == 0) {
            long N = atol(arg);
            int prime_count = count_primes_up_to(N);
            sprintf(result, "%s %d", client_id, prime_count);
        } else if (strcmp(command, "PRIMEDIVISORS") == 0) {
            long N = atol(arg);
            int pd = count_prime_divisors(N);
            sprintf(result, "%s %d", client_id, pd);
        } else if (strcmp(command, "ANAGRAMS") == 0) {
            long cnt = anagram_count(arg);
            sprintf(result, "%s Total anagrams: %ld", client_id, cnt);
        } else if (strcmp(command, "WAIT") == 0) {
            // WAIT should not come to workers, but if it does, just ignore
            sprintf(result, "ERROR: Worker received WAIT command");
        } else {
            sprintf(result, "%s ERROR: Unknown command", client_id);
        }

        MPI_Send(result, (int)strlen(result)+1, MPI_CHAR, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
}
