#pragma once

#include <stdbool.h>

typedef struct {
  bool flag_baudrate;
  const char *baudrate;
  bool flag_help;
  bool flag_password;
  const char *password;
  bool flag_serial_device;
  const char *serial_device;
  bool flag_username;
  const char *username;
  bool flag_verbose;
} arguments_t;

int process_commandline(int argc, char **argv, arguments_t *args);
void usage(const char *progname);
