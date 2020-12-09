#pragma once
#include <stdbool.h>
#include "serialcon/serialcon.h"

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
    STATE_COMMAND_EXECUTION_OK,
    STATE_REBOOT,
} state_e;

void statemachine_init(serialcon_connection *conn);
state_e statemachine_next(serialcon_connection *conn, int token);
