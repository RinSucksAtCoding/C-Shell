#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <glob.h>
#include <time.h>

// Max command line limits
#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_ALIASES 50

// ANSI color codes
#define COLOR_BLACK "\x1b[1;30m"
#define COLOR_RED "\x1b[1;31m"
#define COLOR_GREEN "\x1b[1;32m"
#define COLOR_YELLOW "\x1b[1;33m"
#define COLOR_BLUE "\x1b[1;34m"
#define COLOR_MAGENTA "\x1b[1;35m"
#define COLOR_CYAN "\x1b[1;36m"
#define COLOR_WHITE "\x1b[1;37m"
#define COLOR_RESET "\x1b[0m"

// Set up command history
char *history[MAX_HISTORY];
int history_count = 0;

// Set up aliases
typedef struct {
    char *name;
    char *command;
} Alias;

Alias aliases[MAX_ALIASES];
int alias_count = 0;

volatile pid_t foreground_pid = 0;

void print_prompt(void) {
    char cwd[MAX_LINE];
    if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "unknown");

    char *username = getenv("USER");
    if (username == NULL) username = "user";

    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    // Basename of current working directory (e.g. 'projects' instead of '/sdcard/projects')
    char *pwd_base = strrchr(cwd, '/');
    pwd_base = (pwd_base != NULL && *(pwd_base + 1) != '\0') ? pwd_base + 1 : cwd;

    char *ps1 = getenv("PS1");
    if (ps1 == NULL) {
        // If PS1 is not set, fallback to this colorful, default prompt
        printf("%sC-Shell%s as %s%s%s@%s%s%s in %s%s%s $ ",
        COLOR_GREEN, COLOR_RESET,
        COLOR_YELLOW, username, COLOR_RESET,
        COLOR_GREEN, hostname, COLOR_RESET,
        COLOR_CYAN, pwd_base, COLOR_RESET);
    } else {
        // Parse basic escape sequences in PS1
        for (int i = 0; ps1[i] != '\0'; i++) {
            if (ps1[i] == '\\' && ps1[i + 1] != '\0') {
                i++;
                switch (ps1[i]) {
                    // System info escapes
                    case 'u': printf("%s", username); break;
                    case 'h': printf("%s", hostname); break;
                    case 'w': printf("%s", cwd); break;
                    case 'W': printf("%s", pwd_base); break;
                    case 'n': printf("\n"); break;
                    case '$': printf("$"); break;

                    // Color & formatting shortcuts
                    case 'e': printf("\x1b"); break; // Standard escape character
                    case 'K': printf("%s", COLOR_BLACK);
                    case 'R': printf("%s", COLOR_RED); break;
                    case 'G': printf("%s", COLOR_GREEN); break;
                    case 'Y': printf("%s", COLOR_YELLOW); break;
                    case 'B': printf("%s", COLOR_BLUE); break;
                    case 'M': printf("%s", COLOR_MAGENTA); break;
                    case 'C': printf("%s", COLOR_CYAN); break;
                    case 'H': printf("%s", COLOR_WHITE); break;
                    case 'X': printf("%s", COLOR_RESET); break;

                    default: putchar('\\'); putchar(ps1[i]); break;
                }
            } else {
                putchar(ps1[i]);
            }
        }
    }
    fflush(stdout);
}

void handle_sigchld(int sig) {
    (void)sig;

    // Reap all dead background child processes without blocking
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Handle CTRL + C without crashing
void handle_sigint(int sig) {
    (void)sig;

    // Kill the foreground activity if it exists, otherwise just print a new line
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    } else {
        printf("\n");
        print_prompt();
    }
}

void add_to_history(const char *cmd) {
    if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0) {
        return;
    }

    char *copy = strdup(cmd);
    if (copy == NULL) {
        perror("strdup");
        return;
    }

    if (history_count < MAX_HISTORY) {
        history[history_count++] = copy;
    } else {
        free(history[0]);

        for (int i = 1; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }

        history[MAX_HISTORY - 1] = copy;
    }
}


void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}

