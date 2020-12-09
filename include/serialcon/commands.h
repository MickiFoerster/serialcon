#pragma once
#include <stdbool.h>

typedef struct command_s command_t;

struct command_s {
  const char *cmdline;
  int exitcode;
};

typedef struct list_of_cmds_s list_of_cmds_t;
struct list_of_cmds_s {
  command_t cmd;
  list_of_cmds_t *next;
};

