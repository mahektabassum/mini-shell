#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

        /* 6. Create a child process to run the command */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            execvp(args[0], args);   /* only returns if it failed */
            perror(args[0]);
            exit(1);
        }

        /* 7. Parent waits for the child to finish */
        waitpid(pid, NULL, 0);
    }
    return 0;
}