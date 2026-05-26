#ifndef ENV_H
#define ENV_H

#define MAX_ENV_VARS 128

// Initialize our dynamic environment from the kernel's stack envp
void init_env(char **envp);

// Get a pointer to the array of environment variable strings (for execve)
char **get_env_array(void);

// Get the value of a specific environment variable
char *get_env_val(const char *name);

// Set or update an environment variable (export)
int set_env_val(const char *name, const char *value);

// Unset an environment variable
int unset_env_val(const char *name);

#endif
