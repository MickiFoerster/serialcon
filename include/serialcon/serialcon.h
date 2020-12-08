#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "serialcon/statemachine.h"

typedef struct {
  const char *serial_dev;
  uint32_t baudrate;
  const char *username;
  const char *password;
  int fd;
  int pidChildProcess;
  pthread_t threadParentStatemachine;
  state_e new_state;
  pthread_mutex_t mtx_new_state;
  pthread_cond_t new_state_available;
  int stopThreadParentStatemachine;
  void *lexer_instance;
  char exitsignal[8];
  int exitcode;
} serialcon_connection;

serialcon_connection *serialcon_Open(const char *serial_dev, uint32_t baudrate,
                                     const char *user, const char *password);
void serialcon_Close(serialcon_connection *conn);
int serialcon_Run(serialcon_connection *conn, const char *cmd);
bool serialcon_Upload(serialcon_connection *conn, const char *source_file_path,
                      const char *target_file_path);
