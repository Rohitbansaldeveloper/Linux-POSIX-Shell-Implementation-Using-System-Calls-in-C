#include "builtins.h"
#include "string_utils.h"
#include "syscalls.h"
#include "env.h"
#include "jobs.h"

int is_builtin(const char *cmd) {
    if (!cmd) return 0;
    if (str_cmp(cmd, "cd") == 0) return 1;
    if (str_cmp(cmd, "exit") == 0) return 1;
    if (str_cmp(cmd, "export") == 0) return 1;
    if (str_cmp(cmd, "unset") == 0) return 1;
    if (str_cmp(cmd, "env") == 0) return 1;
    if (str_cmp(cmd, "jobs") == 0) return 1;
    if (str_cmp(cmd, "fg") == 0) return 1;
    if (str_cmp(cmd, "bg") == 0) return 1;
    if (str_cmp(cmd, "pwd") == 0) return 1;
    return 0;
}

int execute_builtin(Command *cmd) {
    if (str_cmp(cmd->argv[0], "cd") == 0) {
        if (cmd->argc < 2) {
            print_str(2, "cd: missing argument\n");
            return -1;
        }
        if (sys_chdir(cmd->argv[1]) < 0) {
            print_str(2, "cd: ");
            print_str(2, cmd->argv[1]);
            print_str(2, ": No such file or directory\n");
            return -1;
        }
        return 0;
    } else if (str_cmp(cmd->argv[0], "pwd") == 0) {
        char buf[1024];
        if (sys_getcwd(buf, sizeof(buf)) >= 0) {
            print_str(1, buf);
            print_str(1, "\n");
        } else {
            print_str(2, "pwd: error\n");
            return -1;
        }
        return 0;
    } else if (str_cmp(cmd->argv[0], "exit") == 0) {
        sys_exit(0);
        return 0;
    } else if (str_cmp(cmd->argv[0], "export") == 0) {
        if (cmd->argc < 2) {
            print_str(2, "export: missing argument\n");
            return -1;
        }
        // Basic parsing: split by '='
        char *arg = cmd->argv[1];
        int i = 0;
        while (arg[i] && arg[i] != '=') i++;
        if (arg[i] == '=') {
            arg[i] = '\0';
            set_env_val(arg, &arg[i+1]);
        }
        return 0;
    } else if (str_cmp(cmd->argv[0], "unset") == 0) {
        if (cmd->argc < 2) {
            print_str(2, "unset: missing argument\n");
            return -1;
        }
        unset_env_val(cmd->argv[1]);
        return 0;
    } else if (str_cmp(cmd->argv[0], "env") == 0) {
        char **env_arr = get_env_array();
        for (int i = 0; env_arr[i] != NULL; i++) {
            print_str(1, env_arr[i]);
            print_str(1, "\n");
        }
        return 0;
    } else if (str_cmp(cmd->argv[0], "jobs") == 0) {
        print_jobs();
        return 0;
    } else if (str_cmp(cmd->argv[0], "fg") == 0 || str_cmp(cmd->argv[0], "bg") == 0) {
        if (cmd->argc < 2) {
            print_str(2, cmd->argv[0]); print_str(2, ": missing job id\n");
            return -1;
        }
        // Basic atoi
        int id = 0;
        for (int i = 0; cmd->argv[1][i]; i++) {
            if (cmd->argv[1][i] >= '0' && cmd->argv[1][i] <= '9') {
                id = id * 10 + (cmd->argv[1][i] - '0');
            }
        }
        continue_job(id, (cmd->argv[0][0] == 'b'));
        return 0;
    }
    return -1;
}
