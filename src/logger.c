#include "logger.h"
#include "syscalls.h"
#include "string_utils.h"

static int log_pipe[2] = {-1, -1};
static pid_t logger_pid = -1;

void logger_init(void) {
    if (sys_pipe(log_pipe) < 0) return;

    logger_pid = sys_fork();
    if (logger_pid == 0) {
        // Child: Background Logger Process
        sys_close(log_pipe[1]); // Close write end
        
        int log_fd = sys_open("shell.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd < 0) sys_exit(1);
        
        // Epoll-based async listening inside the logger
        int epfd = sys_epoll_create1(0);
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = log_pipe[0];
        sys_epoll_ctl(epfd, EPOLL_CTL_ADD, log_pipe[0], &ev);
        
        char buf[256];
        struct epoll_event events[1];
        while (1) {
            int n = sys_epoll_wait(epfd, events, 1, -1);
            if (n > 0) {
                ssize_t bytes = sys_read(log_pipe[0], buf, sizeof(buf) - 1);
                if (bytes <= 0) break; // Pipe closed by parent shell
                
                sys_write(log_fd, "[ASYNC LOG] ", 12);
                sys_write(log_fd, buf, bytes);
                sys_write(log_fd, "\n", 1);
            }
        }
        sys_close(log_fd);
        sys_exit(0);
    }
    // Parent: Main Shell Process
    sys_close(log_pipe[0]); // Close read end
}

void logger_shutdown(void) {
    if (log_pipe[1] != -1) {
        sys_close(log_pipe[1]); // This sends EOF to the logger
        sys_wait4(logger_pid, NULL, 0, NULL);
    }
}

void log_msg(const char *msg) {
    if (log_pipe[1] != -1 && msg) {
        sys_write(log_pipe[1], msg, str_len(msg));
    }
}
