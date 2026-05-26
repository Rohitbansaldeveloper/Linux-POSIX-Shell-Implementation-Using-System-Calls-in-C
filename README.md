# Zero libc POSIX Shell

This project is a minimal, bare-metal POSIX shell built entirely from scratch without utilizing the standard C library (`libc`). It interacts with the Linux kernel directly by invoking software interrupts (system calls) via inline assembly.

This is a true systems programming project. It bypasses high-level wrappers like `system()`, `popen()`, or `<unistd.h>`, making it a living reference for Linux systems programmers to understand the underlying mechanics of process creation, Inter-Process Communication (IPC), memory allocation, and TTY driver control.

---

## 🏗️ Architecture & Flow Diagram

The shell operates through a standard Read-Eval-Print Loop (REPL), breaking down raw text into actionable system processes.

```mermaid
flowchart TD
    Start([main.c: _start]) --> Init[Initialize Memory, Signals, Env, Jobs]
    Init --> Prompt[terminal.c: read_line_raw]
    
    Prompt --> Tokenizer[tokenizer.c: tokenize]
    Tokenizer --> |Expands $VAR| Parser[parser.c: parse]
    Parser --> |AST / Pipeline Array| Executor[executor.c: execute_pipeline]
    
    Executor --> CheckBuiltin{Is Built-in?}
    CheckBuiltin -->|Yes| Builtin[builtins.c: execute_builtin]
    Builtin --> Prompt
    
    CheckBuiltin -->|No| Fork[syscalls.c: sys_fork]
    Fork -->|Child Process| ChildWire[Wire multiple pipes & dup2]
    ChildWire --> Exec[syscalls.c: sys_execve]
    Exec --> Terminate([Program Replaces Shell Image])
    
    Fork -->|Parent Process| ParentWait[syscalls.c: sys_wait4]
    ParentWait --> Prompt
```

---

## 📁 Deep Dive into File Structure & Functions

Because we lack `libc`, this project is highly modular, reimplementing many fundamental OS interfaces and C primitives from the ground up.

### 1. Build System & Entry Point

* **`Makefile`**: 
  The compiler configuration. It uses `-nostdlib`, `-static`, and `-fno-builtin` flags to ensure the compiler generates a freestanding binary without linking the standard C runtime (`crt0` or `glibc`).
* **`src/main.c`**: 
  Contains the `_start` symbol, which the kernel directly jumps to when executing the binary.
  - **Function**: Manually parses the raw CPU stack pointer (`%rsp`) to extract `argc`, `argv`, and the initial `envp` (environment pointer). It calls all initialization routines and then loops indefinitely to power the shell's REPL.

### 2. Linux Kernel Interface (The Primitives)

* **`src/syscalls.h` & `src/syscalls.c`**: 
  The core of the "zero libc" implementation.
  - **Function**: Uses `x86_64` inline assembly to trigger software interrupts via the `syscall` instruction. It maps C function arguments into the specific CPU registers the Linux kernel demands (`%rdi`, `%rsi`, `%rdx`, `%r10`, etc.).
  - **Provides**: Wrappers for `sys_read`, `sys_write`, `sys_open`, `sys_pipe`, `sys_dup2`, `sys_fork`, `sys_execve`, `sys_wait4`, `sys_mmap`, `sys_ioctl`, and `sys_rt_sigaction`.

  ```mermaid
  flowchart TD
      UserCode["Shell C Code<br>(e.g. sys_write)"] --> SyscallFunc["syscall3(number, arg1, arg2, arg3)"]
      SyscallFunc --> Assembly["Inline Assembly<br>'syscall' instruction"]
      Assembly --> RegisterMap["Map args to Registers<br>%rdi, %rsi, %rdx, %r10, %r8, %r9"]
      RegisterMap --> KernelTrap["Ring 0 Transition<br>(Kernel Space)"]
      KernelTrap --> KernelExecute["Linux Kernel executes<br>system call"]
      KernelExecute --> ReturnRegister["Result returned in %rax"]
      ReturnRegister --> UserCode
  ```
* **`src/memory.h` & `src/memory.c`**: 
  A custom memory allocator replacing `malloc`/`free`.
  - **Function**: Uses `sys_mmap` with `MAP_ANONYMOUS` to request a raw 16MB page of memory directly from the OS. It implements a rapid "bump pointer" allocator (`mem_alloc`) to dole out dynamic memory chunks to the shell.
  
  ```mermaid
  flowchart TD
      Start["Request Memory (mem_alloc)"] --> Check{"Has Enough Free Space?"}
      Check -->|No| Syscall["sys_mmap(MAP_ANONYMOUS)"]
      Syscall --> Map["Kernel maps new page<br>(4096 bytes)"]
      Map --> SetPointers["Update heap_start & heap_end"]
      SetPointers --> Allocate["Bump Pointer:<br>current_ptr += size"]
      Check -->|Yes| Allocate
      Allocate --> Return["Return Address"]
  ```

