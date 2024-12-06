#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


// Function to split a command into an array of arguments
char **split_command(const char *cmd) {
    char **args = malloc(256 * sizeof(char *));
    if (!args) {
        perror("malloc failed");
        exit(1);
    }
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        perror("strdup failed");
        exit(1);
    }

    char *token = strtok(cmd_copy, " ");
    int i = 0;
    while (token) {
        args[i++] = strdup(token);
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // NULL-terminate the array
    free(cmd_copy);
    return args;
}

// Function to resolve the absolute path of a command
char *resolve_path(const char *cmd) {
    if (cmd[0] == '/') {
        return strdup(cmd); // Absolute path already
    }

    char *path_env = getenv("PATH");
    if (!path_env) {
        perror("PATH environment variable not set");
        exit(1);
    }

    char *paths = strdup(path_env);
    if (!paths) {
        perror("strdup failed");
        exit(1);
    }

    char *token = strtok(paths, ":");
    char abs_path[PATH_MAX];
    while (token) {
        snprintf(abs_path, PATH_MAX, "%s/%s", token, cmd);
        if (access(abs_path, X_OK) == 0) { // Check if executable
            free(paths);
            return strdup(abs_path);
        }
        token = strtok(NULL, ":");
    }

    free(paths);
    fprintf(stderr, "Command not found: %s\n", cmd);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <inp> <cmd> <out>\n", argv[0]);
        return 1;
    }

    char *input_file = argv[1];
    char *command = argv[2];
    char *output_file = argv[3];

    // Handle input redirection
    int input_fd = STDIN_FILENO;
    if (strcmp(input_file, "-") != 0) {
        input_fd = open(input_file, O_RDONLY);
        if (input_fd < 0) {
            perror("Failed to open input file");
            return 1;
        }
    }

    // Handle output redirection
    int output_fd = STDOUT_FILENO;
    if (strcmp(output_file, "-") != 0) {
        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            perror("Failed to open output file");
            return 1;
        }
    }

    // Split command into arguments
    char **args = split_command(command);

    // Resolve command path
    char *abs_path = resolve_path(args[0]);

    // Fork and exec the command
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) { // Child process
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        execv(abs_path, args);
        perror("execv failed"); // If exec fails
        exit(1);
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);

    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);

    // Free allocated memory
    free(abs_path);
    for (int i = 0; args[i]; i++) {
        free(args[i]);
    }
    free(args);

    return WEXITSTATUS(status);
}
