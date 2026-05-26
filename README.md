# Zero libc POSIX Shell

This project is a minimal, bare-metal POSIX shell built entirely from scratch without utilizing the standard C library (`libc`). It interacts with the Linux kernel directly by invoking software interrupts (system calls) via inline assembly.

This is a true systems programming project. It bypasses high-level wrappers like `system()`, `popen()`, or `<unistd.h>`, making it a living reference for Linux systems programmers to understand the underlying mechanics of process creation, Inter-Process Communication (IPC), memory allocation, and TTY driver control.

---

## ✨ Advanced Features

* **Persistent History & Reverse Search (`Ctrl+R`)**: Command history is saved to `~/.minishell_history` using raw `sys_open`/`sys_read`/`sys_write`. A live interactive reverse-i-search is built directly into the raw TTY terminal driver!
* **$PATH Auto-Completion**: Pressing `TAB` on the first word of a command dynamically searches all directories in your `$PATH` using `sys_getdents64` to provide live executable autocompletion.
* **Wildcard Globbing (`*`, `?`)**: A custom recursive regex-like glob matching engine built entirely from scratch expands wildcards before passing arguments to `sys_execve`.
* **Custom Aliases**: Supports `alias name=value` and `unalias name`. The tokenizer recursively expands aliases inline during lexical analysis before the AST is built!
* **Generalized FD Redirection (`N>&M`)**: Lexes and parses complex file descriptor duplications, utilizing an array of custom `sys_dup2` routings in the execution engine.

```mermaid
flowchart TD
    classDef feat fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef process fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef io fill:#fdcb6e,stroke:#ffeaa7,stroke-width:2px,color:#2d3436;

    Input["User Types Input"]:::io --> Term["terminal.c"]:::process
    
    Term --> CheckCtrlR{"Is Ctrl+R?"}:::feat
    CheckCtrlR -->|"Yes"| History["Read ~/.minishell_history<br/>Reverse Search"]:::feat
    CheckCtrlR -->|"No"| CheckTab{"Is TAB?"}:::feat
    
    CheckTab -->|"Yes"| PathComp["Split $PATH & Scan<br/>using sys_getdents64"]:::feat
    CheckTab -->|"No"| Enter{"Is Enter?"}:::process
    
    Enter -->|"Yes"| Tokenize["tokenizer.c"]:::process
    
    Tokenize --> Alias{"Check Alias Map"}:::feat
    Alias -->|"Match Found"| ExpandAlias["Inline Lexical Replacement"]:::feat
    Alias -->|"No Match"| Parse["parser.c"]:::process
    ExpandAlias --> Parse
    
    Parse --> ParseRedir["Parse N>&M Redirection<br/>into AST Node"]:::feat
    ParseRedir --> Exec["executor.c"]:::process
    
    Exec --> Glob{"Contains * or ?"}:::feat
    Glob -->|"Yes"| ExpandGlob["sys_getdents64<br/>Dynamic Arg Expansion"]:::feat
    Glob -->|"No"| Run["sys_fork & sys_execve"]:::process
    ExpandGlob --> Run
    
    Run --> Dup2["Custom sys_dup2 Loop<br/>for N>&M Redirections"]:::feat
```

---

## 🏗️ Architecture & Flow Diagram

The shell operates through a standard Read-Eval-Print Loop (REPL), breaking down raw text into actionable system processes.

```mermaid
flowchart TD
    classDef init fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef process fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef decision fill:#d63031,stroke:#ff7675,stroke-width:2px,color:#fff;
    classDef io fill:#fdcb6e,stroke:#ffeaa7,stroke-width:2px,color:#2d3436;

    Start(["main.c: _start"]):::init --> Init["Initialize Memory, Signals, Env, Jobs"]:::process
    Init --> Prompt["terminal.c: read_line_raw"]:::io
    
    Prompt --> Tokenizer["tokenizer.c: tokenize"]:::process
    Tokenizer -->|"Expands $VAR & $(cmd)"| Parser["parser.c: parse"]:::process
    Parser -->|"Recursive AST"| Executor["executor.c: execute_ast"]:::process
    
    Executor --> CheckBuiltin{"Is Built-in?"}:::decision
    CheckBuiltin -->|"Yes"| Builtin["builtins.c: execute_builtin"]:::process
    Builtin --> Prompt
    
    CheckBuiltin -->|"No"| Fork["syscalls.c: sys_fork"]:::process
    Fork -->|"Child Process"| ChildWire["Wire multiple pipes & dup2"]:::process
    ChildWire --> Exec["syscalls.c: sys_execve"]:::process
    Exec --> Terminate(["Program Replaces Shell Image"]):::init
    
    Fork -->|"Parent Process"| ParentWait["syscalls.c: sys_wait4"]:::process
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
    classDef code fill:#6c5ce7,stroke:#a29bfe,stroke-width:2px,color:#fff;
    classDef kernel fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;

    UserCode["Shell C Code<br/>(e.g. sys_write)"]:::code --> SyscallFunc["syscall3(number, arg1, arg2, arg3)"]:::code
    SyscallFunc --> Assembly["Inline Assembly<br/>'syscall' instruction"]:::code
    Assembly --> RegisterMap["Map args to Registers<br/>%rdi, %rsi, %rdx, %r10, %r8, %r9"]:::code
    RegisterMap --> KernelTrap["Ring 0 Transition<br/>(Kernel Space)"]:::kernel
    KernelTrap --> KernelExecute["Linux Kernel executes<br/>system call"]:::kernel
    KernelExecute --> ReturnRegister["Result returned in %rax"]:::kernel
    ReturnRegister --> UserCode
```

