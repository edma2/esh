/* esh.c - Eugene's SHell */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdlib.h>

#define DIRMAX          100
#define INPUTMAX        4096
#define TOKS            16
#define CMDS            16
#define BUFMAX          256 

int builtin_chdir(char *path);
int builtin_cwd(void);
int parse(char *input, char *cmd[CMDS][TOKS], int n);
int execcmds(char *cmd[CMDS][TOKS], int fd);
void free_cmd(char *cmd[CMDS][TOKS]);

int main(void) {
        char input[INPUTMAX];
        char *cmd[CMDS][TOKS];
        int fd;

        while (1) {
                printf("esh$ ");

                /* Parse input into tokens */
                if (fgets(input, INPUTMAX, stdin) == NULL)
                        break;
                input[strlen(input)-1] = '\0';
                if ((fd = parse(input, cmd, INPUTMAX)) < 0) {
                        fprintf(stderr, "error: parse() failed\n");
                        free_cmd(cmd);
                        continue;
                }
                /* Look at first token */
                if (strcmp(cmd[0][0], "cd") == 0) {
                        if (cmd[0][2] != NULL || cmd[1][0] != NULL)
                                fprintf(stderr, "usage: cd [dir]\n");
                        else if (builtin_chdir(cmd[0][1]) < 0)
                                fprintf(stderr, "error: builtin cd failed\n");
                } else if (strcmp(cmd[0][0], "pwd") == 0) {
                        if (cmd[0][1] != NULL || cmd[1][0] != NULL)
                                fprintf(stderr, "usage: pwd\n");
                        else if (builtin_cwd() < 0) 
                                fprintf(stderr, "error: builtin pwd failed\n");
                } else if (strcmp(cmd[0][0], "exit") == 0) {
                        if (cmd[0][1] != NULL || cmd[1][0] != NULL) {
                                fprintf(stderr, "usage: exit\n");
                        } else {
                                free_cmd(cmd);
                                return 0;
                        }
                } else {
                        if (execcmds(cmd, fd) < 0)
                                fprintf(stderr, "error: execcmds() failed\n");
                }
                free_cmd(cmd);
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


int parse(char *input, char *cmd[CMDS][TOKS], int n) {
        int i = 0, cmd_index, tok_index, buf_index;
        enum { START, END, ERROR, SPACE, PIPE, TOKEN, REDIRECT } state;
        char buf[BUFMAX];
        char c;
        int flag_redirect = 0;
        int fd = 0;

        state = START;
        while (state != ERROR && state != END) {
                /* Read next character */
                c = input[i++];
                switch(state) {
                        case START:
                                /* Initialize all indices to zero */
                                cmd_index = 0;
                                tok_index = 0;
                                buf_index = 0;
                                if (c == '|' || c == '\0' || c == '>') {
                                        state = ERROR;
                                } else if (c == ' ') {
                                        state = SPACE;
                                } else {
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case SPACE:
                                if (c == '|') {
                                        /* If we haven't seen any tokens yet */
                                        if (tok_index == 0) {
                                                state = ERROR;
                                        } else {
                                                /* 
                                                 * This means we've already read a
                                                 * word into the array, so the pipe is legal. 
                                                 * Terminate the current command
                                                 */
                                                cmd[cmd_index++][tok_index] = NULL;
                                                /* 
                                                 * Increment command index and reset
                                                 * token index.
                                                 */
                                                tok_index = 0;
                                                state = PIPE;
                                        }
                                } else if (c == '\0') {
                                        if (tok_index == 0) {
                                                state = ERROR;
                                        } else {
                                                /* Terminate current command */
                                                cmd[cmd_index++][tok_index] = NULL;
                                                /* Terminate everything */
                                                cmd[cmd_index][0] = NULL;
                                                state = END;
                                        }
                                } else if (c == '>') {
                                        if (tok_index == 0) {
                                                state = ERROR;
                                        } else {
                                                /* Terminate current command */
                                                cmd[cmd_index++][tok_index] = NULL;
                                                /* Terminate everything */
                                                cmd[cmd_index][0] = NULL;
                                                buf_index = 0;
                                                state = flag_redirect ? ERROR : REDIRECT;
                                        }
                                } else if (c != ' ') {
                                        /* Begin reading new token into buffer */
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case TOKEN:                           
                                if (c == '|') {
                                        /* Terminate string */
                                        buf[buf_index] = '\0';
                                        if ((cmd[cmd_index][tok_index] = strdup(buf)) == NULL)
                                                state = ERROR;
                                        /* Terminate current command */
                                        cmd[cmd_index][tok_index+1] = NULL;
                                        cmd_index++;
                                        tok_index = 0;
                                        buf_index = 0;
                                        state = PIPE;
                                } else if (c == ' ') {
                                        buf[buf_index] = '\0';
                                        if ((cmd[cmd_index][tok_index] = strdup(buf)) == NULL)
                                                state = ERROR;
                                        tok_index++;
                                        buf_index = 0;
                                        state = SPACE;
                                } else if (c == '\0') { 
                                        buf[buf_index] = '\0';
                                        if ((cmd[cmd_index][tok_index++] = strdup(buf)) == NULL)
                                                state = ERROR;
                                        /* Terminate current command */
                                        cmd[cmd_index++][tok_index] = NULL;
                                        /* Terminate everything */
                                        cmd[cmd_index][0] = NULL;
                                        state = END;
                                } else if (c == '>') { 
                                        buf[buf_index] = '\0';
                                        if ((cmd[cmd_index][tok_index++] = strdup(buf)) == NULL)
                                                state = ERROR;
                                        /* Terminate current command */
                                        cmd[cmd_index++][tok_index] = NULL;
                                        /* Terminate everything */
                                        cmd[cmd_index][0] = NULL;
                                        buf_index = 0;
                                        state = flag_redirect ? ERROR : flag_redirect;
                                } else {
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case PIPE:
                                if (c == '|' || c == '\0' || c == '>') {
                                        state = ERROR;
                                } else if (c != ' ') {
                                        /* Begin reading new token into buffer */
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case REDIRECT:
                                if (c == '|') {
                                        state = ERROR;
                                } else if (c == '>') {
                                        state = ++flag_redirect > 2 ? ERROR : REDIRECT;
                                } else if (c == ' ') {
                                        /* This means we're done with the filename */
                                        if (buf_index > 0) {
                                                buf[buf_index] = '\0';
                                                if (flag_redirect == 1)
                                                        fd = creat(buf, 0666);
                                                else
                                                        fd = open(buf, O_RDWR|O_CREAT|O_APPEND, 0666);
                                        }
                                        state = REDIRECT;
                                } else if (c == '\0') {
                                        /* This means we're done with the filename */
                                        if (buf_index > 0) {
                                                buf[buf_index] = '\0';
                                                if (flag_redirect == 1)
                                                        fd = creat(buf, 0666);
                                                else
                                                        fd = open(buf, O_RDWR|O_CREAT|O_APPEND, 0666);
                                        }
                                        state = fd ? END : ERROR;
                                } else if (c != ' ') {
                                        buf[buf_index++] = c;
                                        state = fd ? ERROR : REDIRECT;
                                }
                        default:
                                break;
                }
                /* Reserve extra element in array for terminating bytes */
                if (tok_index >= TOKS+1 || cmd_index >= CMDS+1 
                                || buf_index >= BUFMAX+1 || i >= n)
                        state = ERROR;
        }
        if (state == ERROR) {
                free_cmd(cmd);
                return -1;
        }
        /* Return either given file descriptor or stdout */
        return fd ? fd : 1;
}

void free_cmd(char *cmd[CMDS][TOKS]) {
        int cmd_index, tok_index;
        for (cmd_index = 0; cmd_index < CMDS; cmd_index++) {
                if (cmd[cmd_index][0] == NULL)
                        break;
                for (tok_index = 0; tok_index < TOKS; tok_index++) {
                        if (cmd[cmd_index][tok_index] == NULL)
                                break;
                        free(cmd[cmd_index][tok_index]);
                }
        }
}

int execcmds(char *cmd[CMDS][TOKS], int fd) {
        int i, pid;
        int even[2], odd[2];
        int *p0, *p1;

        for (i = 0; i < CMDS; i++) {
                if (cmd[i][0] == NULL)
                        break;
                p0 = i & 1 ? odd : even;
                p1 = i & 1 ? even : odd;
                /* Make a new pipe child will write to */
                if (pipe(p0) < 0) {
                        fprintf(stderr, "error: pipe() failed\n");
                        return -1;
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
                        if (cmd[i+1][0] != NULL) {
                                close(p0[0]);
                                if (dup2(p0[1], 1) < 0) {
                                        fprintf(stderr, "error: dup2() failed\n");
                                        exit(-1);
                                }
                        } else {
                                /* Last iteration - write to specified file */
                                if (dup2(fd, 1) < 0) {
                                        fprintf(stderr, "error: dup2() failed\n");
                                        exit(-1);
                                }
                        }
                        /* Replace with new image in child process */
                        if (execvp(*cmd[i], cmd[i]) < 0) {
                                printf("%s\n", *cmd[i]);
                                fprintf(stderr, "error: execvp() failed\n");
                                exit(-1);
                        }
                } else if (pid > 0) {
                        if (i > 0) {
                                /* Close previous pipe */
                                close(p1[0]);
                                close(p1[1]);
                        }
                }
        }
        /* Wait for child to die */
        while (wait(NULL) != pid)
                ;
        return 0;
}
