#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

extern char **environ;

pid_t foreground_pid = -1;

void handle_sigint(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    } else {
        write(STDOUT_FILENO, "\n", 1);
    }
}

void handle_sigalrm(int sig) {
    if (foreground_pid > 0) {
        printf("\nProcess timed out after 10 seconds. Terminating.\n");
        kill(foreground_pid, SIGKILL);
    }
}

int tokenize(char *cmdline, char **args, int *is_background, char **outfile) {
    int i = 0;
    *is_background = 0;
    *outfile = NULL;

    char *token = strtok(cmdline, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        if (strcmp(token, "&") == 0) {
            *is_background = 1;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            if (token != NULL) {
                *outfile = token;
            }
        } else {
            if (token[0] == '$') {
                char *env_val = getenv(token + 1);
                if (env_val != NULL) {
                    args[i] = env_val;
                } else {
                    args[i] = "";
                }
            } else {
                args[i] = token;
            }
            i++;
        }
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

int main() {
    char cmdline[MAX_LINE];
    char *args[MAX_ARGS];
    char cwd[MAX_LINE];
    int is_background;
    char *outfile;

    // Register signal handlers
    signal(SIGINT, handle_sigint);
    signal(SIGALRM, handle_sigalrm);

    while (1) {
        foreground_pid = -1;

        // Print prompt: <cwd> >[cite: 1]
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s> ", cwd);
        } else {
            printf("shell> ");
        }
        fflush(stdout);

        // Read input
        if (fgets(cmdline, MAX_LINE, stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            continue;
        }

        // Remove trailing newline
        cmdline[strcspn(cmdline, "\n")] = 0;

        // Skip empty lines
        if (strlen(cmdline) == 0) {
            continue;
        }

        // Tokenize input and extract background/redirection flags[cite: 1]
        int argc = tokenize(cmdline, args, &is_background, &outfile);
        if (argc == 0) continue;

        // Handle Built-in Commands[cite: 1]
        if (strcmp(args[0], "exit") == 0) {
            exit(0);
        }
        else if (strcmp(args[0], "pwd") == 0) {
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            }
        }
        else if (strcmp(args[0], "cd") == 0) {
            if (args[1] != NULL) {
                if (chdir(args[1]) != 0) {
                    perror("cd failed");
                }
            } else {
                chdir(getenv("HOME"));
            }
        }
        else if (strcmp(args[0], "echo") == 0) {
            for (int j = 1; j < argc; j++) {
                printf("%s ", args[j]);
            }
            printf("\n");
        }
        else if (strcmp(args[0], "env") == 0) {
            if (args[1] == NULL) {
                for (int j = 0; environ[j] != NULL; j++) {
                    printf("%s\n", environ[j]);
                }
            } else {
                char *val = getenv(args[1]);
                if (val != NULL) {
                    printf("%s\n", val);
                }
            }
        }
        else if (strcmp(args[0], "setenv") == 0) {
            if (args[1] != NULL) {
                char *eq = strchr(args[1], '=');
                if (eq != NULL) {
                    *eq = '\0';
                    setenv(args[1], eq + 1, 1);
                } else if (args[2] != NULL) {
                    setenv(args[1], args[2], 1);
                }
            }
        }
        else {
            // External Commands, Background, Redirection, and Timeout Handling[cite: 1]
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
            } else if (pid == 0) {
                // Child process
                if (outfile != NULL) {
                    int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) {
                        perror("open failed");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                if (execvp(args[0], args) < 0) {
                    perror("execvp() failed");
                    exit(1);
                }
            } else {
                // Parent process
                if (!is_background) {
                    foreground_pid = pid;
                    alarm(10); // Task 5: Set 10-second timer for foreground process[cite: 1]
                    int status;
                    waitpid(pid, &status, 0);
                    alarm(0);  // Cancel alarm if process finishes within limit[cite: 1]
                    foreground_pid = -1;
                } else {
                    // Task 3: Background process[cite: 1]
                    printf("[Background PID: %d]\n", pid);
                }
            }
        }
    }
    return 0;
}