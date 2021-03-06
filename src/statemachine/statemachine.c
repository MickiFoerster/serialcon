#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lexer.h"
#include "serialcon/serialcon.h"

typedef enum {
  KEYWORD_UNDEFINED = 0,
  KEYWORD_LOGIN_PROMPT,
  KEYWORD_LOGIN_INCORRECT,
  KEYWORD_PASSWORD_PROMPT,
  KEYWORD_PASSWORD_SUDO_PROMPT,
  KEYWORD_USER_PROMPT,
  KEYWORD_ROOT_PROMPT,
  KEYWORD_LIBVIRT_CONSOLE_MSG,
  KEYWORD_COMMAND_EXECUTION_OK,
  KEYWORD_COMMAND_EXECUTION_FAILED,
} keyword_e;

typedef struct {
  char word[64];
  keyword_e id;
} keyword_t;

typedef struct {
  state_e from;
  state_e to;
  keyword_e keyword;
  void *(*action)(void *);
} transition_t;

static void *onLOGIN_PROMPT_FOUND(void *argv);
static void *onPASSWORD_PROMPT_FOUND(void *argv);
static void *onUSER_PROMPT_FOUND(void *argv);
static void *onROOT_PROMPT_FOUND(void *argv);
static state_e check_transition(serialcon_connection *conn,
                                state_e current_state, int token);
static keyword_t keywords[] = {
    {
        .word = " login: ",
        .id = KEYWORD_LOGIN_PROMPT,
    },
    {
        .word = "\nPassword: ",
        .id = KEYWORD_PASSWORD_PROMPT,
    },
    {
        .word = "[sudo] password for ",
        .id = KEYWORD_PASSWORD_SUDO_PROMPT,
    },
    {
        .word = "$ ",
        .id = KEYWORD_USER_PROMPT,
    },
    {
        .word = "# ",
        .id = KEYWORD_ROOT_PROMPT,
    },
    {
        .word = "Escape character is ^]",
        .id = KEYWORD_LIBVIRT_CONSOLE_MSG,
    },
    {
        .word = "\nCMD-EXECUTION-RESULT-OK",
        .id = KEYWORD_COMMAND_EXECUTION_OK,
    },
    {
        .word = "\nCMD-EXECUTION-RESULT-FAILED\r\n",
        .id = KEYWORD_COMMAND_EXECUTION_FAILED,
    },
    {
        .word = "\nLogin incorrect\r\n",
        .id = KEYWORD_LOGIN_INCORRECT,
    },
};

void statemachine_init() {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        keyword_t *kw = &keywords[i];
        patterns_push_back(kw->word, kw->id);
    }
}

