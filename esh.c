/* esh.c - Eugene's SHell */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define DIRMAX          100
#define INPUTMAX        100        
#define MAXTOK          100

#define CMD_CHDIR       0
#define CMD_LS          1
#define CMD_CWD         2
#define CMD_EXIT        3
#define CMD_ERR         -1

int cmd_ls(void);
int cmd_chdir(char *path);
int cmd_cwd(void);
int parse(char *buf, int n);

int main(void) {
        char buf[INPUTMAX];
        int retval;

        while (1) {
                printf("esh$ ");
                if (fgets(buf, INPUTMAX, stdin) == NULL)
                        break;
                buf[strlen(buf)-1] = '\0';
                retval = parse(buf, strlen(buf));
                switch (retval) {
                        case CMD_CHDIR:
                                if (cmd_chdir(buf) < 0)
                                        fprintf(stderr, "error: cmd_chdir() failed\n");
                                break;
                        case CMD_LS:
                                if (cmd_ls() < 0)
                                        fprintf(stderr, "error: cmd_ls() failed\n");
                                break;
                        case CMD_CWD:
                                if (cmd_cwd() < 0) 
                                        fprintf(stderr, "error: cmd_cwd() failed\n");
                                break;
                        case CMD_EXIT:
                                printf("Have a nice day!\n");
                                return 0;
                        case CMD_ERR:
                                fprintf(stderr, "esh: unknown string pattern\n");
                                break;
                }
        }

        return 0;
}

int cmd_ls(void) {
        int pid;
        char *cmd[] = { "ls", NULL };

        /* Launch external program */
        pid = fork();
        if (pid == 0) {
                if (execvp(*cmd, cmd) < 0) {
                        fprintf(stderr, "error: execvp() failed\n");
                        return -1;
                }
        } else {
                while (wait(NULL) != pid)
                        ;
        }

        return 0;
}

int cmd_cwd(void) {
        char dir[DIRMAX];

        if (getcwd(dir, DIRMAX) == NULL)
                return -1;
        printf("%s\n", dir);

        return 0;
}

int cmd_chdir(char *path) {
        return chdir(path);
}

int parse(char *buf, int n) {
        char *tok;

        tok = strtok(buf, " ");
        if (strncmp(tok, "cd", strlen(tok)) == 0) {
                if ((tok = strtok(NULL, " ")) == NULL) {
                        printf("error: cd command requires a relative path\n");
                        return CMD_ERR;
                }
                /* Store path in buf */
                strncpy(buf, tok, n);
                return CMD_CHDIR;
        } else if (strncmp(tok, "ls", strlen(tok)) == 0) {
                return CMD_LS;
        } else if (strncmp(tok, "pwd", strlen(tok)) == 0) {
                return CMD_CWD;
        } else if (strncmp(tok, "bye", strlen(tok)) == 0) {
                return CMD_EXIT;
        }
        return CMD_ERR;
}
