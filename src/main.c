/*
 * main.c
 * 
 * This is the entry point of the application. Since we are compiling with
 * `-nostdlib`, the C compiler does not link the standard C runtime (crt0).
 * Normally, the C runtime sets up the environment, arguments, and calls `main()`.
 * Without it, the kernel directly jumps to a symbol named `_start`.
 * 
 * At the moment `_start` begins, the stack pointer (%rsp) points directly to the 
 * argument count (argc), followed immediately by the argument array (argv),
 * a NULL pointer, the environment array (envp), and another NULL pointer.
 */

#include "syscalls.h"
#include "string_utils.h"
#include "memory.h"
#include "tokenizer.h"
#include "parser.h"
#include "executor.h"
#include "jobs.h"
#include "signals.h"
#include "terminal.h"
#include "env.h"

#define MAX_INPUT 1024

void _start(void) {
    // 1. Stack parsing
    // We use inline assembly to read the current stack pointer (%rsp) into a C variable.
    long *rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    
    // According to the System V AMD64 ABI:
    // rsp[0] is argc
    long argc = rsp[0];
    
    // rsp[1] to rsp[1 + argc] are the argv pointers
    char **argv = (char **)&rsp[1];
    
    // rsp[1 + argc] is a NULL pointer terminating argv
    // rsp[1 + argc + 1] starts the envp pointers
    char **envp = (char **)&rsp[1 + argc + 1];

    // 2. Subsystem Initialization
    mem_init();       // Setup the heap (via mmap)
    init_env(envp);   // Initialize dynamic environment variables
    setup_signals();  // Setup signal handlers (ignore Ctrl+C in parent)
    init_jobs();      // Setup job control structures

    // 3. Shell REPL (Read-Eval-Print Loop)
    char input[MAX_INPUT];
    Token tokens[MAX_TOKENS];
    Pipeline pipeline;

    while (1) {
        // Print the shell prompt to stdout (file descriptor 1)
        print_str(1, "$ ");

        // Read user input using raw terminal mode (supports history/arrows)
        int bytes_read = read_line_raw(input, MAX_INPUT);
        if (bytes_read == 0) {
            // EOF (Ctrl+D)
            print_str(1, "\n");
            break; 
        }
        input[bytes_read] = '\0'; // Null-terminate the raw input string

        // Asynchronously reap any background children that have exited
        // so they don't become zombies.
        check_background_jobs();

        // Lexical Analysis: Convert raw string into tokens
        int num_tokens = tokenize(input, tokens, MAX_TOKENS);
        if (num_tokens == 0 || tokens[0].type == TOKEN_EOF) {
            continue; // Empty input (e.g. just pressed enter)
        }

        // Parsing: Convert tokens into a structured Pipeline
        if (parse(tokens, &pipeline) < 0) {
            print_str(2, "syntax error\n"); // Print errors to stderr (2)
            continue;
        }

        // Execution: Run the pipeline
        execute_pipeline(&pipeline);
    }

    // Normal termination
    sys_exit(0);
}
