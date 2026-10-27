### Contributing to C-Shell

First off, thank you for taking the time to contribute! This project is a lightweight exploration of low-level systems programming in C, coded entirely on mobile devices. 

### The Core Challenge: Squash the Bug!

My primary roadmap goal is implementing **Alias Support** and a **`.cshellrc` Configuration File Parser**. 

Currently, these features are omitted from the codebase because my early string parsing attempts triggered severe **Segmentation Faults (core dumped)** during command tokenization. 

### What I Need Help With:

* A memory-safe, lightweight string tracking or hashing mechanism.
* A robust text evaluation routine that can substitute an alias string (e.g., swapping `ll` out for `ls -lh`) within the input buffer *before* passing tokens to `execvp`.
* Bound-checking logic that keeps our compiled binary size near the ~8.3 KB limit.

### Development Workflow

Because this tool is built for mobile ecosystems, I prefer verification inside the target environment: 

1. **Environment:** Test your fixes directly inside **Termux** on Android using `clang`.
2. **Coding Style:** Keep it clean, standard C99/C11, utilizing native POSIX system calls. Avoid adding massive external third-party dependencies to keep the binary small.
3. **Compilation Check:** Always verify your changes compile cleanly without throwing new memory leak hazards: 

```bash
clang -Wall -Wextra -O2 c-shell.c -o c-shell
```

### Submitting Pull Requests

1. Fork the repository and create your feature branch (`git checkout -b feature/squash-alias-bug`).
2. Commit your changes with a clear message explaining how you managed the pointers or array indices safely.
3. Push to the branch and open a Pull Request!
