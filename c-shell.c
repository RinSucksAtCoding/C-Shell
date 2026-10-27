#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_ALIASES 20

#define COLOR_GREEN "\x1b[1;32m"
#define COLOR_CYAN "\x1b[1;36m"
#define COLOR_RESET "\x1b[0m"

char *history[MAX_HISTORY];
int history_count = 0;

volatile pid_t foreground_pid = 0;

void handle_sigint(int sig) {
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
    if (history_count < MAX_HISTORY) {
        history[history_count++] = strdup(cmd);
    } else {
        free(history[0]);
        for (int i = 0; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }
        history[MAX_HISTORY - 1] = strdup(cmd);
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}


void expand_env_vars(char *input, char *output) {
    char *src = input;
    char *dst = output;

    while (*src) {
        if (*src == '$') {
            src++;
            char var_name[64];
            int i = 0;
            while (*src && ((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z') || (*src >= '0' && *src <= '9') || *src == '_')) {
                var_name[i++] = *src++;
            }
            var_name[i] = '\0';
            char *value = getenv(var_name);
            if (value) {
                strcpy(dst, value);
                dst += strlen(value);
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
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
        int fd_in = open(input_file, O_RDONLY);
        if (fd_in < 0) { perror("Input redirection failed..."); exit(1); }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }

    if (output_file != NULL) {
        int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd_out < 0) { perror("Output redirection failed..."); exit(1); }
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }

    execvp(clean_args[0], clean_args);
    fprintf(stderr, "%s: command not found\n", clean_args[0]);
    exit(1);
}

int run_pipeline(char *pipeline_str) {
    char *commands[MAX_ARGS];
    int num_cmds = 0;

    char *pipe_str_copy = strdup(pipeline_str);
    char *cmd_token = strtok(pipe_str_copy, "|");
    while (cmd_token != NULL && num_cmds < MAX_ARGS - 1) {
        commands[num_cmds++] = cmd_token;
        cmd_token = strtok(NULL, "|");
    }
    commands[num_cmds] = NULL;

    char *first_args[MAX_ARGS];
    char temp_first[MAX_LINE];
    strcpy(temp_first, commands[0]);
    int arg_idx = 0;
    char *token = strtok(temp_first, " ");
    while (token != NULL) {
        first_args[arg_idx++] = token;
        token = strtok(NULL, " ");
    }
    first_args[arg_idx] = NULL;

    if (arg_idx == 0) { free(pipe_str_copy); return 0; }

    if (strcmp(first_args[0], "exit") == 0) { printf("See you later!\n"); free(pipe_str_copy); exit(0); }
    if (strcmp(first_args[0], "history") == 0) { print_history(); free(pipe_str_copy); return 0; }
    if (strcmp(first_args[0], "cd") == 0) {
        char *target = (arg_idx > 1) ? first_args[1] : getenv("HOME");
        int ret = 0;
        if (chdir(target) != 0) { perror("cd failed"); ret = 1; }
        free(pipe_str_copy);
        return ret;
    }

    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) { perror("Pipe failed"); free(pipe_str_copy); return 1; }
    }

    int last_status = 0;
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (i > 0) dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            if (i < num_cmds - 1) dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            for (int j = 0; j < 2 * (num_cmds - 1); j++) close(pipefds[j]);

            char *args[MAX_ARGS];
            int k = 0;
            char *arg_tok = strtok(commands[i], " ");
            while (arg_tok != NULL) { args[k++] = arg_tok; arg_tok = strtok(NULL, " "); }
            args[k] = NULL;

            execute_sub_command(args);
        } else {
            if (i == num_cmds - 1) foreground_pid = pid;
        }
    }

    for (int j = 0; j < 2 * (num_cmds - 1); j++) close(pipefds[j]);

    for (int i = 0; i < num_cmds; i++) {
        int status;
        pid_t reaped = wait(&status);
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
        
    expand_env_vars(cmd_str, expanded_line);

    char *pipelines[MAX_ARGS];
    char operators[MAX_ARGS];
    int pipeline_count = 0;

    char *ptr = expanded_line;
    char *start = expanded_line;
    while (*ptr) {
        if (strncmp(ptr, "&&", 2) == 0) {
            *ptr = '\0';
            operators[pipeline_count] = '&';
            pipelines[pipeline_count++] = strdup(start);
            ptr += 2;
            start = ptr;
        } else if (strncmp(ptr, "||", 2) == 0) {
            *ptr = '\0';
            operators[pipeline_count] = '|';
            pipelines[pipeline_count++] = strdup(start);
            ptr += 2;
            start = ptr;
        } else {
            ptr++;
        }
    }
    pipelines[pipeline_count++] = strdup(start);

    int status = run_pipeline(pipelines[0]);

    for (int i = 1; i < pipeline_count; i++) {
        if (operators[i - 1] == '&' && status != 0) break;
            if (operators[i - 1] == '|' && status == 0) break;
                status = run_pipeline(pipelines[i]);
            }
    for (int i = 0; i < pipeline_count; i++) free(pipelines[i]);
    return status;
}

int main() {
    char raw_line[MAX_LINE];
    char working_line[MAX_LINE];

    signal(SIGINT, handle_sigint);

    while (1) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "unknown");
        char *pwd = strrchr(cwd, '/');
        pwd = (pwd != NULL && *(pwd + 1) != '\0') ? pwd + 1 : cwd;
        printf("$ %s%s%s | %sC-Shell%s > ", COLOR_CYAN, pwd, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
        fflush(stdout);

        if (fgets(raw_line, sizeof(raw_line), stdin) == NULL) {
            printf("\nExiting...\n");
            break;
        }
        raw_line[strcspn(raw_line, "\n")] = '\0';

        if (strlen(raw_line) == 0) continue;

        strcpy(working_line, raw_line);

        int status = run_command_string(working_line);
        if (status == 0) {
            add_to_history(raw_line);
        }
    }

    for (int i = 0; i < history_count; i++) free(history[i]);

    return 0;
}