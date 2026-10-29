# C-Shell

> [!Note]
> This project is entirely vibe-coded for fun. **It is NOT meant to replace Bash, Zsh, or Fish.**

## About

C-Shell is a shell script made entirely in C using [Acode](https://github.com/Acode-Foundation/Acode).

## Features

- **I/O Redirection:** Supports standard input/output redirection (`>`, `<`).
- **Piping:** Allows basic command piping (`|`) to chain UNIX programs.
- **Operators:** Allows commands to execute only if the last one succeeds (`&&`), or fails (`||`).
- **History Checking:** Tracks previously executed commands during the active session.

## 🚧 Roadblocked Features (Unavailable Due to Bugs)

To preserve system stability, the following features are **completely absent from the source code** until structural pointer bugs are resolved:

- **No Alias Support:** Implementing string replacement for command aliases triggered severe **Segmentation Faults (core dumped)** during tokenization. The feature has been completely left out.
- **No Configuration File:** A startup configuration file (proposed as `.cshellrc` to store future aliases and custom prompts) is currently unavailable, as it relies heavily on the broken alias/string-parsing subsystem.

## 🛠️ Under the Hood: Technical Architecture

`c-shell.c` interacts directly with the Linux kernel via POSIX system calls:

### 1. Piping & Redirection Mechanics
Inter-process communication (IPC) for commands like `cat file.txt | grep "test"` is managed manually:
- **`pipe(pipefds)`:** Instantiates a unidirectional data channel with a read and write file descriptor.
- **`fork()`:** Spawns distinct child processes for each side of the pipeline.
- **`dup2()`:** Rearranges file descriptors. It duplicates the pipe's write-end over standard output (`STDOUT_FILENO`) for the producer, and the read-end over standard input (`STDIN_FILENO`) for the consumer.
- **`execvp()`:** Mutates the child process images into the targeted system binaries.

### 2. Session History Architecture
The `history` command reads from an internal command array structured as a sequential tracking log:
- **Active Session Bounds:** The shell tracks the last **100 commands** (depending on stack optimization limits) executed within the running instance.
- **Volatile Execution:** Because configuration parsing is locked out, history is strictly stored in heap/stack memory and clears completely upon typing `exit`.

## Getting Started

### Prerequisites

- Termux must be installed.
- `clang` must be installed. If not installed already (confirm by running `clang --version`), do so by running `pkg install clang`.
- Either `curl` or `wget` must be installed.

### Installation

Since this is a lightweight, zero-dependency script, no `Makefile` or `CMake` is required.

### Step 1. Download the file
| Method | Installation |
| ------ | ------------ |
| `curl` | `curl -L -O https://raw.githubusercontent.com/RinSucksAtCoding/C-Shell/refs/heads/main/c-shell.c` |
| `wget` | `wget https://raw.githubusercontent.com/RinSucksAtCoding/C-Shell/refs/heads/main/c-shell.c` |

### Step 2. Compile directly with Clang
Run the following:
```bash
 clang -Wall -Wextra -O2 c-shell.c -o c-shell
```

### Step 3. Run the shell
Run the following:
```bash
./c-shell
```

## Usage & Syntax

Once inside `c-shell`, you can interact with it like a minimal UNIX prompt.

```text
$ home | C-Shell > ls -lh
$ home | C-Shell > find -type f | wc -l > file_counter.log
$ home | C-Shell > history
$ home | C-Shell > exit
```

## Contributing

Want to help unpack the memory bugs? Pull requests are highly welcome! We are actively looking for a robust, lightweight string-tokenization function that can safely parse config text and swap alias strings without overflowing bounds or causing segmentation faults.
