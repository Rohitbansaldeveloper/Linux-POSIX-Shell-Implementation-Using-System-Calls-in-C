#include "builtins.h"
#include "string_utils.h"
#include "syscalls.h"
#include "env.h"
#include "jobs.h"
#include "alias.h"
#include "memory.h"

int is_builtin(const char *cmd) {
    if (!cmd) return 0;
    if (str_cmp(cmd, "cd") == 0) return 1;
    if (str_cmp(cmd, "exit") == 0) return 1;
    if (str_cmp(cmd, "export") == 0) return 1;
    if (str_cmp(cmd, "unset") == 0) return 1;
    if (str_cmp(cmd, "alias") == 0) return 1;
    if (str_cmp(cmd, "unalias") == 0) return 1;
    if (str_cmp(cmd, "env") == 0) return 1;
    if (str_cmp(cmd, "jobs") == 0) return 1;
    if (str_cmp(cmd, "fg") == 0) return 1;
    if (str_cmp(cmd, "bg") == 0) return 1;
    if (str_cmp(cmd, "pwd") == 0) return 1;
    if (str_cmp(cmd, "pstree") == 0) return 1;
    return 0;
}

typedef struct ProcNode {
    int pid;
    int ppid;
    char name[256];
    struct ProcNode *list_next;
    struct ProcNode *sibling;
    struct ProcNode *child;
} ProcNode;

static int parse_int(const char *str, const char **endptr) {
    int val = 0;
    while (*str == ' ') str++;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    if (endptr) *endptr = str;
    return val;
}

static void print_tree(ProcNode *node, int depth) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) {
        print_str(1, "  ");
    }
    if (depth > 0) print_str(1, "|- ");
    print_str(1, node->name);
    print_str(1, " (");
    print_int(1, node->pid);
    print_str(1, ")\n");
    
    print_tree(node->child, depth + 1);
    if (depth > 0) { // Siblings of root are printed at depth 0 below
        print_tree(node->sibling, depth);
    }
}

static void print_pstree(void) {
    int fd = sys_open("/proc", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        print_str(2, "pstree: cannot open /proc\n");
        return;
    }
    
    ProcNode *head = NULL;
    char buf[4096];
    while (1) {
        int nread = sys_getdents64(fd, buf, sizeof(buf));
        if (nread <= 0) break;
        
        int bpos = 0;
        while (bpos < nread) {
            struct linux_dirent64 {
                unsigned long  d_ino;    
                long           d_off;    
                unsigned short d_reclen; 
                unsigned char  d_type;   
                char           d_name[]; 
            };
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);
            
            if (d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                char stat_path[256];
                str_cpy(stat_path, "/proc/");
                str_cpy(stat_path + str_len(stat_path), d->d_name);
                str_cpy(stat_path + str_len(stat_path), "/stat");
                
                int sfd = sys_open(stat_path, O_RDONLY, 0);
                if (sfd >= 0) {
                    char sbuf[1024];
                    int sn = sys_read(sfd, sbuf, sizeof(sbuf) - 1);
                    sys_close(sfd);
                    if (sn > 0) {
                        sbuf[sn] = '\0';
                        const char *p = sbuf;
                        int pid = parse_int(p, &p);
                        
                        char *lparen = str_chr(sbuf, '(');
                        char *rparen = lparen;
                        char *tmp = lparen;
                        while (tmp) {
                            rparen = tmp;
                            tmp = str_chr(tmp + 1, ')');
                        }
                        
                        if (lparen && rparen) {
                            char name[256] = {0};
                            int len = rparen - lparen - 1;
                            if (len > 255) len = 255;
                            mem_cpy(name, lparen + 1, len);
                            name[len] = '\0';
                            
                            p = rparen + 2; 
                            while (*p && *p != ' ') p++; 
                            if (*p == ' ') p++;
                            int ppid = parse_int(p, NULL);
                            
                            ProcNode *node = mem_alloc_temp(sizeof(ProcNode));
                            if (node) {
                                node->pid = pid;
                                node->ppid = ppid;
                                str_cpy(node->name, name);
                                node->list_next = head;
                                node->sibling = NULL;
                                node->child = NULL;
                                head = node;
                            }
                        }
                    }
                }
            }
            bpos += d->d_reclen;
        }
    }
    sys_close(fd);
    
    // Build tree
    ProcNode *curr = head;
    ProcNode *root_list = NULL;
    
    while (curr) {
        ProcNode *p = head;
        int found = 0;
        if (curr->ppid != 0) {
            while (p) {
                if (p->pid == curr->ppid) {
                    curr->sibling = p->child;
                    p->child = curr;
                    found = 1;
                    break;
                }
                p = p->list_next;
            }
        }
        if (!found) {
            curr->sibling = root_list;
            root_list = curr;
        }
        curr = curr->list_next;
    }
    
    // Print all roots
    ProcNode *r = root_list;
    while (r) {
        print_tree(r, 0);
        r = r->sibling;
    }
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
    } else if (str_cmp(cmd->argv[0], "alias") == 0) {
        if (cmd->argc == 1) {
            print_aliases();
            return 0;
        }
        char *arg = cmd->argv[1];
        int i = 0;
        while (arg[i] && arg[i] != '=') i++;
        if (arg[i] == '=') {
            arg[i] = '\0';
            set_alias(arg, &arg[i+1]);
        }
        return 0;
    } else if (str_cmp(cmd->argv[0], "unalias") == 0) {
        if (cmd->argc > 1) {
            unset_alias(cmd->argv[1]);
        }
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
    } else if (str_cmp(cmd->argv[0], "pstree") == 0) {
        print_pstree();
        return 0;
    }
    return -1;
}
