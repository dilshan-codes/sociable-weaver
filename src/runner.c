#include <winsock2.h>  // check_port uses sockets — must come before windows.h
#include <windows.h>   // CreateProcess, HANDLE, STARTUPINFO, etc.
#include <stdio.h>
#include <string.h>
#include "runner.h"

// global process table — shared across all requests
// static = only visible inside this file, not accessible from other files
static RunningProcess process_table[MAX_RUNNING];
static int table_ready = 0; // tracks whether we have zeroed the table yet

// ── init_table ─────────────────────────────────────────────────────────────
// zeroes the process table on first use
// called at the start of runner_start() and runner_stop()
static void init_table() {
    if (!table_ready) {
        memset(process_table, 0, sizeof(process_table));
        table_ready = 1;
    }
}

// ── find_slot ──────────────────────────────────────────────────────────────
// searches the process table for a slot with the given name
// returns the index if found, -1 if not found
static int find_slot(const char *name) {
    for (int i = 0; i < MAX_RUNNING; i++) {
        if (process_table[i].running &&
            strcmp(process_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // not found
}

// ── find_empty ─────────────────────────────────────────────────────────────
// finds an empty (not running) slot in the process table
// returns the index, or -1 if the table is full
static int find_empty() {
    for (int i = 0; i < MAX_RUNNING; i++) {
        if (!process_table[i].running) return i;
    }
    return -1;
}

// ── check_port ─────────────────────────────────────────────────────────────
// tries to connect to localhost on the given port
// returns 1 if something is listening, 0 if nothing is there
// port <= 0 means no database required — always returns 1
static int check_port(int port) {
    if (port <= 0) return 1; // no db required — skip the check

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return 0;

    // set 1 second timeout so we do not wait forever
    DWORD timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(sock);
    return result == 0; // 0 = connected = port is alive
}

// ── runner_start ───────────────────────────────────────────────────────────
// spawns a project as a child process with its stdout piped back to us
// returns: 0=success, -1=failed to start, -2=db port not detected
int runner_start(const char *name, const char *path, const char *command, int db_port) {
    init_table();

    // check database port before doing anything else
    if (!check_port(db_port)) return -2;

    // do not start if already running
    if (find_slot(name) >= 0) return 0;

    int slot = find_empty();
    if (slot < 0) return -1; // table is full

    // ── create the pipe ────────────────────────────────────────────────────
    // SECURITY_ATTRIBUTES with bInheritHandle = TRUE means the child process
    // can inherit the write end of the pipe as its stdout
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE pipe_read, pipe_write;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0)) return -1;
    // pipe_read  = parent reads from here to get the child's output
    // pipe_write = child writes its stdout here

    // make the read end NOT inheritable — only the child needs the write end
    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    // ── set up STARTUPINFO ────────────────────────────────────────────────
    // STARTUPINFO tells CreateProcess how to configure the new process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = pipe_write; // redirect child's stdout to our pipe
    si.hStdError  = pipe_write; // redirect child's stderr too — capture errors
    si.dwFlags    = STARTF_USESTDHANDLES; // tells Windows to use our handles above
    // without STARTF_USESTDHANDLES, the hStdOutput field is ignored

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // command must be a mutable char array — CreateProcess may modify it
    char cmd_copy[512];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);

    // ── CreateProcess ──────────────────────────────────────────────────────
    // this is the Windows equivalent of fork() + exec()
    // it creates a brand new process running the given command
    // Arguments:
    // NULL          = no explicit executable path — find it from cmd_copy
    // cmd_copy      = the command line e.g. "python main.py"
    // NULL, NULL    = default security for process and thread
    // TRUE          = inherit handles — child gets our pipe_write handle
    // 0             = no special creation flags
    // NULL          = inherit parent's environment variables
    // path          = working directory for the child process
    // &si           = startup configuration including our pipe handles
    // &pi           = receives the new process's handles and IDs
    BOOL ok = CreateProcess(
        NULL, cmd_copy, NULL, NULL,
        TRUE, 0, NULL, path, &si, &pi
    );

    CloseHandle(pipe_write); // parent does not write — close our copy of write end
    // if we do not close this, ReadFile on pipe_read will never return EOF

    if (!ok) {
        CloseHandle(pipe_read);
        return -1; // CreateProcess failed — command not found or bad path
    }

    CloseHandle(pi.hThread); // we do not need the thread handle

    // store everything in the process table
    strncpy(process_table[slot].name, name, 255);
    process_table[slot].process   = pi.hProcess;
    process_table[slot].pipe_read = pipe_read;
    process_table[slot].running   = 1;

    return 0; // success
}

// ── runner_get_output ──────────────────────────────────────────────────────
// reads any available output from the project's pipe without blocking
// also checks if the process has exited and updates running flag
// Parameters:
// name          = which project to read from
// out           = buffer to write output into
// out_size      = size of out buffer
// still_running = set to 1 if process is still running, 0 if it has exited
void runner_get_output(const char *name, char *out, int out_size, int *still_running) {
    out[0] = '\0';
    *still_running = 0;

    int slot = find_slot(name);
    if (slot < 0) return; // project not found in table

    // check if process has exited
    DWORD exit_code;
    GetExitCodeProcess(process_table[slot].process, &exit_code);
    if (exit_code != STILL_ACTIVE) {
        // process finished — read any remaining output then mark as stopped
        process_table[slot].running = 0;
        *still_running = 0;
    } else {
        *still_running = 1;
    }

    // PeekNamedPipe checks how many bytes are available WITHOUT reading them
    // this lets us read only what is there without blocking if nothing is ready
    DWORD available = 0;
    PeekNamedPipe(process_table[slot].pipe_read, NULL, 0, NULL, &available, NULL);

    if (available > 0) {
        // cap how much we read to our buffer size
        DWORD to_read = available < (DWORD)(out_size - 1) ? available : (DWORD)(out_size - 1);
        DWORD bytes_read = 0;
        ReadFile(process_table[slot].pipe_read, out, to_read, &bytes_read, NULL);
        out[bytes_read] = '\0'; // null terminate the output string
    }
}

// ── runner_stop ────────────────────────────────────────────────────────────
// kills a running project process and frees its slot in the table
// returns 1 if stopped, 0 if not found
int runner_stop(const char *name) {
    init_table();
    int slot = find_slot(name);
    if (slot < 0) return 0; // not running — nothing to stop

    TerminateProcess(process_table[slot].process, 0);
    // TerminateProcess forcefully kills the process immediately
    // argument 0 = exit code to give the process

    CloseHandle(process_table[slot].process);  // release process handle
    CloseHandle(process_table[slot].pipe_read); // release pipe handle

    // clear the slot so it can be reused
    memset(&process_table[slot], 0, sizeof(RunningProcess));
    return 1;
}