* **`src/dirent.h` & `src/dirent.c`**:
  Raw Directory Parsing.
  - **Function**: Uses the `sys_getdents64` system call to bypass `libc`'s `opendir` and read the raw binary filesystem structures (`linux_dirent64`) directly from the kernel into a memory buffer. This allows the shell to match prefixes for TAB autocompletion.

  ```mermaid
  flowchart TD
      Start["User presses TAB"] --> OpenDir["sys_open('.', O_RDONLY)"]
      OpenDir --> GetDents["sys_getdents64(fd, buffer)"]
      GetDents --> ReadBuffer["Parse linux_dirent64 structs<br>from raw byte buffer"]
      ReadBuffer --> CheckPrefix{"Matches input<br>prefix?"}
      CheckPrefix -->|Yes| SaveMatch["Save as auto-completion<br>candidate"]
      CheckPrefix -->|No| NextEntry["Move pointer by<br>d_reclen bytes"]
      SaveMatch --> NextEntry
      NextEntry --> EOFCheck{"More bytes in buffer?"}
      EOFCheck -->|Yes| ReadBuffer
      EOFCheck -->|No| Complete["Return matching string"]
  ```

* **`src/string_utils.h` & `src/string_utils.c`**: 
  Replaces `<string.h>`.
  - **Function**: Implements lightweight equivalents of `strlen`, `strcmp`, `strncmp`, `memcpy`, and `memset` necessary for text processing. Also includes a custom integer-to-string formatting routine since `printf` is unavailable.

  ```mermaid
  flowchart TD
      StringReq["String Operation<br>(e.g., str_cpy, str_cmp)"] --> LoopChars["Loop through memory<br>byte by byte"]
      LoopChars --> Compare{"Check char<br>against '\0'"}
      Compare -->|Not NULL| Process["Copy/Compare/Count byte"]
      Process --> Advance["Pointer++"]
      Advance --> LoopChars
      Compare -->|NULL| Return["Return Length/Diff/Pointer"]
  ```

### 3. Advanced Terminal & Environment Management

* **`src/terminal.h` & `src/terminal.c`**: 
  A low-level terminal (TTY) driver.
  - **Function**: Replaces the standard `read()` loop. It uses the `sys_ioctl` system call to strip the terminal of its default canonical mode (line-buffering), forcing it into **Raw Mode**.
  - **Features**: This allows the shell to intercept individual keystrokes. It parses multi-byte ANSI escape sequences (e.g., `\e[A`) for Up/Down Arrow keys (Command History), and it hooks the `TAB` key into `src/dirent.c` to provide live inline autocompletion for files in the current directory!

  ```mermaid
  flowchart TD
      Start["read_line_raw()"] --> DisableCanon["sys_ioctl(TCGETS/TCSETS)<br>Disable ICANON & ECHO"]
      DisableCanon --> Loop["sys_read(1 byte)"]
      Loop --> CheckChar{"What Character?"}
      
      CheckChar -->|Printable| Echo["sys_write(byte)<br>Add to buffer"]
      CheckChar -->|Enter| End["Restore Terminal<br>Return Line"]
      CheckChar -->|Backspace| Del["sys_write('\b \b')<br>Remove from buffer"]
      CheckChar -->|Tab| AutoComp["getdents64()<br>Scan Directory"]
      CheckChar -->|Arrow Keys| ANSI["Parse ANSI Escape<br>Navigate History"]
      
      Echo --> Loop
      Del --> Loop
      AutoComp --> Loop
      ANSI --> Loop
  ```

* **`src/env.h` & `src/env.c`**: 
  A dynamic environment variable manager.
  - **Function**: Captures the initial environment block from the kernel stack and copies it into our custom memory allocator. It exposes `get_env_val` and `set_env_val` so variables can be modified and expanded safely, eventually passing the customized array down to child processes.

  ```mermaid
  flowchart TD
      Init["_start extracts envp<br>from stack pointer"] --> CopyEnv["mem_alloc space for<br>MAX_ENV strings"]
      CopyEnv --> LoadArray["Deep copy initial<br>OS environment variables"]
      
      LoadArray --> Request{"User Request"}
      
      Request -->|export VAR=VAL| Export["Find empty slot or update<br>Set env_array[i] = 'VAR=VAL'"]
      Request -->|env| Print["Loop and sys_write<br>all non-NULL slots"]
      Request -->|Token expands $VAR| GetVal["Iterate env_array<br>Match prefix 'VAR='<br>Return pointer to 'VAL'"]
      
      Export --> Exec["Pass custom env_array<br>down to child sys_execve"]
  ```

