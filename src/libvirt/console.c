#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 600
#define _XOPEN_SOURCE 600
#endif

#include "console.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lexer.h"
#include "serialcon/serialcon.h"

#define RETURN_CODE_ERROR -1
#define RETURN_CODE_OK 0

enum {
  TOKEN_UNDEFINED = 0,
  TOKEN_CONSOLE_READY,
  TOKEN_LOGIN,
  TOKEN_PASSWORD,
  TOKEN_PROMPT,
};

static void *thread_wait_for_child_process(void *argv) {
    int status;
    serialcon_connection *conn = (serialcon_connection *)argv;

    waitpid(conn->pidChildProcess, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        printf("virsh console command exited with code %d\n", exit_status);
        if (exit_status) exit(exit_status);
    }
    fprintf(stderr, "thread that waits for end of child process terminates\n");

    return NULL;
}

static void *statemachine_thread(void *argv) {
    serialcon_connection *conn = (serialcon_connection *)argv;
    statemachine_init();
    conn->lexer_instance = lexer_init(conn->fd, 5000);
    int token;
    fprintf(stderr, "DEBUG: before loop %d\n",
            conn->stopThreadParentStatemachine);
    for (; conn->stopThreadParentStatemachine == 0;) {
        fprintf(stderr, "DEBUG: inner loop of statemachine thread:\n");
        do {
            if (conn->stopThreadParentStatemachine != 0) break;
            token = lexer(conn->lexer_instance);
            fprintf(stderr, "lexer returned token '%s'\n",
                    patterns_get_pattern_from_ID(token));
        } while (token == -1);

        pthread_mutex_lock(&conn->mtx_new_state);
        {
            conn->new_state = statemachine_next(conn, token);
            pthread_cond_signal(&conn->new_state_available);
        }
        pthread_mutex_unlock(&conn->mtx_new_state);
    }

    fprintf(stderr, "statemachine_thread finishs now\n");
    conn->stopThreadParentStatemachine = 2;  // signal that thread exits
    return NULL;
}

