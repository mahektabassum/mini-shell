#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_CMDS 16

/* Removes "<" and ">" (and their file names) from cmd.
   Returns -1 if a file name is missing. */
static int parse_redirects(char **cmd, char **infile, char **outfile) {
    *infile = NULL;
    *outfile = NULL;
    for (int j = 0; cmd[j] != NULL; ) {
        int is_out = strcmp(cmd[j], ">") == 0;
        int is_in  = strcmp(cmd[j], "<") == 0;
        if (!is_out && !is_in) { j++; continue; }
        if (cmd[j + 1] == NULL) {
            fprintf(stderr, "mysh: missing file name after %s\n", cmd[j]);
            return -1;
        }
        if (is_out) *outfile = cmd[j + 1];
        else        *infile  = cmd[j + 1];
        /* shift the remaining words left by 2 */
        for (int k = j; ; k++) {
            cmd[k] = cmd[k + 2];
            if (cmd[k] == NULL) break;
        }
    }
    return 0;
}

int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        /* 0. Clean up any background jobs that have finished */
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;

        /* 1. Show the prompt */
        printf("mysh> ");
        fflush(stdout);

        /* 2. Read a line (Ctrl+D gives NULL, so we exit) */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* 3. Remove the trailing newline */
        line[strcspn(line, "\n")] = '\0';

        /* 4. Split the line into words: "ls -l" -> {"ls", "-l", NULL} */
        int i = 0;
        char *token = strtok(line, " \t");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }
        args[i] = NULL;

        /* 4b. A trailing "&" means: run in the background */
        int background = 0;
        if (i > 0 && strcmp(args[i - 1], "&") == 0) {
            background = 1;
            args[i - 1] = NULL;
        }

        /* 5. Ignore empty input, handle the built-ins "exit" and "cd" */
        if (args[0] == NULL) continue;
        if (strcmp(args[0], "exit") == 0) break;
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) args[1] = getenv("HOME");
            if (chdir(args[1]) != 0) perror("cd");
            continue;
        }

        /* 6. Split the words into commands at each "|" */
        char **cmds[MAX_CMDS];
        int ncmds = 0;
        int bad = 0;
        cmds[ncmds++] = args;
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], "|") == 0) {
                if (ncmds >= MAX_CMDS) {
                    fprintf(stderr, "mysh: too many commands in pipeline\n");
                    bad = 1;
                    break;
                }
                args[j] = NULL;              /* end the previous command here */
                cmds[ncmds++] = &args[j + 1];
            }
        }
        if (bad) continue;

        /* 7. Handle "<" and ">" for each command, and check for empty ones */
        char *infile[MAX_CMDS], *outfile[MAX_CMDS];
        for (int c = 0; c < ncmds; c++) {
            if (cmds[c][0] == NULL ||
                parse_redirects(cmds[c], &infile[c], &outfile[c]) < 0 ||
                cmds[c][0] == NULL) {
                fprintf(stderr, "mysh: syntax error\n");
                bad = 1;
                break;
            }
        }
        if (bad) continue;

        /* 8. Start every command, connecting them with pipes */
        pid_t pids[MAX_CMDS];
        int started = 0;
        int prev_read = -1;                  /* read end of the previous pipe */

        for (int c = 0; c < ncmds; c++) {
            int fds[2];
            int has_next = (c < ncmds - 1);

            if (has_next && pipe(fds) < 0) {
                perror("pipe");
                break;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                if (has_next) { close(fds[0]); close(fds[1]); }
                break;
            }

            if (pid == 0) {
                /* Child: read from the previous pipe, write to the next one */
                if (prev_read != -1) {
                    dup2(prev_read, STDIN_FILENO);
                    close(prev_read);
                }
                if (has_next) {
                    close(fds[0]);
                    dup2(fds[1], STDOUT_FILENO);
                    close(fds[1]);
                }
                /* A background job must not steal keyboard input */
                if (background && c == 0 && !infile[c]) {
                    int fd = open("/dev/null", O_RDONLY);
                    if (fd >= 0) { dup2(fd, STDIN_FILENO); close(fd); }
                }
                /* Files given with < or > win over the pipe */
                if (infile[c]) {
                    int fd = open(infile[c], O_RDONLY);
                    if (fd < 0) { perror(infile[c]); exit(1); }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (outfile[c]) {
                    int fd = open(outfile[c], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { perror(outfile[c]); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                execvp(cmds[c][0], cmds[c]);  /* only returns if it failed */
                perror(cmds[c][0]);
                exit(1);
            }

            /* Parent: close the pipe ends it no longer needs */
            pids[started++] = pid;
            if (prev_read != -1) close(prev_read);
            if (has_next) {
                close(fds[1]);
                prev_read = fds[0];
            } else {
                prev_read = -1;
            }
        }
        if (prev_read != -1) close(prev_read);

        /* 9. Wait for the children, unless this is a background job */
        if (background) {
            if (started > 0) printf("[%d]\n", (int)pids[started - 1]);
        } else {
            for (int c = 0; c < started; c++) {
                waitpid(pids[c], NULL, 0);
            }
        }
    }
    return 0;
}