### 4. The Shell Pipeline (Parsing & Execution)

* **`src/tokenizer.h` & `src/tokenizer.c`**: 
  The Lexical Analyzer.
  - **Function**: Scans the raw user input character by character and groups them into logical `Token` structures.
  - **Features**: Detects standard words, pipes (`|`), standard input/output redirections (`<`, `>`, `>>`), background flags (`&`), **stderr merging** (`2>&1`), and **Here-Documents** (`<<`). It also detects the `$` prefix to immediately perform **Variable Expansion** against the dynamic environment (e.g., transforming `$HOME` into `/home/user`).
  
  ```mermaid
  flowchart TD
      Input["Raw Input String<br>'ls -l | grep txt'"] --> SkipSpace{"Is Space?"}
      SkipSpace -->|Yes| Skip["Advance Pointer"]
      Skip --> SkipSpace
      SkipSpace -->|No| CheckSpecial{"Is Special Char?<br>'|', '<', '>', '&'"}
      
      CheckSpecial -->|Yes| MapToken["Map to Specific Token<br>e.g., TOKEN_PIPE"]
      CheckSpecial -->|No| CheckExpansion{"Starts with '$'?"}
      
      CheckExpansion -->|Yes| Expand["Lookup Env Var<br>Inline Replace"]
      CheckExpansion -->|No| BuildWord["Consume chars until<br>space or quote ends"]
      
      MapToken --> AddArray["Add to Token Array"]
      Expand --> AddArray
      BuildWord --> AddArray
      
      AddArray --> NextChar{"End of String?"}
      NextChar -->|No| SkipSpace
      NextChar -->|Yes| EOF["Append TOKEN_EOF"]
  ```

* **`src/parser.h` & `src/parser.c`**: 
  The AST Generator.
  - **Function**: Sweeps through the tokens to construct a `Pipeline` object containing up to 16 `Command` arrays. It resolves `argc` and `argv` boundaries and maps any file redirections and Here-Doc delimiters to the specific command's input/output properties.

  ```mermaid
  flowchart TD
      Start[Iterate Token Array] --> TokenWord{Is TOKEN_WORD?}
      TokenWord -->|Yes| AddArg[Add to current\ncmd->argv array]
      
      TokenWord -->|No| TokenPipe{Is TOKEN_PIPE?}
      TokenPipe -->|Yes| NextCmd[Null-terminate argv\nMove to next Command slot]
      
      TokenPipe -->|No| TokenRedirect{Is Redirect?\n'<', '>', '>>'}
      TokenRedirect -->|Yes| SetFile[Set redirect_in or\nredirect_out to next token]
      
      TokenRedirect -->|No| TokenHeredoc{Is Here-Doc?\n'<<'}
      TokenHeredoc -->|Yes| SetDelim[Set heredoc_delimiter\nto next token]
      
      TokenHeredoc -->|No| TokenBackground{Is '&'?}
      TokenBackground -->|Yes| SetBg[Set pipeline->background = 1]
      
      AddArg --> NextToken[Next Token]
      NextCmd --> NextToken
      SetFile --> NextToken
      SetDelim --> NextToken
      SetBg --> NextToken
      
      NextToken --> EOFCheck{Is TOKEN_EOF?}
      EOFCheck -->|No| TokenWord
      EOFCheck -->|Yes| Done[Return Pipeline AST]
  ```