void load_history(void) {
    char path[MAX_LINE];
    char *home = getenv("HOME");
    if (home == NULL) return;

    snprintf(path, sizeof(path), "%s/.cshell_history", home);
    FILE *file = fopen(path, "r");
    if (file == NULL) return; // Safely ignore the history file if it doesn't exist yet

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Strip trailing newline
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strlen(line) > 0) {
            add_to_history(line);
        }
    }
    fclose(file);
}

void save_history(void) {
    char path[MAX_LINE];
    char *home = getenv("HOME");
    if (home == NULL) return;

    snprintf(path, sizeof(path), "%s/.cshell_history", home);
    FILE *file = fopen(path, "w");
    if (file == NULL) return;

    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s\n", history[i]);
    }

    fclose(file);
}

void add_alias(const char *name, const char *command) {
    // Check if alias already exists, update if so
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].command);
            aliases[i].command = strdup(command);
            return;
        }
    }
    if (alias_count < MAX_ALIASES) {
        aliases[alias_count].name = strdup(name);
        aliases[alias_count].command = strdup(command);
        alias_count++;
    }
}

const char *get_alias(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            return aliases[i].command;
        }
    }
    return NULL;
}

// Free aliases on exit
void free_aliases(void) {
    for (int i = 0; i < alias_count; i++) {
        free(aliases[i].name);
        free(aliases[i].command);
    }
}

int expand_env_vars(const char *input, char *output, size_t output_size) {
    size_t used = 0;

    if (output_size == 0) {
        return -1;
    }

    while (*input != '\0') {
        if (*input != '$') {
            if (used + 1 >= output_size) {
                output[0] = '\0';
                return -1;
            }

            output[used++] = *input++;
            continue;
        }

        input++;  // Skip '$'

        char var_name[MAX_LINE];
        size_t name_length = 0;

        while ((*input >= 'A' && *input <= 'Z') ||
               (*input >= 'a' && *input <= 'z') ||
               (*input >= '0' && *input <= '9') ||
               *input == '_') {
            if (name_length + 1 >= sizeof(var_name)) {
                output[0] = '\0';
                return -1;
            }

            var_name[name_length++] = *input++;
        }

        // A '$' not followed by a variable name is kept literally.
        if (name_length == 0) {
            if (used + 1 >= output_size) {
                output[0] = '\0';
                return -1;
            }

            output[used++] = '$';
            continue;
        }

        var_name[name_length] = '\0';

        const char *value = getenv(var_name);
        if (value != NULL) {
            size_t value_length = strlen(value);

            // Leave room for the final '\0'.
            if (value_length >= output_size - used) {
                output[0] = '\0';
                return -1;
            }

            memcpy(output + used, value, value_length);
            used += value_length;
        }
    }

    output[used] = '\0';
    return 0;
}

void strip_quotes(char *str) {
    int len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

void execute_sub_command(char **args) {
    char *input_file = NULL;
    char *output_file = NULL;
    int clean_count = 0;
    char *clean_args[MAX_ARGS];

    for (int j = 0; args[j] != NULL; j++) {
        if (strcmp(args[j], "<") == 0) {
            if (args[j + 1] != NULL) { input_file = args[j + 1]; j++; }
        }
        else if (strcmp(args[j], ">") == 0) {
            if (args[j + 1] != NULL) { output_file = args[j + 1]; j++; }
        }
        else {
            strip_quotes(args[j]);

            // --- WILDCARD / GLOBBING EXPANSION ---
            if (strchr(args[j], '*') != NULL || strchr(args[j], '?') != NULL) {
                glob_t glob_result;
                // GLOB_NOCHECK keeps the pattern literal if no matching files are found
                if (glob(args[j], GLOB_NOCHECK, NULL, &glob_result) == 0) {
                    for (size_t g = 0; g < glob_result.gl_pathc; g++) {
                        if (clean_count < MAX_ARGS - 1) {
                            // Duplicate the string so it survives globfree()
                            clean_args[clean_count++] = strdup(glob_result.gl_pathv[g]);
                        }
                    }
                    globfree(&glob_result);
                    continue;
                }
            }
            // -------------------------------------

            clean_args[clean_count++] = args[j];
        }
    }
    clean_args[clean_count] = NULL;

    if (clean_count == 0) exit(0);      

    if (input_file != NULL) {
        int fd_in = open(input_file, O_RDONLY);
        if (fd_in < 0) { perror("Input redirection failed"); exit(1); }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("dup2 input");
            close(fd_in);
            exit(1);
        }
        close(fd_in);
    }

    if (output_file != NULL) {
        int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) { perror("Output redirection failed..."); exit(1); }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd_out);
            exit(1);
        }
        close(fd_out);
    }

    execvp(clean_args[0], clean_args);
    perror(clean_args[0]);
    exit(1);
}

