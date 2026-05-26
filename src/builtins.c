#include "builtins.h"
#include "string_utils.h"
#include "syscalls.h"
#include "env.h"

int is_builtin(const char *cmd) {
    if (!cmd) return 0;
    if (str_cmp(cmd, "cd") == 0) return 1;
    if (str_cmp(cmd, "exit") == 0) return 1;
    if (str_cmp(cmd, "export") == 0) return 1;
    if (str_cmp(cmd, "env") == 0) return 1;
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
    } else if (str_cmp(cmd->argv[0], "env") == 0) {
        char **env_arr = get_env_array();
        for (int i = 0; env_arr[i] != NULL; i++) {
            print_str(1, env_arr[i]);
            print_str(1, "\n");
        }
        return 0;
    }
    return -1;
}
