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
#include "logger.h"

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
    logger_init();    // Initialize logger
    load_history();   // Load command history from disk

    // 3. Script Execution Mode
    if (argc > 1) {
        int fd = sys_open(argv[1], 0, 0); // O_RDONLY
        if (fd < 0) {
            print_str(2, "minishell: cannot open script\n");
            sys_exit(1);
        }
        
        char *script_buf = mem_alloc_temp(65536);
        if (script_buf) {
            int total = 0;
            while (total < 65535) {
                int n = sys_read(fd, script_buf + total, 65535 - total);
                if (n <= 0) break;
                total += n;
            }
            script_buf[total] = '\0';
            
            Token tokens[4096];
            int num_tokens = tokenize(script_buf, tokens, 4096);
            if (num_tokens > 0) {
                ASTNode *ast = parse(tokens);
                if (ast) execute_ast(ast);
            }
        }
        sys_close(fd);
        logger_shutdown();
        sys_exit(0);
    }

    // 4. Shell REPL (Read-Eval-Print Loop)
    char input[MAX_INPUT];
    Token tokens[MAX_TOKENS];

    while (1) {
        // Read user input using raw terminal mode (supports history/arrows)
        int bytes_read = read_line_raw("$ ", input, MAX_INPUT);
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

        // Parsing: Convert tokens into a structured AST
        ASTNode *ast = parse(tokens);
        if (!ast) {
            if (tokens[0].type != TOKEN_EOF && tokens[0].type != TOKEN_SEMI) {
                print_str(2, "syntax error\n");
            }
            mem_reset_temp();
            continue;
        }

        // Execution: Run the pipeline
        execute_ast(ast);
        
        // Reset the temporary memory arena to free the AST
        mem_reset_temp();
    }

    // Normal termination
    sys_exit(0);
}
