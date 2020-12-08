#include "serialcon/serialcon.h"
#include "commands.h"
#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

serialcon_connection *serialcon_Open(const char *serial_dev, uint32_t baudrate,
                                     const char *user, const char *password) {
    serialcon_connection *conn =
        (serialcon_connection *)malloc(sizeof(serialcon_connection));
    if (conn) {
        conn->serial_dev = serial_dev;
        conn->baudrate = baudrate;
        conn->username = user;
        conn->password = password;

        if (is_conn_libvirt(serial_dev)) {
            int rc = libvirt_console_open(conn);
            if (rc != 0) {
                fprintf(stderr, "error: libvirt_console_open failed\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return conn;
}

void serialcon_Close(serialcon_connection *conn) {
    if (is_conn_libvirt(conn->serial_dev)) {
        libvirt_console_close(conn);
    }
    free(conn);
}

int serialcon_Run(serialcon_connection *conn, const char *cmd) {
  fprintf(stderr, "serialcon_Run\n");
  if (!add_command(cmd)) {
    fprintf(stderr, "error: could not add command\n");
    exit(EXIT_FAILURE);
  }

  int rc = -1;
  if (is_conn_libvirt(conn->serial_dev)) {
      printf("call is_conn_libvirt\n");
      rc = libvirt_console_run(conn, cmd);
  } else {
      rc = 0;
  }
  // 0 = success, 1 = error => last command in done-list points to failed
  // one
  switch (rc) {
      case 0:
          break;
      case 1: {
          command_t *cmd = failed_cmd();
          if (!cmd) break;
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
