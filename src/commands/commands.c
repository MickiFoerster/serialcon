#include "commands.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct list_of_cmds_s list_of_cmds_t;
struct list_of_cmds_s {
  command_t cmd;
  list_of_cmds_t *next;
};

list_of_cmds_t cmd_list = {
    .next = NULL,
};
list_of_cmds_t cmd_list_done = {
    .next = NULL,
};

bool add_command(const char *cmd) {
  list_of_cmds_t *new_elem = (list_of_cmds_t *)malloc(sizeof(list_of_cmds_t));
  if (!new_elem)
    return false;

  new_elem->cmd.cmdline = cmd;
  new_elem->cmd.exitcode = -1;
  new_elem->next = NULL;

  list_of_cmds_t *p = &cmd_list;
  while (p->next != NULL)
    p = p->next;

  p->next = new_elem;
  return true;
}

void free_cmd_list(void) {
  list_of_cmds_t *p = cmd_list.next;
  while (p != NULL) {
    list_of_cmds_t *tmp = p;
    p = p->next;
    free(tmp);
  }

  p = cmd_list_done.next;
  while (p != NULL) {
    list_of_cmds_t *tmp = p;
    p = p->next;
    free(tmp);
  }
}

command_t *next_cmd(void) {
  if (!cmd_list.next)
    return NULL;

  command_t *cmd = &cmd_list.next->cmd;
  list_of_cmds_t *new_done = cmd_list.next;
  cmd_list.next = cmd_list.next->next;
  new_done->next = NULL;

  list_of_cmds_t *runner_through_done_list = &cmd_list_done;
  while (runner_through_done_list->next != NULL)
    runner_through_done_list = runner_through_done_list->next;
  runner_through_done_list->next = new_done;

  return cmd;
}

command_t *failed_cmd(void) {
  list_of_cmds_t *runner_through_done_list = &cmd_list_done;
  if (!cmd_list_done.next)
    return NULL;
  while (runner_through_done_list->next != NULL)
    runner_through_done_list = runner_through_done_list->next;
  return &runner_through_done_list->cmd;
}