* **`src/memory.h` & `src/memory.c`**: 
  A custom memory allocator replacing `malloc`/`free`.
  - **Function**: Uses `sys_mmap` with `MAP_ANONYMOUS` to request a raw 16MB page of memory directly from the OS. It implements a rapid "bump pointer" allocator (`mem_alloc`) to dole out dynamic memory chunks to the shell.

```mermaid
flowchart TD
    classDef func fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef dec fill:#d63031,stroke:#ff7675,stroke-width:2px,color:#fff;
    
    Start["Request Memory (mem_alloc)"]:::func --> Check{"Has Enough Free Space?"}:::dec
    Check -->|"No"| Syscall["sys_mmap(MAP_ANONYMOUS)"]:::func
    Syscall --> Map["Kernel maps new page<br/>(4096 bytes)"]:::func
    Map --> SetPointers["Update heap_start & heap_end"]:::func
    SetPointers --> Allocate["Bump Pointer:<br/>current_ptr += size"]:::func
    Check -->|"Yes"| Allocate
    Allocate --> Return["Return Address"]:::func
```

* **`src/dirent.h` & `src/dirent.c`**:
  Raw Directory Parsing.
  - **Function**: Uses the `sys_getdents64` system call to bypass `libc`'s `opendir` and read the raw binary filesystem structures (`linux_dirent64`) directly from the kernel into a memory buffer. This allows the shell to match prefixes for TAB autocompletion.

```mermaid
flowchart TD
    classDef step fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef check fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;
    classDef env fill:#fdcb6e,stroke:#ffeaa7,stroke-width:2px,color:#2d3436;

    Start["User presses TAB"]:::step --> CheckCmd{"Is First Word<br/>(Command)?"}:::check
    CheckCmd -->|"Yes"| SplitPath["Split $PATH string<br/>by ':'"]:::env
    SplitPath --> OpenDir["sys_open(dir_path, O_RDONLY)"]:::step
    
    CheckCmd -->|"No"| OpenLocal["sys_open('.', O_RDONLY)"]:::step
    OpenLocal --> GetDents
    
    OpenDir --> GetDents["sys_getdents64(fd, buffer)"]:::step
    GetDents --> ReadBuffer["Parse linux_dirent64 structs<br/>from raw byte buffer"]:::step
    ReadBuffer --> CheckPrefix{"Matches input<br/>prefix?"}:::check
    CheckPrefix -->|"Yes"| SaveMatch["Save as auto-completion<br/>candidate"]:::step
    CheckPrefix -->|"No"| NextEntry["Move pointer by<br/>d_reclen bytes"]:::step
    SaveMatch --> NextEntry
    NextEntry --> EOFCheck{"More bytes in buffer?"}:::check
    EOFCheck -->|"Yes"| ReadBuffer
    EOFCheck -->|"No"| PathCheck{"More PATH dirs?"}:::check
    
    PathCheck -->|"Yes"| OpenDir
    PathCheck -->|"No"| Complete["Return matching string"]:::step
```

* **`src/string_utils.h` & `src/string_utils.c`**: 
  Replaces `<string.h>`.
  - **Function**: Implements lightweight equivalents of `strlen`, `strcmp`, `strncmp`, `memcpy`, and `memset` necessary for text processing. Also includes a custom integer-to-string formatting routine since `printf` is unavailable.

### 3. Advanced Terminal & Environment Management

* **`src/terminal.h` & `src/terminal.c`**: 
  A low-level terminal (TTY) driver.
  - **Function**: Replaces the standard `read()` loop. It uses the `sys_ioctl` system call to strip the terminal of its default canonical mode (line-buffering), forcing it into **Raw Mode**.
  - **Features**: This allows the shell to intercept individual keystrokes. It parses multi-byte ANSI escape sequences (e.g., `\e[A`) for Up/Down Arrow keys (Command History), and it hooks the `TAB` key into `src/dirent.c` to provide live inline autocompletion for files in the current directory!

