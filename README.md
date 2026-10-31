# C-Shell

> [!Note]
> This project is entirely vibe-coded for fun. **It is NOT meant to replace Bash, Zsh, or Fish.**

## About

C-Shell is a lightweight, feature-rich, POSIX-compliant command-line shell script written in native C using [Acode](https://github.com/Acode-Foundation/Acode).

## ✨ Features

- **I/O Redirection:** Handle input/output redirection with either `>`, `<`, or `>>`.
- **Piping:** Chain commands with pipes (`|`).
- **Logical Operators:** Execute a command only if the last one succeeds (`&&`), or fails (`||`).
- **History Checking:** Track previously executed commands and save them to `.cshell_history`.
- **Background Execution:** Run commands asynchronously with `&`, and automatically kills zombie processes (`SIGCHLD`).
- **Custom Aliases:** Define shortcuts with `alias` to save time (e.g. `alias la='ls -a'`).
- **Environment Management:** Modify environment variables with `export` and expand them right away (`$TEXT`)
- **Wildcard Globbing:** Full support for pattern matching (`*`).
- **Startup Configuration:** Automatically execute commands on startup with `.cshellrc`.
- **PS1 Customization::** Style your prompt with custom text, modules (`\u`, `\W`), and color codes.
- **Execution Timing:** Measure elapsed time for commands taking longer than 0.5 seconds.

## ✅ Getting Started

### Prerequisite

A standard C compiler (either `clang` or `gcc`) must be installed. If not already (confirm by running `clang --version`), assuming you're on Termux:
```bash
pkg update
pkg install clang
```

### 📥 Installation

### Step 1.
Clone the repository and compile the source code:
```bash
git clone https://github.com/RinSucksAtCoding/C-Shell.git
cd C-Shell
clang c-shell.c -o c-shell
```

### Step 2.
Run your new shell:
```bash
./c-shell
```

## ⚙️ Configuration
C-Shell looks for a configuration file (`~/.cshellrc`) on startup. You can use it to set up custom aliases, environment variables, and your custom prompt format.

## Example:
```bash
alias la='ls -a'
alias weather='curl wttr.in'

export EDITOR='nano'
export PS1="\Y\u\X@\G\h\X in \C\w\X$ "
```

### 🤝 Contributing

Contributions, bug reports, and suggestions are welcome! Get started by checking [CONTRIBUTING.md](CONTRIBUTING.md)!

### ©️ License
Distributed under the MIT license. See [LICENSE](LICENSE) for more information.
