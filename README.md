\# mini-shell



A small Unix shell written in C. It reads commands, runs programs, and supports the features you use every day: built-in `cd`, input/output redirection, pipes, and background jobs.



Built to understand how a real shell works with processes, file descriptors, and system calls.



\## Features



| Feature | Example |

|---|---|

| Run any program | `ls -l`, `echo hello` |

| Built-in `cd` and `exit` | `cd ..`, `cd` (goes to home) |

| Output redirection | `ls > files.txt` |

| Input redirection | `wc -l < files.txt` |

| Pipes (any number of stages) | `ls \\| sort \\| head -3` |

| Pipes combined with redirection | `echo hi \\| cat > out.txt` |

| Background jobs | `sleep 10 \&` |



\## Build and run



Requires Linux (or WSL on Windows) and GCC.



```

gcc -Wall -o mysh mysh.c

./mysh

```



Example session:



```

mysh> ls | wc -l

4

mysh> echo hello > out.txt

mysh> cat < out.txt

hello

mysh> sleep 5 \&

\[4821]

mysh> exit

```



Type `exit` or press Ctrl+D to quit.



\## How it works



1\. \*\*Read and parse.\*\* The shell reads a line and splits it into words with `strtok`.

2\. \*\*Built-ins.\*\* `cd` and `exit` run inside the shell itself. `cd` must work this way, because a child process changing its own directory would not affect the shell.

3\. \*\*Run commands.\*\* For everything else the shell calls `fork()` to create a child, and the child calls `execvp()` to become the requested program. The parent calls `waitpid()` to wait for it.

4\. \*\*Redirection.\*\* In the child, before `execvp()`, the shell opens the file with `open()` and uses `dup2()` to replace file descriptor 0 (stdin) or 1 (stdout). The program writes to "stdout" as usual, and the output goes to the file.

5\. \*\*Pipes.\*\* For `a | b`, the shell calls `pipe()` to get a read end and a write end. Process `a` has its stdout connected to the write end, and process `b` has its stdin connected to the read end. Unused pipe ends are closed in every process, so the reader sees end-of-file when the writer finishes.

6\. \*\*Background jobs.\*\* A trailing `\&` makes the shell skip `waitpid()` for that job. Before each prompt the shell calls `waitpid(-1, NULL, WNOHANG)` to collect finished jobs, so they don't remain as zombie processes.



\## System calls used



`fork`, `execvp`, `waitpid`, `pipe`, `dup2`, `open`, `close`, `chdir`



\## Limitations



\- Words must be separated by spaces: write `ls | wc -l`, not `ls|wc -l`.

\- No quoting (`"hello world"` is split into two words).

\- No `>>` (append), `\&\&`, `;`, wildcards, or environment variable expansion.

\- No job control commands such as `jobs` or `fg`.



\## Possible improvements



\- Support `>>` and quoted strings

\- Add command history and tab completion

\- Add `jobs` and `fg` for background job control

\- Handle Ctrl+C so it stops the running command instead of the shell