```mermaid
flowchart TD
    classDef term fill:#6c5ce7,stroke:#a29bfe,stroke-width:2px,color:#fff;
    classDef dec fill:#d63031,stroke:#ff7675,stroke-width:2px,color:#fff;

    Start["read_line_raw()"]:::term --> DisableCanon["sys_ioctl(TCGETS/TCSETS)<br/>Disable ICANON & ECHO"]:::term
    DisableCanon --> Loop["sys_read(1 byte)"]:::term
    Loop --> CheckChar{"What Character?"}:::dec
    
    CheckChar -->|"Printable"| Echo["sys_write(byte)<br/>Add to buffer"]:::term
    CheckChar -->|"Enter"| End["Restore Terminal<br/>Return Line"]:::term
    CheckChar -->|"Backspace"| Del["sys_write('\b \b')<br/>Remove from buffer"]:::term
    CheckChar -->|"Tab"| AutoComp["getdents64()<br/>Scan Directory"]:::term
    CheckChar -->|"Arrow Keys"| ANSI["Parse ANSI Escape<br/>Navigate History"]:::term
    
    Echo --> Loop
    Del --> Loop
    AutoComp --> Loop
    ANSI --> Loop
```

* **`src/env.h` & `src/env.c`**: 
  A dynamic environment variable manager.
  - **Function**: Captures the initial environment block from the kernel stack and copies it into our custom memory allocator. It exposes `get_env_val` and `set_env_val` so variables can be modified and expanded safely, eventually passing the customized array down to child processes.

### 4. The Shell Pipeline (Parsing & Execution)

* **`src/tokenizer.h` & `src/tokenizer.c`**: 
  The Lexical Analyzer.
  - **Function**: Scans the raw user input character by character and groups them into logical `Token` structures.
  - **Features**: Detects standard words, pipes (`|`), standard input/output redirections (`<`, `>`, `>>`), background flags (`&`), logical operators (`&&`, `||`), control keywords (`if`, `while`), **stderr merging** (`2>&1`), **Command Substitution** (`$(cmd)`), and **Here-Documents** (`<<`). It also detects the `$` prefix to immediately perform **Variable Expansion** against the dynamic environment.

```mermaid
flowchart TD
    classDef token fill:#e84393,stroke:#fd79a8,stroke-width:2px,color:#fff;
    classDef dec fill:#00cec9,stroke:#81ecec,stroke-width:2px,color:#2d3436;

    Input["Raw Input String<br/>'ls -l | grep txt'"]:::token --> SkipSpace{"Is Space?"}:::dec
    SkipSpace -->|"Yes"| Skip["Advance Pointer"]:::token
    Skip --> SkipSpace
    SkipSpace -->|"No"| CheckSpecial{"Is Special Char?<br/>'|', '<', '>', '&'"}:::dec
    
    CheckSpecial -->|"Yes"| MapToken["Map to Specific Token<br/>e.g., TOKEN_PIPE"]:::token
    CheckSpecial -->|"No"| CheckExpansion{"Starts with '$'?"}:::dec
    
    CheckExpansion -->|"Yes"| Expand["Lookup Env Var<br/>Inline Replace"]:::token
    CheckExpansion -->|"No"| BuildWord["Consume chars until<br/>space or quote ends"]:::token
    
    MapToken --> AddArray["Add to Token Array"]:::token
    Expand --> AddArray
    BuildWord --> AddArray
    
    AddArray --> NextChar{"End of String?"}:::dec
    NextChar -->|"No"| SkipSpace
    NextChar -->|"Yes"| EOF["Append TOKEN_EOF"]:::token
```

* **`src/parser.h` & `src/parser.c`**: 
  The Recursive Descent AST Generator.
  - **Function**: Sweeps through the tokens to construct a true Abstract Syntax Tree (AST) using a temporary bump-pointer arena (`mem_alloc_temp`). 
  - **Features**: It recursively parses logical chains (`&&`, `||`), conditionals (`if/then/else/fi`), loops (`while/do/done`), and terminal pipelines. 

```mermaid
flowchart TD
    classDef parse fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef dec fill:#d63031,stroke:#ff7675,stroke-width:2px,color:#fff;

    Start["parse(tokens)"]:::parse --> Logical["parse_logical()"]:::parse
    Logical --> Statement["parse_statement()"]:::parse
    Statement --> CheckType{"Token Type?"}:::dec
    
    CheckType -->|"if"| ParseIf["Build NODE_IF<br/>Recursive: parse_logical()<br/>for condition, then, else"]:::parse
    CheckType -->|"while"| ParseWhile["Build NODE_WHILE<br/>Recursive: parse_logical()<br/>for condition, body"]:::parse
    CheckType -->|"word"| Pipeline["parse_pipeline()<br/>Build NODE_PIPELINE array"]:::parse
    
    ParseIf --> ReturnStmt["Return ASTNode to Logical"]:::parse
    ParseWhile --> ReturnStmt
    Pipeline --> ReturnStmt
    
    ReturnStmt --> CheckOp{"Followed by<br/>'&&' or '||'?"}:::dec
    CheckOp -->|"Yes"| BuildBinary["Build NODE_AND / NODE_OR<br/>Recursive: parse_statement()"]:::parse
    BuildBinary --> CheckOp
    CheckOp -->|"No"| ReturnRoot["Return Root ASTNode"]:::parse
```