int libvirt_console_open(serialcon_connection *conn) {
    int rc;
    int masterFd = open("/dev/ptmx", O_RDWR | O_NOCTTY);  // Open pty master
    if (masterFd == -1) {
        perror("open(/dev/ptmx) failed");
        exit(1);
    }

    if (grantpt(masterFd) == -1) {  // Grant access to slave pty
        perror("grantpt() failed");
        close(masterFd);
        exit(1);
    }

    if (unlockpt(masterFd) == -1) {  // Unlock slave pty
        perror("unlockpt() failed");
        close(masterFd);
        exit(1);
    }

    char *slaveName = ptsname(masterFd);  // Get slave pty name
    if (slaveName == NULL) {
        perror("ptsname() failed");
        close(masterFd);
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {  // fork failed
        perror("fork failed");
        close(masterFd);
        return RETURN_CODE_ERROR;
    } else if (pid == 0) {     // child process
        if (setsid() == -1) {  // start new session
            perror("setsid() failed");
            close(masterFd);
            exit(1);
        }

        close(masterFd);  // not needed in child

        int slaveFd = open(slaveName, O_RDWR);  // Becomes controlling tty
        if (slaveFd == -1) {
            perror("open(slavePty) failed");
            close(masterFd);
            exit(1);
        }

        if (dup2(slaveFd, STDIN_FILENO) != STDIN_FILENO ||
            dup2(slaveFd, STDOUT_FILENO) != STDOUT_FILENO ||
            dup2(slaveFd, STDERR_FILENO) != STDERR_FILENO) {
            perror("Redirecting standard I/O devices failed");
            close(masterFd);
            exit(1);
        }

        if (slaveFd > STDERR_FILENO) {
            close(slaveFd);  // No longer needed file descriptor
        }

        const char path[] = "/usr/bin/virsh";
        char *const args[] = {"/usr/bin/virsh", "console",
                              is_conn_libvirt(conn->serial_dev), NULL};

        rc = execv(path, args);
        if (rc < 0) {
            fprintf(stderr, "execv failed: %s\n", strerror(errno));
            return RETURN_CODE_ERROR;
        }
        assert(0 && "Here execution MAY never arrive");
    }
    // parent
    conn->fd = masterFd;
    conn->pidChildProcess = pid;
    conn->stopThreadParentStatemachine = 0;
    pthread_mutex_init(&conn->mtx_new_state, NULL);
    pthread_cond_init(&conn->new_state_available, NULL);

    pthread_t tid;
    rc = pthread_create(&tid, NULL, statemachine_thread, conn);
    if (rc != 0) {
        perror("pthread_create failed");
        return RETURN_CODE_ERROR;
    }

    rc = pthread_detach(tid);
    if (rc != 0) {
        perror("pthread_detach failed");
        return RETURN_CODE_ERROR;
    }

    conn->threadParentStatemachine = tid;

    rc = pthread_create(&tid, NULL, thread_wait_for_child_process, conn);
    if (rc != 0) {
        perror("pthread_create failed");
        return RETURN_CODE_ERROR;
    }

    rc = pthread_detach(tid);
    if (rc != 0) {
        perror("pthread_detach failed");
        return RETURN_CODE_ERROR;
    }

    return RETURN_CODE_OK;
}

int libvirt_console_run(serialcon_connection *conn, const char *cmd) {
    bool stop = false;
    for (; !stop;) {
        pthread_mutex_lock(&conn->mtx_new_state);
        pthread_cond_wait(&conn->new_state_available, &conn->mtx_new_state);
        fprintf(stderr, "statemachine has new state: %s\n",
                statemachine_getName(conn->new_state));
        if (conn->new_state == STATE_EXIT ||
            conn->new_state == STATE_LOGIN_FAILED ||
            conn->new_state == STATE_COMMAND_EXECUTION_FAILED ||
            conn->new_state == STATE_COMMAND_EXECUTION_OK) {
            fprintf(stderr, "RUN finished\n");
            stop = true;
        }
        pthread_mutex_unlock(&conn->mtx_new_state);
    }

    return 0;
}

void libvirt_console_close(serialcon_connection *conn) {
    conn->stopThreadParentStatemachine = 1;
    while (conn->stopThreadParentStatemachine != 2)  // wait for thread's exit
        ;
    lexer_finish(conn->lexer_instance);
    waitpid(conn->pidChildProcess, NULL, 0);
}

char *is_conn_libvirt(const char *serial_dev) {
    const char libvirt_prefix[] = "/libvirt/";

    if (strstr(serial_dev, libvirt_prefix) == serial_dev) {
        serial_dev += strlen(libvirt_prefix);
        return (char *)serial_dev;
    }

    return NULL;
}

#if 0
int callVirshConsole(serialcon_connection *conn) {
  int rc;
  int masterFd = open("/dev/ptmx", O_RDWR | O_NOCTTY); // Open pty master
  if (masterFd == -1) {
    perror("open(/dev/ptmx) failed");
    exit(1);
  }

  if (grantpt(masterFd) == -1) { // Grant access to slave pty
    perror("grantpt() failed");
    close(masterFd);
    exit(1);
  }

  if (unlockpt(masterFd) == -1) { // Unlock slave pty
    perror("unlockpt() failed");
    close(masterFd);
    exit(1);
  }

  char *slaveName = ptsname(masterFd); // Get slave pty name
  if (slaveName == NULL) {
    perror("ptsname() failed");
    close(masterFd);
    exit(1);
  }

  pid_t pid = fork();
  if (pid == -1) { // fork failed
    perror("fork failed");
    close(masterFd);
    return RETURN_CODE_ERROR;
  } else if (pid == 0) {  // child process
    if (setsid() == -1) { // start new session
      perror("setsid() failed");
      close(masterFd);
      exit(1);
    }

    close(masterFd); // not needed in child

    int slaveFd = open(slaveName, O_RDWR); // Becomes controlling tty
    if (slaveFd == -1) {
      perror("open(slavePty) failed");
      close(masterFd);
      exit(1);
    }

    if (dup2(slaveFd, STDIN_FILENO) != STDIN_FILENO ||
        dup2(slaveFd, STDOUT_FILENO) != STDOUT_FILENO ||
        dup2(slaveFd, STDERR_FILENO) != STDERR_FILENO) {
      perror("Redirecting standard I/O devices failed");
      close(masterFd);
      exit(1);
    }

    if (slaveFd > STDERR_FILENO) {
      close(slaveFd); // No longer needed file descriptor
    }

    const char path[] = "/usr/bin/virsh";
    char *const args[] = {"/usr/bin/virsh", "console", (char *)argv->vmname,
                          NULL};

    rc = execv(path, args);
    if (rc < 0) {
      fprintf(stderr, "execv failed: %s\n", strerror(errno));
      return RETURN_CODE_ERROR;
    }
    assert(0 && "Here execution MAY never arrive");
  } else { // parent
    // Now parent process reads from master pty
    statemachine_arg_t statemachine_args = {
        .fd = masterFd,
        .username = argv->username,
        .password = argv->password,
    };
    statemachine_init(&statemachine_args);
    void *lexer_instance = lexer_init(masterFd, 5000);
    int token;
    state_e new_state;
    for (;;) {
      do {
        token = lexer(lexer_instance);
        // fprintf(stderr, "lexer returned %d\n", token);
      } while (token == -1);
      new_state = statemachine_next(token);
      if (new_state == STATE_EXIT || new_state == STATE_LOGIN_FAILED ||
          new_state == STATE_COMMAND_EXECUTION_FAILED) {
        break;
      }
    }

    lexer_finish(lexer_instance);
    waitpid(pid, NULL, 0);

    return (new_state == STATE_EXIT) ? 0 : 1;
  }

  assert(0 && "Here execution MAY never arrive");
  return RETURN_CODE_OK;
}
#endif
