# Contributing to C-Shell

First off, thankk you for taking the time to contribute! Whether it's fixing a bug, adding a new built-in command, or improving documentation, every contribution makes this project better.

Please review this guide to make your contribution process smooth and effective.

---

## ✅ Getting Started

1. **Fork the Repository** and clone your fork locally:
```bash
git clone https://github.com/RinSucksAtCoding/C-Shell.git
cd C-Shell
```

2. **Set up your environment**
Ensure you have either `clang` or `gcc` installed.

3. **Create a new branch for your feature or bug fix:
```bash
git checkout -b feature/your-feature-here
```

## 🛠️ How to Add a New Built-In Command

If you want to add a built-in command, follow these steps:
1. **Handle the command inside `run_pipeline()`** before the pipe/fork executes:
```C
if (strcmp(first_args[0], "mycmd") == 0) {
    // Your command goes here
    free(pipe_str_copy);
    return 0;
}
```
2. **Manage memory responsibly ** Make sure you free any temporary buffers, strings, or copy pointers before returning to prevent memory leaks.
3. **Test it thoroughly** both directly in the terminal and inside a `.cshellrc` startup script or pipeline.

## 💻 Code Style Guidelines

* **Language Standard:** Written in clean, readable **C99**.
* **Memory Safety ** Always check for `NULL` pointers after `malloc`, `strdup`, or `getenv`. Free dynamically allocated memory when it's no longer needed (especially in alias management, history tracking, and argument parsing).
* **Readability:** Keep functions modular, comment complex parsing or piping logic, and use meaningful variable names.

## 🐞 Reporting Bugs & Issues

If you encounter a bug (e.g., pipe deadlocks, quote parsing edge cases, memory leaks), please open an issue on GitHub with:
* A clear description of what the bug does.
* Steps to reproduce it.
* Your environment details (OS, Termux version, Clang/GCC version).

## Pull Request Process

1. Compile and test your changes locally to ensure there are no compilation warnings or runtime crashes:
```bash
clang c-shell.c -o c-shell -lreadline
./c-shell
```
2. Push your branch and open a **Pull Request** targeting the `main` branch.
3. Provide a clear summary of what your PR changes and why it's useful.