* **`src/executor.h` & `src/executor.c`**: 
  The heart of the shell—the Execution Engine.
  - **Function**: Iterates through the pipeline array and orchestrates the complex dance of process cloning.
  - **Advanced I/O**: For Here-Documents (`<<`), the shell dynamically spins up an anonymous pipe, takes over the terminal to read lines of text until the specified delimiter is reached, and securely pipes that data into the child process. It also fully supports mapping `stderr` to `stdout` (`2>&1`).
  - **Multi-Pipe Support**: Creates an array of IPC channels via `sys_pipe`.
  - **Wiring**: For each command, it triggers `sys_fork()`. Inside the child process, it uses `sys_dup2` to meticulously stitch together standard inputs and outputs to the previously created pipes or file redirections. 
  - **Image Replacement**: Finally, it calls `sys_execve()` to obliterate the child shell process and replace it with the target binary (e.g., `/bin/ls`), passing along our custom environment.

  ```mermaid
  flowchart TD
      Start[Execute Pipeline] --> GenPipes[sys_pipe array\nfor N-1 commands]
      GenPipes --> Loop[For each command in AST]
      
      Loop --> Heredoc{Has Here-Doc?}
      Heredoc -->|Yes| ReadHeredoc[Parent reads TTY into\nanonymous pipe]
      Heredoc -->|No| Fork[sys_fork]
      ReadHeredoc --> Fork
      
      Fork -->|Parent| SavePID[Track child PID\nsys_setpgid]
      SavePID --> NextCmdCheck{More commands?}
      NextCmdCheck -->|Yes| Loop
      NextCmdCheck -->|No| WaitChild[sys_wait4 on children]
      
      Fork -->|Child| WirePipes[sys_dup2: Wire stdin/stdout\nto adjacent IPC pipes]
      WirePipes --> FileRedir{Has File Redir?}
      FileRedir -->|Yes| OpenFile[sys_open file\nsys_dup2 over stdin/stdout]
      FileRedir -->|No| CheckStderr{merge_stderr?}
      OpenFile --> CheckStderr
      CheckStderr -->|Yes| DupStderr[sys_dup2 1, 2]
      CheckStderr -->|No| Execve[sys_execve path, args, env]
      DupStderr --> Execve
      Execve --> End([Process Replaced])
  ```

### 5. Process Control & Builtins

* **`src/builtins.h` & `src/builtins.c`**: 
  State-modifying shell commands.
  - **Function**: Executes commands that must run inside the parent shell process (since a child cannot change the parent's environment). Supports `cd` (using the `chdir` syscall), `exit`, `env`, `export`, `jobs`, `fg`, and `bg`.
* **`src/signals.h` & `src/signals.c`**: 
  POSIX Signal Handling.
  - **Function**: Constructs a kernel `k_sigaction` struct and registers it via `sys_rt_sigaction`. It explicitly tells the kernel to ignore (`SIG_IGN`) `SIGINT` (Ctrl+C), `SIGQUIT` (Ctrl+\), and job control signals (`SIGTTOU`, `SIGTTIN`, `SIGTSTP`) for the parent shell. This ensures you don't accidentally kill or suspend the shell when interacting with a foreground job. Child processes have their signals reset (`SIG_DFL`) before execution.
* **`src/jobs.h` & `src/jobs.c`**: 
  True Job Control & Process Groups.
  - **Function**: Manages the complex state of foreground and background jobs. It assigns processes to distinct Process Groups (`sys_setpgid`) and explicitly hands over terminal control (`sys_ioctl` with `TIOCSPGRP`) to foreground jobs.
  - **Features**: This allows you to press `Ctrl+Z` to suspend a running process (`SIGTSTP`), track it in the background using the `jobs` command, and resume it in the foreground using `fg 1` (sending `SIGCONT`). Background "zombie" processes are cleanly reaped using a non-blocking `sys_wait4` call (`WNOHANG | WUNTRACED`).

  ```mermaid
  flowchart TD
      Start["Parent Shell"] --> SetupSignals["sys_rt_sigaction<br>Ignore SIGINT, SIGTTOU"]
      SetupSignals --> Prompt["Wait for Command"]
      Prompt --> Exec{"Foreground or Background?"}
      
      Exec -->|Foreground| Handover["sys_ioctl(TIOCSPGRP)<br>Give Terminal to Child"]
      Handover --> Wait4["sys_wait4(WUNTRACED)"]
      Wait4 --> CheckStatus{"WIFSTOPPED?"}
      CheckStatus -->|Yes| SaveJob["Save as JOB_STOPPED<br>Print [Stopped]"]
      CheckStatus -->|No| Done["Process Finished"]
      SaveJob --> TakeBack["sys_ioctl(TIOCSPGRP)<br>Reclaim Terminal"]
      Done --> TakeBack
      TakeBack --> Prompt
      
      Exec -->|Background| BgSave["Save as JOB_RUNNING"]
      BgSave --> Prompt
  ```

---

## 🚀 How to Compile & Run

1. Open a Linux environment (or WSL on Windows).
2. Run `make` to compile the statically linked, zero-libc binary.
3. Run `./tests.sh` to execute the automated POSIX compliance tests.
4. Run `./minishell` to launch the REPL and interact with it (Try testing pipes, redirection, `$PATH` expansion, and the Up-Arrow history!).
