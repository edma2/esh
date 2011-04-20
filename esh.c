/* esh.c - Eugene's SHell */
/* TODO: Pipes, set environment variables */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

#define DIRMAX          100
#define INPUTMAX        100        
#define MAXTOK          100
#define PATH            "/home/edma2/bin/esh" /* Fake path */

int execcmds(char **cmds[], int n);
int builtin_chdir(char *path);
int builtin_cwd(void);
void splitstring(char *buf, char *toks[], int n);
void splitcmd(char *cmd, char *cmds[], int n);

int main(void) {
        char buf[INPUTMAX];
        char *args[MAXTOK];

        /* Initialization stuff */
        setenv("SHELL", PATH, 1);

        while (1) {
                printf("esh$ ");

                /* Parse input into tokens */
                if (fgets(buf, INPUTMAX, stdin) == NULL)
                        break;
                buf[strlen(buf)-1] = '\0';
                splitstring(buf, args, MAXTOK);

                /* Look at first token */
                if (strcmp(args[0], "cd") == 0) {
                        if (builtin_chdir(args[1]) < 0)
                                fprintf(stderr, "error: builtin cd failed\n");
                } else if (strcmp(args[0], "pwd") == 0) {
                        if (builtin_cwd() < 0) 
                                fprintf(stderr, "error: builtin pwd failed\n");
                } else if (strcmp(args[0], "bye") == 0) {
                        printf("Have a nice day!\n");
                        return 0;
                } else {
                        external_cmd(args);
                }
        }

        return 0;
}

/* 
 * Split piped command into array of commands.
 * Returns redirected or stdout file descriptor.
 */
int parse_input(char *cmd, char *cmds[], int n) {
        int i, j, k;
        char *path;
        int perms;

        cmds[0] = strtok(cmd, "|");
        /* Parse arguments */
        for (i = 1; i < n; i++) {
                if ((cmds[i] = strtok(NULL, "|")) == NULL)
                        break;
        }
        /* Examine last token, count redirection symbols */
        for (j = 0, k = 0; j < strlen(cmds[i-1]); j++) {
                if (cmds[i-1][j] == '>')
                        k++;
        }
        /* Open file for writing */
        if (k > 0) {
                if ((path = strtok(cmd, "> ")) != NULL) {
                        if (k == 1)
                                return creat(path, 0666);
                        if (k == 2)
                                return open(path, O_RDWR|O_CREAT|O_APPEND, 0666);
                        fprintf(stderr, "error: parse error (too many redirection symbols)\n");
                        return -1;
                }
        }
        /* Return stdout by default */
        return 1;
}

/* Split command into tokens */
void parse_cmd(char *cmd, char *toks[], int n) {
        int i;

        toks[0] = strtok(cmd, " ");
        /* Parse arguments */
        for (i = 1; i < n; i++) {
                if ((toks[i] = strtok(NULL, " ")) == NULL)
                        break;
                /* Environment variable */
                if (*toks[i] == '$')
                        toks[i] = getenv(toks[i]+1);
        }
}

int execcmds(char **cmds[], int n, int fd) {
        int i, pid;
        int even[2], odd[2];
        int *p0, *p1;

        for (i = 0; i < n; i++) {
                p0 = i & 1 ? odd : even;
                p1 = i & 1 ? even : odd;
                /* Make a new pipe child will write to */
                if (pipe(p0) < 0) {
                        fprintf(stderr, "error: pipe() failed\n");
                        exit(-1);
                }
                pid = fork();
                if (pid == 0) {
                        /* Read from previous pipe */
                        if (i > 0) {
                                close(p1[1]);
                                if (dup2(p1[0], 0) < 0) {
                                        fprintf(stderr, "error: dup2() failed\n");
                                        exit(-1);
                                }
                        }
                        /* Write to new pipe */
                        if (i < n-1) {
                                close(p0[0]);
                                if (dup2(p0[1], 1) < 0) {
                                        fprintf(stderr, "error: dup2() failed\n");
                                        exit(-1);
                                }
                        } else {
                                /* Last iteration - write to file */
                                if (dup2(fd, 1) < 0) {
                                        fprintf(stderr, "error: dup2() failed\n");
                                        exit(-1);
                                }
                        }
                        /* Replace with new image in child process */
                        if (execvp(*cmds[i], cmds[i]) < 0) {
                                fprintf(stderr, "error: execvp() failed\n");
                                exit(-1);
                        }
                } else if (pid > 0) {
                        if (i > 0) {
                                /* Close previous pipe */
                                close(p1[0]);
                                close(p1[1]);
                        }
                        /* Wait for child to die */
                        while (wait(NULL) != pid)
                                ;
                }
        }
        return 0;
}

/* Built-in current working directory command */
int builtin_cwd(void) {
        char dir[DIRMAX];

        if (getcwd(dir, DIRMAX) == NULL)
                return -1;
        printf("%s\n", dir);

        return 0;
}

/* Built-in change directory command */
int builtin_chdir(char *path) {
        return chdir(path);
}
