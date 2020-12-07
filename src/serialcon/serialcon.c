#include "serialcon/serialcon.h"
#include "commands.h"
#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

serialcon_connection *serialcon_Open(const char *serial_dev, uint32_t baudrate,
                                     const char *user, const char *password) {
  serialcon_connection *sc =
      (serialcon_connection *)malloc(sizeof(serialcon_connection));
  if (sc) {
    const char libvirt_prefix[] = "/libvirt/";
    if (strstr(serial_dev, libvirt_prefix) == serial_dev) {
      serial_dev += strlen(libvirt_prefix);

      callVirshConsole_args console_argv = {
          .vmname = (char *)serial_dev,
          .username = user,
          .password = password,
      };
      int rc = libvirt_console_open(&console_argv);
      if (rc != 0) {
        fprintf(stderr, "error: libvirt_console_open failed\n");
        exit(EXIT_FAILURE);
      }
    }

    sc->serial_dev = serial_dev;
    sc->baudrate = baudrate;
    sc->username = user;
    sc->password = password;
  }
  return sc;
}

void serialcon_Close(serialcon_connection *conn) { free(conn); }

int serialcon_Run(serialcon_connection *conn, const char *cmd) {
  fprintf(stderr, "serialcon_Run\n");
  if (!add_command(cmd)) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  callVirshConsole_args console_argv = {
      .vmname = (char *)conn->serial_dev,
      .username = conn->username,
      .password = conn->password,
  };
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
  return 0;
}

bool serialcon_Upload(serialcon_connection *conn, const char *source_file_path,
                      const char *target_file_path) {
  abort(); // not implemented
  return true;
}
