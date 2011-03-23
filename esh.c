/* esh.c - Eugene's SHell */
/* TODO: Pipes */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

#define DIRMAX          100
#define INPUTMAX        100        
#define MAXTOK          100
#define PATH            "/home/edma2/bin/esh" /* Fake path */

int external_cmd(char **args);
int builtin_chdir(char *path);
int builtin_cwd(void);
void splitstring(char *buf, char *toks[], int n);

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

/* Split a string into an array of tokens */
void splitstring(char *buf, char *toks[], int n) {
        int i;

        toks[0] = strtok(buf, " ");
        /* Parse arguments */
        for (i = 1; i < n; i++) {
                if ((toks[i] = strtok(NULL, " ")) == NULL)
                        break;
                /* Environment variable */
                if (*toks[i] == '$')
                        toks[i] = getenv(toks[i]+1);
        }
}

/* Launch external command */
int external_cmd(char **args) {
        int pid;

        pid = fork();
        if (pid == 0) {
                /* Child process */
                if (execvp(*args, args) < 0) {
                        /* Kill child process immediately if failed */
                        fprintf(stderr, "error: execvp() failed\n");
                        exit(-1);
                }
        } else {
                /* Wait for child process to die */
                while (wait(NULL) != pid)
                        ;
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
