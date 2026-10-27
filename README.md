# C-Shell

[![C Standard](https://shields.io)](https://wikipedia.org/wiki/C_(programming_language))
[![Platform](https://shields.io)]
[![Footprint](https://shields.io)]

> A lightweight Linux shell mimic vibe-coded using Acodd, compiled and run directly inside Termux

## ⚠️ Disclaimer & Project Scope

This project is a experimental C-based shell mimic. **It is NOT meant to replace Bash, Zsh, or Fish.** It was built to explore low-level system calls, process creation, and file descriptor manipulation on mobile environments.

## Features

- **Ultralight Footprint:** Compiles down to a tiny binary (~8.3 KB).
- **I/O Redirection:** Supports standard input/output redirection (`>`, `<`).
- **Piping:** Allows basic command piping (`|`) to chain UNIX programs.
- **History Checking:** Tracks previously executed commands during the active session.

## Current Limitations (Known Bugs)

- **No Aliases:** Attempting to implement shell aliases currently results in a **Segmentation Fault (core dumped)** during execution. Alias support is unavailable until a function that can do so without string evaluation bugs is made.
- **No Configuration File:** Due to this script being highly W.I.P., and the bug mentioned above, the configuration file (`.c-shellrc`) made for this shell is unavailable at this time.

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

Pull requests are highly welcome! Feel free to try and debug the script or expand the built-in command array.
