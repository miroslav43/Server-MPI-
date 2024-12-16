#include "common.h"
#include "comands.h"

int parse_command_line(const char *line, char *client_id, char *command, char *arg)
{
    if (strncmp(line, "WAIT", 4) == 0)
    {
        strcpy(client_id, "");
        strcpy(command, "WAIT");
        int ret = sscanf(line, "WAIT %s", arg);
        return (ret == 1) ? 0 : -1;
    }
    else if (strncmp(line, "CLI", 3) == 0)
    {
        char temp[CMD_LEN];
        int ret = sscanf(line, "%s %s %[^\n]", client_id, command, temp);
        if (ret < 3) return -1;

        strcpy(arg, temp);
        return 0;
    }
    else
    {
        return -1;
    }
}