state_e statemachine_next(serialcon_connection *conn, int token) {
    static state_e current_state = STATE_UNDEFINED;
    conn->reboot = false;
    conn->finished = false;
    switch (current_state) {
        case STATE_UNDEFINED:
            switch (token) {
                case -1: {
                    const char str[] = "exit\n";
                    write(conn->fd, str, strlen(str));
                } break;
                case KEYWORD_LIBVIRT_CONSOLE_MSG:
                    write(conn->fd, "\n", 1);
                    break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_REBOOT:
            switch (token) {
                case -1: {
                    const char str[] = "\n";
                    write(conn->fd, str, strlen(str));
                } break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_ERROR:
            fprintf(stderr, "error state reached\n");
            break;
        case STATE_LOGIN_PROMPT_FOUND:
            switch (token) {
                case -1:
                    fprintf(stderr, "timeout in STATE_LOGIN_PROMPT_FOUND\n");
                    break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_PASSWORD_PROMPT_FOUND:
        case STATE_PASSWORD_SUDO_PROMPT:
            switch (token) {
                case -1:
                    fprintf(stderr, "timeout in STATE_PASSWORD_PROMPT_FOUND\n");
                    break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_USER_PROMPT_FOUND:
            switch (token) {
                case -1:
                    fprintf(stderr, "timeout in STATE_USER_PROMPT_FOUND\n");
                    break;
                case KEYWORD_COMMAND_EXECUTION_OK:
                    break;
                case KEYWORD_COMMAND_EXECUTION_FAILED:
                    current_state = STATE_COMMAND_EXECUTION_FAILED;
                    break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_ROOT_PROMPT_FOUND:
            switch (token) {
                case -1:
                    fprintf(stderr, "timeout in STATE_ROOT_PROMPT_FOUND\n");
                    break;
                default:
                    current_state =
                        check_transition(conn, current_state, token);
                    break;
            }
            break;
        case STATE_EXIT:
            assert(0 && "state exit may not happen");
            break;
        default:
            fprintf(stderr, "unexpected current state %d\n", current_state);
            assert(0 && "unexpected current state");
            break;
    }

    return current_state;
}

static void *onLOGIN_FAILED(void *argv) {
  fprintf(stderr, "error: Could not login with given password.\n");
  return NULL;
}

static void *onLOGIN_PROMPT_FOUND(void *argv) {
    serialcon_connection *args = (serialcon_connection *)argv;
    write(args->fd, args->username, strlen(args->username));
    write(args->fd, "\n", 1);
    return NULL;
}

static void *onPASSWORD_PROMPT_FOUND(void *argv) {
    serialcon_connection *args = (serialcon_connection *)argv;
    write(args->fd, args->password, strlen(args->password));
    write(args->fd, "\n", 1);
    return NULL;
}

static void *onROOT_PROMPT_FOUND(void *argv) {
  return NULL;
}

static void *onUSER_PROMPT_FOUND(void *argv) {
    serialcon_connection *args = (serialcon_connection *)argv;

    typedef enum {
        SUBSTATE_COMMAND_EXECUTION_UNDEFINED = 0,
        SUBSTATE_COMMAND_EXECUTION_ONGOING = 100,
    } substate_e;

    static substate_e substate = SUBSTATE_COMMAND_EXECUTION_UNDEFINED;
    switch (substate) {
        case SUBSTATE_COMMAND_EXECUTION_UNDEFINED: {
            command_t *cmd = next_cmd(args);
            if (!cmd) {
                const char logout_cmd[] = "exit\n";
                fprintf(stderr, "%s:%u: no command found\n", __FILE__,
                        __LINE__);
                write(args->fd, logout_cmd, strlen(logout_cmd));
                args->finished = true;
                break;
            } else if (strstr(cmd->cmdline, "sudo reboot") != NULL) {
                args->reboot = true;
            }
            fprintf(stderr, "%s:%u: command %s found\n", __FILE__, __LINE__,
                    cmd->cmdline);
            write(args->fd, cmd->cmdline, strlen(cmd->cmdline));
            substate = SUBSTATE_COMMAND_EXECUTION_ONGOING;
            break;
        }
        case SUBSTATE_COMMAND_EXECUTION_ONGOING: {
            char buffer[4096];
            snprintf(buffer, sizeof(buffer),
                     "if [[ $? -eq 0 ]]; then echo CMD-EXECUTION-RESULT-OK; "
                     "else echo "
                     "CMD-EXECUTION-RESULT-FAILED;fi \n");
            substate = SUBSTATE_COMMAND_EXECUTION_UNDEFINED;
            write(args->fd, buffer, strlen(buffer));
            break;
        }
  }

  return argv;
}

static transition_t transitions[] = {
    {
        .from = STATE_UNDEFINED,
        .to = STATE_LOGIN_PROMPT_FOUND,
        .keyword = KEYWORD_LOGIN_PROMPT,
        .action = onLOGIN_PROMPT_FOUND,
    },
    {
        .from = STATE_REBOOT,
        .to = STATE_LOGIN_PROMPT_FOUND,
        .keyword = KEYWORD_LOGIN_PROMPT,
        .action = onLOGIN_PROMPT_FOUND,
    },
    {
        .from = STATE_LOGIN_PROMPT_FOUND,
        .to = STATE_PASSWORD_PROMPT_FOUND,
        .keyword = KEYWORD_PASSWORD_PROMPT,
        .action = onPASSWORD_PROMPT_FOUND,
    },
    {
        .from = STATE_PASSWORD_PROMPT_FOUND,
        .to = STATE_LOGIN_FAILED,
        .keyword = KEYWORD_LOGIN_INCORRECT,
        .action = onLOGIN_FAILED,
    },
    {
        .from = STATE_USER_PROMPT_FOUND,
        .to = STATE_PASSWORD_SUDO_PROMPT,
        .keyword = KEYWORD_PASSWORD_SUDO_PROMPT,
        .action = onPASSWORD_PROMPT_FOUND,
    },
    {
        .from = STATE_PASSWORD_SUDO_PROMPT,
        .to = STATE_USER_PROMPT_FOUND,
        .keyword = KEYWORD_USER_PROMPT,
        .action = onUSER_PROMPT_FOUND,
    },
    {
        .from = STATE_PASSWORD_PROMPT_FOUND,
        .to = STATE_USER_PROMPT_FOUND,
        .keyword = KEYWORD_USER_PROMPT,
        .action = onUSER_PROMPT_FOUND,
    },
    {
        .from = STATE_UNDEFINED,
        .to = STATE_USER_PROMPT_FOUND,
        .keyword = KEYWORD_USER_PROMPT,
        .action = onUSER_PROMPT_FOUND,
    },
    {
        .from = STATE_PASSWORD_PROMPT_FOUND,
        .to = STATE_ROOT_PROMPT_FOUND,
        .keyword = KEYWORD_ROOT_PROMPT,
        .action = onROOT_PROMPT_FOUND,
    },
    {
        .from = STATE_USER_PROMPT_FOUND,
        .to = STATE_USER_PROMPT_FOUND,
        .keyword = KEYWORD_USER_PROMPT,
        .action = onUSER_PROMPT_FOUND,
    },
};

static state_e check_transition(serialcon_connection *conn, state_e current_state,
                                int token) {
  for (size_t i = 0; i < sizeof(transitions) / sizeof(transitions[0]); ++i) {
    transition_t *t = &transitions[i];
    if (t->from == current_state && t->keyword == token) {
      current_state = t->to;
      t->action(conn);
      if (conn->reboot) {
          return STATE_REBOOT;
      } else if (conn->finished) {
          return STATE_EXIT;
      }
      return t->to;
    }
  }
  return current_state;
}

const char *statemachine_getName(state_e state) {
    const char *p = NULL;
    switch (state) {
        case STATE_UNDEFINED:
            return "STATE_UNDEFINED";
        case STATE_ERROR:
            return "STATE_ERROR";
        case STATE_LOGIN_PROMPT_FOUND:
            return "STATE_LOGIN_PROMPT_FOUND";
        case STATE_LOGIN_FAILED:
            return "STATE_LOGIN_FAILED";
        case STATE_PASSWORD_PROMPT_FOUND:
            return "STATE_PASSWORD_PROMPT_FOUND";
        case STATE_PASSWORD_SUDO_PROMPT:
            return "STATE_PASSWORD_SUDO_PROMPT";
        case STATE_USER_PROMPT_FOUND:
            return "STATE_USER_PROMPT_FOUND";
        case STATE_ROOT_PROMPT_FOUND:
            return "STATE_ROOT_PROMPT_FOUND";
        case STATE_EXIT:
            return "STATE_EXIT";
        case STATE_COMMAND_EXECUTION_FAILED:
            return "STATE_COMMAND_EXECUTION_FAILED";
        case STATE_COMMAND_EXECUTION_OK:
            return "STATE_COMMAND_EXECUTION_OK";
        case STATE_REBOOT:
            return "STATE_REBOOT";
    }
    return p;
}