// Parses a command string into arguments while respecting single and double quotes
int parse_arguments(char *str, char **args, int max_args) {
    int count = 0;
    int in_quotes = 0;
    char quote_char = '\0';
    char *p = str;

    while (*p != '\0' && count < max_args - 1) {
        // Skip leading spaces
        while (*p == ' ' && !in_quotes) p++;
        if (*p == '\0') break;

        args[count++] = p; // Start of argument

        while (*p != '\0') {
            if (!in_quotes && (*p == '\'' || *p == '"')) {
                in_quotes = 1;
                quote_char = *p;
                p++;
            } else if (in_quotes && *p == quote_char) {
                in_quotes = 0;
                p++;
            } else if (!in_quotes && *p == ' ') {
                *p = '\0'; // Null-terminate the argument
                p++;
                break;
            } else {
                p++;
            }
        }
    }
    args[count] = NULL;
    return count;
}

int run_pipeline(char *pipeline_str) {
    char *commands[MAX_ARGS];
    int num_cmds = 0;
    char *pipe_str_copy = strdup(pipeline_str);
    char *pipe_save = NULL;
    char *cmd_token = strtok_r(pipe_str_copy, "|", &pipe_save);

    // 1. Split pipeline commands by '|'
    while (cmd_token != NULL && num_cmds < MAX_ARGS - 1) {
        commands[num_cmds++] = cmd_token;
        cmd_token = strtok_r(NULL, "|", &pipe_save);
    }
    if (cmd_token != NULL) {
        fprintf(stderr, "Too many commands in pipeline\n");
        free(pipe_str_copy);
        return 1;
    }
    if (num_cmds == 0) { free(pipe_str_copy); return 0; }
    commands[num_cmds] = NULL;

    // Parse arguments for the first command in the pipeline
    char *first_args[MAX_ARGS];
    char temp_first[MAX_LINE];
    strcpy(temp_first, commands[0]);
    int arg_idx = parse_arguments(temp_first, first_args, MAX_ARGS);

    if (arg_idx == 0) { free(pipe_str_copy); return 0; }

    if (strcmp(first_args[0], "exit") == 0) {
        save_history();
        free_aliases();
        printf("Goodbye!\n");
        free(pipe_str_copy);
        exit(0);
    }
    if (strcmp(first_args[0], "history") == 0) { print_history(); free(pipe_str_copy); return 0; }
    if (strcmp(first_args[0], "cd") == 0) {
        char *target = (arg_idx > 1) ? first_args[1] : getenv("HOME");
        int ret = 0;

        if (target == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            free(pipe_str_copy);
            return 1;
        }

        if (chdir(target) != 0) { perror("cd"); ret = 1; }
        free(pipe_str_copy);
        return ret;
    }
    if (strcmp(first_args[0], "pwd") == 0) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
        free(pipe_str_copy);
        return 0;
    }
    if (strcmp(first_args[0], "alias") == 0) {
        if (arg_idx == 1) {
            // Print all registered aliases
            for (int i = 0; i < alias_count; i++) {
                printf("alias %s='%s'\n", aliases[i].name, aliases[i].command);
            }
        } else {
            // Reconstruct the raw argument string after 'alias'
            char full_arg[MAX_LINE] = "";
            for (int k = 1; k < arg_idx; k++) {
                strcat(full_arg, first_args[k]);
                if (k < arg_idx - 1) strcat(full_arg, " ");
            }

            char *eq = strchr(full_arg, '=');
            if (eq != NULL) {
                *eq = '\0';
                char name[MAX_LINE];
                char cmd[MAX_LINE];
                strcpy(name, full_arg);
                strcpy(cmd, eq + 1);

                strip_quotes(name);
                strip_quotes(cmd); // Strips outer quotes cleanly (e.g. 'ls -A' -> ls -A)

                add_alias(name, cmd);
            } else {
                fprintf(stderr, "alias: usage: alias name='command'\n");
            }
        }
        free(pipe_str_copy);
        return 0;
    }
    if (strcmp(first_args[0], "export") == 0) {
        if (arg_idx < 2) {
            fprintf(stderr, "export: usage: export KEY=VALUE\n");
        } else {
            // Reconstruct the full argument string in case of spaces
            char full_arg[MAX_LINE] = "";
            for (int k = 1; k < arg_idx; k++) {
                strcat(full_arg, first_args[k]);
                if (k < arg_idx - 1) strcat(full_arg, " ");
            }

            char *eq = strchr(full_arg, '=');
            if (eq != NULL) {
                *eq = '\0';
                char name[MAX_LINE];
                char value[MAX_LINE];
                strcpy(name, full_arg);
                strcpy(value, eq + 1);

                strip_quotes(name);
                strip_quotes(value);

                // Set the environment variable [1 = overwrite if it already exists]
                if (setenv(name, value, 1) != 0) perror("export");
            } else {
                fprintf(stderr, "export: usage: export KEY=VALUE\n");
            }
        }
        free(pipe_str_copy);
        return 0;
    }

    int pipefds[2 * (MAX_ARGS - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");

            // Close pipes that were successfully created earlier
            for (int j = 0; j < 2 * i; j++) {
                close(pipefds[j]);
            }

            free(pipe_str_copy);
            return 1;
        }
    }

    int is_background = 0;

    char last_cmd_copy[MAX_LINE];
    strcpy(last_cmd_copy, commands[num_cmds - 1]);
    char *last_args[MAX_ARGS];
    int last_arg_count = parse_arguments(last_cmd_copy, last_args, MAX_ARGS);

    if (last_arg_count > 0 && strcmp(last_args[last_arg_count - 1], "&") == 0) {
        is_background = 1;
        // Strip the '&' character from the actual command string
        char *amp = strchr(commands[num_cmds - 1], '&');
        if (amp) *amp = '\0';
    }

    int last_status = 0;
    int fork_failed = 0;

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (i > 0 && dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) {
                perror("dup2 stdin");
                _exit(1);
            }

            if (i < num_cmds - 1 && dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
                perror("dup2 stdout");
                _exit(1);
            }

            for (int j = 0; j < 2 * (num_cmds - 1); j++) close(pipefds[j]);

            char *args[MAX_ARGS];
            char cmd_copy[MAX_LINE];
            strcpy(cmd_copy, commands[i]);
            parse_arguments(cmd_copy, args, MAX_ARGS);
            execute_sub_command(args);
        } else if (pid < 0) {
            perror("fork");
            fork_failed = 1;
            break;
        } else {
            if (i == num_cmds - 1) { foreground_pid = pid; }
        }
    }

    for (int j = 0; j < 2 * (num_cmds - 1); j++) close(pipefds[j]);

    if (fork_failed) {
        last_status = 1;
    } else if (is_background) {
        // -- BACKGROUND PROCESS --
        printf("[Background PID: %d]\n", foreground_pid);
        fflush(stdout);
        // Do NOT block waiting for children; SIGCHLD will reap them later
        last_status = 0;
    } else {
        for (int i = 0; i < num_cmds; i++) {
            int status;
            pid_t reaped;
            do {
                reaped = wait(&status);
            } while (reaped < 0 && errno == EINTR);

            if (reaped == foreground_pid) {
                if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
                else last_status = 1;
            }
        }
    }

    foreground_pid = 0;
    free(pipe_str_copy);
    return last_status;
}

