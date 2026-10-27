# C-Shell

[![C Standard](https://shields.io)](https://wikipedia.org/wiki/C_(programming_language))
[![Platform](https://shields.io)]
[![Footprint](https://shields.io)]

> A lightweight Linux shell mimic vibe-coded using Acodd, compiled and run directly inside Termux

## ⚠️ Disclaimer & Project Scope

This project is an experimental C-based shell mimic. **It is NOT meant to replace Bash, Zsh, or Fish.** It was built to explore low-level system calls, process creation, and file descriptor manipulation on mobile environments.

## Features

- **Ultralight Footprint:** Compiles down to a tiny binary (~8.3 KB).
- **I/O Redirection:** Supports standard input/output redirection (`>`, `<`).
- **Piping:** Allows basic command piping (`|`) to chain UNIX programs.
- **History Checking:** Tracks previously executed commands during the active session.

## 🚧 Roadblocked Features (Unavailable Due to Bugs)

To preserve system stability and keep the footprint small, the following features are **completely absent from the source code** until structural pointer bugs are resolved:

- **No Alias Support:** Implementing string replacement for command aliases triggered severe **Segmentation Faults (core dumped)** during tokenization. The feature has been completely left out.
- **No Configuration File:** A startup configuration file (proposed as `.cshellrc` to store future aliases and custom prompts) is currently unavailable, as it relies heavily on the broken alias/string-parsing subsystem.

## 🛠️ Under the Hood: Technical Architecture

Despite its ~8.3 KB size, `c-shell.c` interacts directly with the Linux kernel via POSIX system calls:

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

You need an Android device running **Termux** with the `clang` package installed.

```bash
pkg update && pkg upgrade
pkg install clang
```

### Installation & Compilation

Since this is a lightweight, zero-dependency script, no `Makefile` or `CMake` is required. You can compile the source file directly using `clang`:

```bash
# Clone or move your C file into your workspace
# Compile directly with Clang
clang -Wall -Wextra -O2 c-shell.c -o c-shell

# Run the shell
./c-shell
```

## Usage & Syntax

Once inside `c-shell`, you can interact with it like a minimal UNIX prompt.

```text
$ ~ | C-Shell > ls -lh
$ ~ | C-Shell > find -type f | wc -l > file_counter.log
$ ~ | C-Shell > history
$ ~ | C-Shell > exit
```

## Contributing

Want to help unpack the memory bugs? Pull requests are highly welcome! We are actively looking for a robust, lightweight string-tokenization function that can safely parse config text and swap alias strings without overflowing bounds or causing segmentation faults.