* **`src/executor.h` & `src/executor.c`**: 
  The Turing-Complete Execution Engine.
  - **Function**: Recursively traverses the AST. Evaluates condition nodes (e.g. `if`, `while`) by extracting the `WEXITSTATUS` from `sys_wait4`, and conditionally executes `then` or `else` branches.
  - **Advanced I/O**: For Here-Documents (`<<`), the shell dynamically spins up an anonymous pipe, takes over the terminal to read lines of text until the specified delimiter is reached, and securely pipes that data into the child process.
  - **Wiring**: For each command, it triggers `sys_fork()`. Inside the child process, it uses `sys_dup2` to meticulously stitch together standard inputs and outputs to the previously created pipes or file redirections. 

```mermaid
flowchart TD
    classDef exec fill:#fdcb6e,stroke:#ffeaa7,stroke-width:2px,color:#2d3436;
    classDef dec fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;

    Start["Execute Pipeline"]:::exec --> GenPipes["sys_pipe array<br/>for N-1 commands"]:::exec
    GenPipes --> Loop["For each command in AST"]:::exec
    
    Loop --> Heredoc{"Has Here-Doc?"}:::dec
    Heredoc -->|"Yes"| ReadHeredoc["Parent reads TTY into<br/>anonymous pipe"]:::exec
    Heredoc -->|"No"| Fork["sys_fork"]:::exec
    ReadHeredoc --> Fork
    
    Fork -->|"Parent"| SavePID["Track child PID<br/>sys_setpgid"]:::exec
    SavePID --> NextCmdCheck{"More commands?"}:::dec
    NextCmdCheck -->|"Yes"| Loop
    NextCmdCheck -->|"No"| WaitChild["sys_wait4 on children"]:::exec
    
    Fork -->|"Child"| WirePipes["sys_dup2: Wire stdin/stdout<br/>to adjacent IPC pipes"]:::exec
    WirePipes --> FileRedir{"Has File Redir?"}:::dec
    FileRedir -->|"Yes"| OpenFile["sys_open file<br/>sys_dup2 over stdin/stdout"]:::exec
    FileRedir -->|"No"| CheckStderr{"merge_stderr?"}:::dec
    OpenFile --> CheckStderr
    CheckStderr -->|"Yes"| DupStderr["sys_dup2 1, 2"]:::exec
    CheckStderr -->|"No"| Execve["sys_execve path, args, env"]:::exec
    DupStderr --> Execve
    Execve --> End(["Process Replaced"]):::exec
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
    classDef job fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;
    classDef dec fill:#6c5ce7,stroke:#a29bfe,stroke-width:2px,color:#fff;

    Start["Parent Shell"]:::job --> SetupSignals["sys_rt_sigaction<br/>Ignore SIGINT, SIGTTOU"]:::job
    SetupSignals --> Prompt["Wait for Command"]:::job
    Prompt --> Exec{"Foreground or Background?"}:::dec
    
    Exec -->|"Foreground"| Handover["sys_ioctl(TIOCSPGRP)<br/>Give Terminal to Child"]:::job
    Handover --> Wait4["sys_wait4(WUNTRACED)"]:::job
    Wait4 --> CheckStatus{"WIFSTOPPED?"}:::dec
    CheckStatus -->|"Yes"| SaveJob["Save as JOB_STOPPED<br/>Print [Stopped]"]:::job
    CheckStatus -->|"No"| Done["Process Finished"]:::job
    SaveJob --> TakeBack["sys_ioctl(TIOCSPGRP)<br/>Reclaim Terminal"]:::job
    Done --> TakeBack
    TakeBack --> Prompt
    
    Exec -->|"Background"| BgSave["Save as JOB_RUNNING"]:::job
    BgSave --> Prompt
```

---

## 🚀 How to Compile & Run

1. Open a Linux environment (or WSL on Windows).
2. Run `make` to compile the statically linked, zero-libc binary.
3. Run `./tests.sh` to execute the automated POSIX compliance tests.
4. Run `./minishell` to launch the REPL and interact with it (Try testing pipes, redirection, `$PATH` expansion, and the Up-Arrow history!).
