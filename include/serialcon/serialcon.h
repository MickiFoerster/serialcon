#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  const char *serial_dev;
  uint32_t baudrate;
  const char *username;
  const char *password;
  int fd;
  char exitsignal[8];
  int exitcode;
} serialcon_connection;

serialcon_connection *serialcon_Open(const char *serial_dev, uint32_t baudrate,
                                     const char *user, const char *password);
void serialcon_Close(serialcon_connection *conn);
int serialcon_Run(serialcon_connection *conn, const char *cmd);
bool serialcon_Upload(serialcon_connection *conn, const char *source_file_path,
                      const char *target_file_path);
