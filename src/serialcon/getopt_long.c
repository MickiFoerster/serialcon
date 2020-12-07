#include "command-line.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(const char *progname) {
  char help_msg[] = "\n\
[-b|--baudrate] [-h|--help] [-p|--password] [-s|--serial-device] \n\
[-u|--username] [-v|--verbose] \n\
\n\
This is description text\n\
\n\
Options :\n\
  -b, --baudrate      Baudrate to use for serial connection\n\
  -h, --help          Prints this help message\n\
  -p, --password      Password used at password prompt\n\
  -s, --serial-device Serial device e.g. /dev/ttyUSB0 or name of libvirt domain e.g. /libvirt/1 (see virsh list)\n\
  -u, --username      User name used at login prompt\n\
  -v, --verbose       Verbose output\n\
";
  printf("Usage: %s\n%s", progname, help_msg);
}

int process_commandline(int argc, char **argv, arguments_t *args) {
  struct option long_options[] = {{"baudrate", required_argument, 0, 'b'},
                                  {"help", no_argument, 0, 'h'},
                                  {"password", required_argument, 0, 'p'},
                                  {"serial-device", required_argument, 0, 's'},
                                  {"username", required_argument, 0, 'u'},
                                  {"verbose", no_argument, 0, 'v'},
                                  {0, 0, 0, 0}};
  int c;

  memset(args, 0, sizeof(arguments_t));
  while (1) {
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long(argc, argv, "b:hp:s:u:v", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      break;

    case 'b':
      args->flag_baudrate = true;
      args->baudrate = optarg;
      break;

    case 'h':
      args->flag_help = true;
      usage(argv[0]);
      exit(0);
      break;

    case 'p':
      args->flag_password = true;
      args->password = optarg;
      break;

    case 's':
      args->flag_serial_device = true;
      args->serial_device = optarg;
      break;

    case 'u':
      args->flag_username = true;
      args->username = optarg;
      break;

    case 'v':
      args->flag_verbose = true;

      break;

    case '?':
      /* getopt_long already printed an error message. */
      break;
    default:
      fprintf(stderr, "unhandled case\n");
      abort();
    }
  }

  return 0;
}

