#include "command-line.h"
#include "serialcon/serialcon.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  arguments_t args;

  process_commandline(argc, argv, &args);
  if (!args.flag_username)
    args.username = getenv("USER");
  if (!args.flag_baudrate)
    args.baudrate = "115200";
  if (!args.flag_password) {
      printf("error: You have to provide a password per command-line option\n");
      usage(argv[0]);
      exit(1);
  }
  if (!args.flag_serial_device) {
    printf("error: You have to provide the device name\n");
    usage(argv[0]);
    exit(1);
  }

  serialcon_connection *conn = serialcon_Open(
      args.serial_device, atoi(args.baudrate), args.username, args.password);
  int rc = serialcon_Run(conn, "ls -l; pwd; hostname;\n");
  switch (rc) {
  case 0:
    printf("Command was successfull\n");
    break;
  default:
    printf("error: Command failed: code %d\n", rc);
    break;
  }
  serialcon_Close(conn);

  return 0;
}
