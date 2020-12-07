#pragma once
#include <stdbool.h>

typedef struct command_s command_t;

struct command_s {
  const char *cmdline;
  int exitcode;
};

bool add_command(const char *cmd);
void free_cmd_list(void);
command_t *next_cmd(void);
command_t *failed_cmd(void);