int run_command_string(char *cmd_str) {
    char expanded_line[MAX_LINE];

    if (expand_env_vars(cmd_str, expanded_line, sizeof(expanded_line)) != 0) {
        fprintf(stderr, "Expanded command is too long!\n");
        return 1;
    }

    // -- Check for alias expansion --
    char alias_checked[MAX_LINE];
    // Copy first word (command line)
    char first_word[MAX_LINE];
    int i = 0;
    while (expanded_line[i] != ' ' && expanded_line[i] && i < MAX_LINE - 1) {
        first_word[i] = expanded_line[i];
        i++;
    }
    first_word[i] = '\0';

    const char *alias_cmd = get_alias(first_word);
    if (alias_cmd != NULL) {
        // Replace alias with its full command value + its remaining arguments
        snprintf(alias_checked, sizeof(alias_checked), "%s%s", alias_cmd, expanded_line + i);
        strcpy(expanded_line, alias_checked);
    }

    char *pipelines[MAX_ARGS];
    char operators[MAX_ARGS];
    int pipeline_count = 0;

    char *ptr = expanded_line;
    char *start = expanded_line;
    while (*ptr) {
        if (strncmp(ptr, "&&", 2) == 0) {
            if (pipeline_count >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many command segments!\n");
                return 1;
            }

            *ptr = '\0';
            operators[pipeline_count] = '&';
            pipelines[pipeline_count++] = start;
            ptr += 2;
            start = ptr;
        } else if (strncmp(ptr, "||", 2) == 0) {
            if (pipeline_count >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many command segments!\n");
                return 1;
            }

            *ptr = '\0';
            operators[pipeline_count] = '|';
            pipelines[pipeline_count++] = start;
            ptr += 2;
            start = ptr;
        } else {
            ptr++;
        }
    }
    pipelines[pipeline_count++] = start;

    int status = run_pipeline(pipelines[0]);

    for (int i = 1; i < pipeline_count; i++) {
        // Stop the command from executing if the last one failed
        if (operators[i - 1] == '&' && status != 0) break;

        // Stop the command from executing if the last one succeeded
        if (operators[i - 1] == '|' && status == 0) break;
        status = run_pipeline(pipelines[i]);
    }

    return status;
}

