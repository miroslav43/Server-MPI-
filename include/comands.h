#ifndef COMMANDS_H
#define COMMANDS_H

// Function to parse a command line and determine what to do
// E.g. returns a struct or sets arguments
int parse_command_line(const char *line, char *client_id, char *command, char *arg);

#endif
