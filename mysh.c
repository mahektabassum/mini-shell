#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
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

        /* 5. Ignore empty input, handle the built-in "exit" */
        if (args[0] == NULL) continue;
        if (strcmp(args[0], "exit") == 0) break;

        /* 5b. Built-in "cd": must run in the parent, not a child */
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) args[1] = getenv("HOME");
            if (chdir(args[1]) != 0) perror("cd");
            continue;
        }

        /* 5c. Find "<" and ">" and remove them (and the file name) from args */
        char *infile = NULL, *outfile = NULL;
        int bad = 0;
        for (int j = 0; args[j] != NULL; ) {
            int is_out = strcmp(args[j], ">") == 0;
            int is_in  = strcmp(args[j], "<") == 0;
            if (!is_out && !is_in) { j++; continue; }
            if (args[j + 1] == NULL) {
                fprintf(stderr, "mysh: missing file name after %s\n", args[j]);
                bad = 1;
                break;
            }
            if (is_out) outfile = args[j + 1];
            else        infile  = args[j + 1];
            /* shift the remaining words left by 2 */
            for (int k = j; ; k++) {
                args[k] = args[k + 2];
                if (args[k] == NULL) break;
            }
        }
        if (bad) continue;
        if (args[0] == NULL) continue;

        /* 6. Create a child process to run the command */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            /* Point stdin/stdout at files if redirection was requested */
            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { perror(infile); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(outfile); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execvp(args[0], args);   /* only returns if it failed */
            perror(args[0]);
            exit(1);
        }

        /* 7. Parent waits for the child to finish */
        waitpid(pid, NULL, 0);
    }
    return 0;
}