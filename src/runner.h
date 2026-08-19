#ifndef RUNNER_H
#define RUNNER_H

#include <windows.h> // HANDLE type used in the struct below

// tracks one running project process
typedef struct {
    char   name[256];   // project name — used to match browser requests
    HANDLE process;     // Windows process handle — used to kill it
    HANDLE pipe_read;   // read end of pipe — where we get the output from
    int    running;     // 1 = currently running, 0 = empty slot
} RunningProcess;

#define MAX_RUNNING 20  // maximum simultaneous running projects

// starts a project — returns 0=ok, -1=failed, -2=db port not found
int  runner_start(const char *name, const char *path, const char *command, int db_port);

// stops a running project by name — returns 1=ok, 0=not found
int  runner_stop(const char *name);

// reads any new output from the project's pipe into out buffer
void runner_get_output(const char *name, char *out, int out_size, int *still_running);

#endif