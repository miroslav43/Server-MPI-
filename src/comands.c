#include "common.h"
#include "comands.h"

int parse_command_line(const char *line, char *client_id, char *command, char *arg) {
    // Example: "CLI0 PRIMES 10000"
    // or "WAIT 2"
    // Return 0 on success, -1 on error
    if (strncmp(line, "WAIT", 4) == 0) {
        strcpy(client_id, "");
        strcpy(command, "WAIT");
        int ret = sscanf(line, "WAIT %s", arg);
        return (ret == 1) ? 0 : -1;
    } else if (strncmp(line, "CLI", 3) == 0) {
        int ret = sscanf(line, "%s %s %s", client_id, command, arg);
        return (ret == 3) ? 0 : -1;
    } else {
        return -1;
    }
}
