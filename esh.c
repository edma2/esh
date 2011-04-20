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
#define BUFMAX          256 
#define TOKS            16
#define CMDS            16

int builtin_chdir(char *path);
int builtin_cwd(void);
int parse(char *input, char *cmd[CMDS][TOKS], int n);
int cmd_exec(char *cmd[CMDS][TOKS], int fd);
void cmd_free(char *cmd[CMDS][TOKS]);

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
                        fprintf(stderr, "esh: parse() failed\n");
                        cmd_free(cmd);
                        continue;
                }
                /* Look at first token */
                if (strcmp(cmd[0][0], "cd") == 0) {
                        if (cmd[0][2] != NULL || cmd[1][0] != NULL)
                                fprintf(stderr, "esh: usage: cd [dir]\n");
                        else if (builtin_chdir(cmd[0][1]) < 0)
                                fprintf(stderr, "esh: builtin cd failed\n");
                } else if (strcmp(cmd[0][0], "pwd") == 0) {
                        if (cmd[0][1] != NULL || cmd[1][0] != NULL)
                                fprintf(stderr, "esh: usage: pwd\n");
                        else if (builtin_cwd() < 0) 
                                fprintf(stderr, "esh: builtin pwd failed\n");
                } else if (strcmp(cmd[0][0], "exit") == 0) {
                        if (cmd[0][1] != NULL || cmd[1][0] != NULL) {
                                fprintf(stderr, "esh: usage: exit\n");
                        } else {
                                cmd_free(cmd);
                                return 0;
                        }
                } else if (cmd_exec(cmd, fd) < 0) {
                                fprintf(stderr, "esh: cmd_exec() failed\n");
                }
                if (fd != 1)
                        close(fd);
                cmd_free(cmd);
        }

        return 0;
}

/* Built-in current working directory command */
int builtin_cwd(void) {
        char dir[DIRMAX];

        if (getcwd(dir, DIRMAX) == NULL)
                return -1;

        return 0;
}

/* Built-in change directory command */
int builtin_chdir(char *path) {
        return chdir(path);
}

/* Return file descriptor we want to write to */
int parse(char *input, char *cmd[CMDS][TOKS], int n) {
        int i = 0, cmd_index, tok_index, buf_index;
        enum { START, END, ERROR, QUOTE, SPACE, PIPE, TOKEN, REDIRECT } state;
        char buf[BUFMAX];
        char c;
        char quote;
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
                                if (c == '|' || c == '\0' || c == '>' || c == '\"' || c == '\'') {
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
                                                state = REDIRECT;
                                        }
                                } else if (c == '\"' || c == '\'') {
                                        quote = c;
                                        buf[buf_index++] = c;
                                        state = QUOTE;
                                } else if (c != ' ') {
                                        /* Begin reading new token into buffer */
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case QUOTE:
                                buf[buf_index++] = c;
                                state = (c == quote) ? TOKEN : QUOTE;
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
                                } else if (c == '\"' || c == '\'') {
                                        state = ERROR;
                                } else {
                                        buf[buf_index++] = c;
                                        state = TOKEN;
                                }
                                break;
                        case PIPE:
                                if (c == '|' || c == '\0' || c == '>' || c == '\"' || c == '\'') {
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
                                        state = ++flag_redirect > 1 ? ERROR : REDIRECT;
                                } else if (c == ' ') {
                                        /* This means we're done with the filename */
                                        if (buf_index > 0) {
                                                buf[buf_index] = '\0';
                                                if (flag_redirect == 0)
                                                        fd = creat(buf, 0666);
                                                else
                                                        fd = open(buf, O_RDWR|O_CREAT|O_APPEND, 0666);

                                        }
                                        state = REDIRECT;
                                } else if (c == '\0') {
                                        /* This means we're done with the filename */
                                        if (buf_index > 0) {
                                                buf[buf_index] = '\0';
                                                if (flag_redirect == 0)
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
                cmd_free(cmd);
                return -1;
        }
        /* Return either given file descriptor or stdout */
        return fd ? fd : 1;
}

int cmd_exec(char *cmd[CMDS][TOKS], int fd) {
        int i, pid;
        int pipe0[2], pipe1[2];
        int *read, *write;

        for (i = 0; i < CMDS; i++) {
                if (cmd[i][0] == NULL)
                        break;
                write = i & 1 ? pipe1 : pipe0;
                read = i & 1 ? pipe0 : pipe1;
                /* Make a new pipe child will write to */
                if (pipe(write) < 0)
                        return -1;
                pid = fork();
                if (pid == 0) {
                        /* Read from previous pipe */
                        if (i > 0) {
                                close(read[1]);
                                if (dup2(read[0], 0) < 0)
                                        exit(-1);
                        }
                        /* Write to new pipe */
                        if (cmd[i+1][0] != NULL) {
                                close(write[0]);
                                if (dup2(write[1], 1) < 0)
                                        exit(-1);
                        } else {
                                /* Last iteration - write to specified file */
                                if (dup2(fd, 1) < 0)
                                        exit(-1);
                        }
                        /* Replace with new image in child process */
                        if (execvp(*cmd[i], cmd[i]) < 0) {
                                fprintf(stderr, "esh: command not found: %s\n", *cmd[i]);
                                exit(-1);
                        }
                } else if (pid > 0) {
                        if (i > 0) {
                                /* Close previous pipe */
                                close(read[0]);
                                close(read[1]);
                        }
                }
        }
        /* Wait for child to die */
        while (wait(NULL) != pid)
                ;
        return 0;
}

void cmd_free(char *cmd[CMDS][TOKS]) {
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
