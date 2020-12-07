#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 600
#define _XOPEN_SOURCE 600
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands.h"
#include "console.h"
#include "lexer.h"
#include "statemachine.h"

#define RETURN_CODE_ERROR -1
#define RETURN_CODE_OK 0

enum {
  TOKEN_UNDEFINED = 0,
  TOKEN_CONSOLE_READY,
  TOKEN_LOGIN,
  TOKEN_PASSWORD,
  TOKEN_PROMPT,
};

static void *statemachine_thread(void *args) {
  callVirshConsole_args *argv = (callVirshConsole_args *)args;
  statemachine_arg_t statemachine_args = {
      .fd = argv->fdParentProcess,
      .username = argv->username,
      .password = argv->password,
  };
  statemachine_init(&statemachine_args);
  argv->lexer_instance = lexer_init(argv->fdParentProcess, 5000);
  int token;
  state_e new_state;
  for (; argv->stopThreadParentStatemachine == 0;) {
    do {
      fprintf(stderr, "DEBUG: statemachine thread:\n");
      if (argv->stopThreadParentStatemachine != 0)
        break;
      token = lexer(argv->lexer_instance);
      // fprintf(stderr, "lexer returned %d\n", token);
    } while (token == -1);
    new_state = statemachine_next(token);
    if (new_state == STATE_EXIT || new_state == STATE_LOGIN_FAILED ||
        new_state == STATE_COMMAND_EXECUTION_FAILED) {
      break;
    }
  }

  argv->stopThreadParentStatemachine = 2; // signal that thread exits
  return NULL;
}

int libvirt_console_open(callVirshConsole_args *argv) {
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
  }
  // parent
  argv->fdParentProcess = masterFd;
  argv->pidChildProcess = pid;

  argv->stopThreadParentStatemachine = 0;
  pthread_t tid;
  rc = pthread_create(&tid, NULL, statemachine_thread, argv);
  if (rc != 0) {
    perror("pthread_create failed");
    return RETURN_CODE_ERROR;
  }

  rc = pthread_detach(tid);
  if (rc != 0) {
    perror("pthread_detach failed");
    return RETURN_CODE_ERROR;
  }

  argv->threadParentStatemachine = tid;

  return RETURN_CODE_OK;
}

void libvirt_console_run(callVirshConsole_args *argv, const char *cmd) {
  if (!add_command(cmd)) {
    fprintf(stderr, "error: could not add command '%s'\n", cmd);
    exit(EXIT_FAILURE);
  }
}

void libvirt_console_close(callVirshConsole_args *argv) {
  argv->stopThreadParentStatemachine = 1;
  while (argv->stopThreadParentStatemachine != 2) // wait for thread's exit
    ;
  lexer_finish(argv->lexer_instance);
  waitpid(argv->pidChildProcess, NULL, 0);
}

int callVirshConsole(callVirshConsole_args *argv) {
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