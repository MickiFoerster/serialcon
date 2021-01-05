#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct command_s command_t;

struct command_s {
    const char *cmdline;
    int exitcode;
};

typedef struct list_of_cmds_s list_of_cmds_t;
struct list_of_cmds_s {
    command_t cmd;
    list_of_cmds_t *next;
};

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
    list_of_cmds_t cmd_list;
    list_of_cmds_t cmd_list_done;
    char exitsignal[8];
    int exitcode;
    bool reboot;
    bool finished;
} serialcon_connection;

serialcon_connection *serialcon_Open(const char *serial_dev, uint32_t baudrate,
                                     const char *user, const char *password);
void serialcon_Close(serialcon_connection *conn);
int serialcon_Run(serialcon_connection *conn, const char *cmd);
bool serialcon_Upload(serialcon_connection *conn, const char *source_file_path,
                      const char *target_file_path);

bool add_command(serialcon_connection *conn, const char *cmd);
void free_cmd_list(serialcon_connection *conn);
command_t *next_cmd(serialcon_connection *conn);
command_t *failed_cmd(serialcon_connection *conn);

void statemachine_init();
state_e statemachine_next(serialcon_connection *conn, int token);
const char *statemachine_getName(state_e state);

