#pragma once
#include <stdbool.h>

typedef enum {
  STATE_UNDEFINED = 0,
  STATE_ERROR,
  STATE_LOGIN_PROMPT_FOUND,
  STATE_LOGIN_FAILED,
  STATE_PASSWORD_PROMPT_FOUND,
  STATE_PASSWORD_SUDO_PROMPT,
  STATE_USER_PROMPT_FOUND,
  STATE_ROOT_PROMPT_FOUND,
  STATE_EXIT,
  STATE_COMMAND_EXECUTION_FAILED,
  STATE_REBOOT,
} state_e;

typedef struct {
  int fd;
  const char *username;
  const char *password;
  bool reboot;
  bool finished;
} statemachine_arg_t;

void statemachine_init(statemachine_arg_t *argv);
state_e statemachine_next(int token);