void load_config(void) {
    char path[MAX_LINE];
    char *home = getenv("HOME");
    if (home == NULL) return;

    snprintf(path, sizeof(path), "%s/.cshellrc", home);
    FILE *file = fopen(path, "r");
    if (file == NULL) return; // Safely ignore the configuration file if it does not exist

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Strip trailing newline
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        // Skip comments or empty lines
        if (line[0] == '#' || strlen(line) == 0) continue;

        // Execute each line as if it were typed into the shell
        run_command_string(line);
    }
    fclose(file);
}
              
int main(void) {
    // Ensure Termux binaries (like curl, micro, etc.) and Android system binaries are in PATH
    char *current_path = getenv("PATH");
    char new_path[MAX_LINE];

    if (current_path != NULL) {
        snprintf(new_path, sizeof(new_path), "%s:/data/data/com.termux/files/usr/bin:/system/bin", current_path);
    } else {
        snprintf(new_path, sizeof(new_path), "/data/data/com.termux/files/usr/bin:/system/bin");
    }
    setenv("PATH", new_path, 1);

    char raw_line[MAX_LINE];
    char working_line[MAX_LINE];

    load_config(); // Load user aliases/config
    load_history(); // Load past command history
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    while (1) {
        // Print the customizable prompt
        print_prompt();

        // Handle CTRL + D and exit shsll
        if (fgets(raw_line, sizeof(raw_line), stdin) == NULL) {
            save_history();
            printf("\nExiting...\n");
            break;
        }

        char *newline = strchr(raw_line, '\n');

        if (newline != NULL) {
            *newline = '\0';
        } else {
            // No newline fit in the buffer. Check weather the line continues.
            int ch = getchar();

            if (ch != '\n' && ch != EOF) {
                while (ch != '\n' && ch != EOF) {
                    ch = getchar();
                }

                fprintf(stderr, "Command line is too long! (maximum %d characters)\n", MAX_LINE - 1);
                continue;
            }
        }

        if (strlen(raw_line) == 0) continue;
        strcpy(working_line, raw_line);

        // 1. Start the timer RIGHT BEFORE running a command
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        int status = run_command_string(working_line);

        // 2. Stop the timer RIGHT AFTER the command finishes
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        // Calculate elapsed time in seconds
        double elapsed = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

        // Print if it took longer than 0.1 second
        if (elapsed > 0.5) {
            printf("\033[33m[took %.3fs]\033[0m\n", elapsed);
        }

        // Add the command to history only if it SUCCEEDED
        if (status == 0) {
            add_to_history(raw_line);
        }
    }

    // Clear all history after exiting
    for (int i = 0; i < history_count; i++) free(history[i]);
    free_aliases();
    return 0;
}
