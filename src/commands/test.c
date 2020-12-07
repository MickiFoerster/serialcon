#include "commands.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

int main() {
  const char *cmds[] = {"ls -l -h\n", "hostname\n"};
  for (int i = 0; i < 10; ++i) {
    bool rc = add_command(cmds[i % 2]);
    assert(rc);
  }

  command_t *cmd;
  int counter = 0;
  while ((cmd = next_cmd())) {
    printf("%s", cmd->cmdline);
    cmd->exitcode = counter++;
    printf("return code: %d\n", cmd->exitcode);
  }

  free_cmd_list();
  return 0;
}
