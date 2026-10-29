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


// Max command line limits
#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100

// ANSI color codes
#define COLOR_GREEN "\x1b[1;32m"
#define COLOR_CYAN "\x1b[1;36m"
#define COLOR_RESET "\x1b[0m"

// Set up command history
char *history[MAX_HISTORY];
int history_count = 0;

volatile pid_t foreground_pid = 0;

// Handle CTRL + C without crashing
void handle_sigint(int sig) {
    (void)sig;

    // If there's a foreground activity, kill it, otherwise just print a new line
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    } else {
        printf("\n");
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "unknown");
        char *pwd = strrchr(cwd, '/');
        pwd = (pwd != NULL && *(pwd + 1) != '\0') ? pwd + 1 : cwd;
        printf("$ %s%s%s | %sC-Shell%s > ", COLOR_CYAN, pwd, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
        fflush(stdout);
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
            clean_args[clean_count++] = args[j];
        }
    }
    clean_args[clean_count] = NULL;

    if (clean_count == 0) exit(0);
  
    if (input_file != NULL) {
        // Handle input redirection ('<')
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
        // Handle output redirection ('>')
        int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) { perror("Output redirection failed..."); exit(1); }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd_out);
            exit(1);
        }
        close(fd_out);
    }

    // Handle command execution
    execvp(clean_args[0], clean_args);
    perror(clean_args[0]);
    exit(1);
}

int run_pipeline(char *pipeline_str) {
    char *commands[MAX_ARGS];
    int num_cmds = 0;

    char *pipe_str_copy = strdup(pipeline_str);
    char *pipe_save = NULL;
    char *cmd_token = strtok_r(pipe_str_copy, "|", &pipe_save);

    // Handle command piping ('|')
    while (cmd_token != NULL && num_cmds < MAX_ARGS - 1) {
        commands[num_cmds++] = cmd_token;
        cmd_token = strtok_r(NULL, "|", &pipe_save);
    }
    if (cmd_token != NULL) {
        fprintf(stderr, "Too many commands in pipeline\n");
        free(pipe_str_copy);
        return 1;
    }

    if (num_cmds == 0) {
        free(pipe_str_copy);
        return 0;
    }
    commands[num_cmds] = NULL;

    // Handle spaces in command line
    char *first_args[MAX_ARGS];
    char *temp_save = NULL;
    char temp_first[MAX_LINE];
    strcpy(temp_first, commands[0]);
    int arg_idx = 0;
    char *token = strtok_r(temp_first, " ", &temp_save);
    while (token != NULL) {
        first_args[arg_idx++] = token;
        token = strtok_r(NULL, " ", &temp_save);
    }
    first_args[arg_idx] = NULL;

    if (arg_idx == 0) { free(pipe_str_copy); return 0; }

    if (strcmp(first_args[0], "exit") == 0) { printf("Goodbye!\n"); free(pipe_str_copy); exit(0); }
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
            int k = 0;
            char *arg_save = NULL;
            char *arg_tok = strtok_r(commands[i], " ", &arg_save);
            while (arg_tok != NULL) {
                args[k++] = arg_tok; arg_tok = strtok_r(NULL, " ", &arg_save);
            }
            args[k] = NULL;

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
    }

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

int main(void) {
    char raw_line[MAX_LINE];
    char working_line[MAX_LINE];

    signal(SIGINT, handle_sigint);

    while (1) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "unknown");
        // Generates the user's current directory
        char *pwd = strrchr(cwd, '/');
        pwd = (pwd != NULL && *(pwd + 1) != '\0') ? pwd + 1 : cwd;
        // Generates a command line prompt
        printf("$ %s%s%s | %sC-Shell%s > ", COLOR_CYAN, pwd, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
        fflush(stdout);

        // Handle CTRL + D and exit shsll
        if (fgets(raw_line, sizeof(raw_line), stdin) == NULL) {
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

        // Only add commands that succeeded to history
        int status = run_command_string(working_line);
        if (status == 0) {
            add_to_history(raw_line);
        }
    }

    // Clear all history after exiting
    for (int i = 0; i < history_count; i++) free(history[i]);

    return 0;
}
