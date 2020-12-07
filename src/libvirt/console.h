#pragma once
#include <pthread.h>

typedef struct {
  const char *vmname;
  const char *username;
  const char *password;
  int pidChildProcess;
  int fdParentProcess;
  pthread_t threadParentStatemachine;
  int stopThreadParentStatemachine;
  void *lexer_instance;
} callVirshConsole_args;

int callVirshConsole(callVirshConsole_args *argv);
int libvirt_console_open(callVirshConsole_args *argv);
