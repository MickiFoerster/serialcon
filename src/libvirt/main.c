#include "commands.h"
#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  callVirshConsole_args console_argv = {
      .vmname = NULL,
      .username = NULL,
      .password = NULL,
  };

  if (argc > 1) {
    console_argv.vmname = argv[1];
  }
  if (argc > 2) {
    console_argv.username = argv[2];
  }
  if (argc > 3) {
    console_argv.password = argv[3];
  }
  if (console_argv.vmname == NULL || console_argv.username == NULL ||
      console_argv.password == NULL) {
    fprintf(stderr,
            "syntax error:\nusage: %s <VM name> <username> <password>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!add_command("hostname\n")) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  if (!add_command("sudo apt update && sudo apt upgrade -y && sudo apt "
                   "autoremove -y \n")) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  if (!add_command("sudo reboot\n")) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  if (!add_command("head /proc/cpuinfo\n")) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  int rc = callVirshConsole(&console_argv);
  // 0 = success, 1 = error => last command in done-list points to failed one
  switch (rc) {
  case 0:
    break;
  case 1: {
    command_t *cmd = failed_cmd();
    if (!cmd)
      break;
    printf("Command '");
    for (int i = 0; i < strlen(cmd->cmdline); ++i) {
      switch (cmd->cmdline[i]) {
      case '\r':
        printf("\\r");
        break;
      case '\n':
        printf("\\n");
        break;
      default:
        printf("%c", cmd->cmdline[i]);
        break;
      }
    }
    printf("' failed.\n");
    break;
  }
  default:
    break;
  }

  free_cmd_list();

  return rc;
}
