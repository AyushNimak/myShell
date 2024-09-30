#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <glob.h>
#include <sys/wait.h>
#include <signal.h>

#include "arraylist.h"

#define BUFSIZE 1024

static int mode; //0 if batch mode, 1 if interactive, 2 for (rare) case of instruction in call line
static int success_status; //1 if prev comand was succesful (true), 0 if not (false)
static arraylist_t word_in_commands; //array list to maintain all tokens in a line of a command
static int output_adjusted; //counter for fork processes to determine if output has been altered, 1 if it has been

void shell(int argc, char **argv, int fd);
void batchShell(int argc, char **argv);
void parseLine(char *line);
void expandLine(char *token);
void runCommand(int start, int end);
void forkMyLife(int start, int end);
void pipeDream(int start, int end);

//maintains if there's a pipe and where
struct hasPipe {
    int present;
    int location;
} pipeStat;

//maintains if there's a < and where
struct hasInputRedir {
    int present;
    int location;
} inputStat;

//maintains if there's a > and where
struct hasOutputRedir {
    int present;
    int location;
} outputStat;

int main(int argc, char **argv) {


    if(argc == 1 && isatty(0) == 1) {

        //one argument, interactive mode
        mode = 1;
        shell(argc, argv, 0);
    }

    else if(argc == 2) {


        mode = 0;

        int fd = open(argv[1], O_RDONLY);
    
        if(isatty(fd)) {

            //given input is a terminal
            mode = 1;
            shell(argc, argv, 0);
        }
        else{ 

            //given a .sh file, pass the fd of that file
            shell(argc, argv, fd);
        }

    }

    else {
        
        //intruction in command line ex. echo echo hello | ./mysh
        mode = 2;
        shell(argc, argv, 0);
    }

    return 0;
}

void shell(int argc, char **argv, int fd) {

    //buffer to hold command line
    char command[BUFSIZE];

    printf("Welcome to my shell!\n");
    printf("Type \"exit\" to end shell.\n");

    while(1) {


        if(mode == 1)    printf("mysh> "); //only print when in interactive mode

        fflush(stdout);

        //read lines

        char c;
        int bytes = 0;
        int position = 0;
        

        while(1) {


            //read input 1 byte at a time
            bytes = read(fd, &c, 1);

            if(bytes < 0) {
                fprintf(stderr, "Read error.\n");
                exit(EXIT_FAILURE);
            }
            else if(bytes == 0 || c == '\n') {

                //reads full line
                command[position] = '\0';
                break;
            }

            command[position++] = c;
        }


        if(strcmp(command, "exit") == 0) {
   
            exit(EXIT_SUCCESS);
            return;
        }

        parseLine(command);

        //runs once for case where instruction is in call line
        if(mode == 2)   exit(EXIT_SUCCESS);
    }

}

void parseLine(char *line) {

    al_init(&word_in_commands, 4);

    pipeStat.present = 0;
    inputStat.present = 0;
    outputStat.present = 0;

    char *token;

    token = strtok(line, " ");

    //handles "then" conditional
    if(strcmp(token, "then") == 0) {
        if (success_status == 1) {
            token = strtok(NULL, " ");
        }
        else {
            success_status = 0;
            al_destroy(&word_in_commands);
            return;
        }

    } 
    
    //handles else conditional
    else if(strcmp(token, "else") == 0) {

        if (success_status == 0) {
            token = strtok(NULL, " ");
        }
        else {
            success_status = 0;
            al_destroy(&word_in_commands);
            return;
        }
    } 

    while(token != NULL) {
 
        expandLine(token);
        token = strtok(NULL, " ");
        
    }

    //if no redirect/pipes, runs command directly through forkMyLife();
    if(pipeStat.present == 0 && inputStat.present == 0 && outputStat.present == 0) {
        forkMyLife(0, al_length(&word_in_commands) - 1);
    }
    else {
        pipeDream(0, al_length(&word_in_commands) - 1);
    }

    
    al_destroy(&word_in_commands);
    return;
}

void expandLine(char *token) {

        char *wildCard = strchr(token, '*');
        if(wildCard != NULL) {

            glob_t glob_result;
            int glob_status;

            //Handles wildcard and adds matching files to array list

            glob_status = glob(token, GLOB_NOSORT, NULL, &glob_result);
            if (glob_status == 0) {
                for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                    al_push(&word_in_commands, glob_result.gl_pathv[i]);
                }
                globfree(&glob_result);
            } else if (glob_status == GLOB_NOMATCH) {
                al_push(&word_in_commands, token);  //No matching files found
            } else {
                fprintf(stderr,"Glob error.\n");
                success_status = 0;
            }
            token = strtok(NULL, " ");

            return;
        }

        //check for pipe
        char *pipePos = strchr(token, '|');

        if(pipePos != NULL) {

            char *before = NULL;
            char *after = NULL;

            *pipePos = '\0';

            before = token;
            after = pipePos + 1;

            //recursively handles left and right hand portion!
            if(strlen(before) != 0) expandLine(before); 
            pipeStat.location = al_length(&word_in_commands);
            al_push(&word_in_commands, "|");
            if(strlen(after) != 0)  expandLine(after);

            pipeStat.present = 1;
            return;
        }

        //check for <
        char *inputPos = strchr(token, '<');

        if(inputPos != NULL) {

            char *before = NULL;
            char *after = NULL;

            *inputPos = '\0';

            before = token;
            after = inputPos + 1;

            //recursively handles left and right hand portion!
            if(strlen(before) != 0) expandLine(before);
            inputStat.location = al_length(&word_in_commands);
            al_push(&word_in_commands, "<");
            if(strlen(after) != 0)  expandLine(after);

            inputStat.present = 1;
            return;
        }

        //check for >
        char *outputPos = strchr(token, '>');

        if(outputPos != NULL) {

            char *before = NULL;
            char *after = NULL;

            *outputPos = '\0';

            before = token;
            after = outputPos + 1;

            //recursively handles left and right hand portion!
            if(strlen(before) != 0) expandLine(before);
            outputStat.location = al_length(&word_in_commands);
            al_push(&word_in_commands, ">");
            if(strlen(after) != 0)  expandLine(after);

            outputStat.present = 1;
            return;
        }

        al_push(&word_in_commands, token);

    return;

}

void runCommand(int start, int end) {

    //isolates first word as the command
    char *command = al_get(&word_in_commands, 0);

    //first checks for commands that we were required to code, and performs accordingly
    //note that each updates success_status for future conditionals    
    if(strcmp(command, "exit") == 0) {
        printf("exiting: ");

        for(int i = 1; i < al_length(&word_in_commands); i++) {
            printf("%s ", al_get(&word_in_commands, i));
        }
        printf("\n");
        exit(EXIT_SUCCESS);
    }
    
    else if(strcmp(command, "cd") == 0) {
        if(al_length(&word_in_commands) != 2) {
            fprintf(stderr, "Incorrect number of arguments.\n");
            success_status = 0;
            exit(EXIT_SUCCESS);
            return;
        } 

        int x = chdir(al_get(&word_in_commands, 1));
        if(x < 0) {
            //failed
            fprintf(stderr, "Error accessing directory.\n");
            success_status = 0;
        } 
        success_status = 1;
        return;
    }

    else if(strcmp(command, "pwd") == 0) {
        
        char pwdBuf[BUFSIZE];
        printf("current directory: %s\n", getcwd(pwdBuf, BUFSIZE));
        success_status = 1;
        return;
    }

    else if(strcmp(command, "which") == 0) {

        if((end - start + 1) != 2) {
            fprintf(stderr, "Incorrect number of arguments.\n");
            success_status = 0;
            return;
        }

        char* directories[] = {"/usr/local/bin/", "/usr/bin/", "/bin/"};
        int numDirectories = sizeof(directories) / sizeof(directories[0]);
        static char result[256]; // Assuming max path length

        for (int i = 0; i < numDirectories; i++) {
            char fullPath[256]; // Assuming max path length
            strcpy(fullPath, directories[i]);
            strcat(fullPath, al_get(&word_in_commands, start + 1));

            if (access(fullPath, X_OK) != -1) {
                strcpy(result, fullPath);
                printf("%s\n", result);
                break;
            }
        }

        success_status = 1;

        return;
    }

    //if / if found in first argument
    else if(strchr(command, '/')) {

        //its an executable
        int i = start;
        int j = 1;
        char **arguments = malloc((end-start + 2) * sizeof(char*));

        arguments[0] = malloc(50 * sizeof(char));
        arguments[0] = command;

        while (i < end) {
            
            arguments[j] = malloc(50 * sizeof(char));
            arguments[j] = al_get(&word_in_commands, i + 1);
            i++;
            j++;


        }
        arguments[end-start+1] = malloc(50 * sizeof(char));
        arguments[end-start+1] = NULL;

        if(execv(command, arguments) == -1) {
            //failed
            success_status = 0;
            for(int i = 0; i < end - start; i++) {
            free(arguments[i]);
            }
            free(arguments);
            return;
        }

        for(int i = 0; i < end - start; i++) {
            free(arguments[i]);
        }
        free(arguments);

        success_status = 1;
        exit(EXIT_SUCCESS);
        return;

    }

    //handles all other prebuilt cases
    else {

        char* directories[] = {"/usr/local/bin/", "/usr/bin/", "/bin/"};
        int numDirectories = sizeof(directories) / sizeof(directories[0]);
        static char result[256];

        for (int i = 0; i < numDirectories; i++) {
            char fullPath[256]; //
            strcpy(fullPath, directories[i]);
            strcat(fullPath, al_get(&word_in_commands, start));

            if (access(fullPath, X_OK) != -1) {


                strcpy(result, fullPath);

                int i = start;
                int j = 1;
                char **arguments = malloc((end-start+2) * sizeof(char*));

                arguments[0] = malloc(50 * sizeof(char));
                arguments[0] = result;

                while (i < end) {

                    arguments[j] = malloc(50 * sizeof(char));
                    arguments[j] = al_get(&word_in_commands, i+1);

                    i++;
                    j++;

                }
                arguments[end-start+1] = malloc(50 * sizeof(char));
                arguments[end-start+1] = NULL;

                if(execv(result, arguments) == -1) {
                    //failed
                    success_status = 0;
                    for(int i = 0; i < end - start; i++) {
                    free(arguments[i]);
                    }
                    free(arguments);
                    return;
                }
                for(int i = 0; i < end - start; i++) {
                    free(arguments[i]);
                }
                free(arguments);
                break;
            }
        }


        return;
        
    }

}

void forkMyLife(int start, int end) {

    //forks
    pid_t pid = fork();
    success_status = 1;
    if(pid < 0) {
        fprintf(stderr, "Fork error.\n");
        success_status = 0;
        return;
    }

    //child process runs command
    if(pid == 0) {

        runCommand(start, end);

    } else {

        //parent waits
                
        int status;
        wait(&status);
        if(strcmp(al_get(&word_in_commands, start), "exit") == 0)   exit(EXIT_SUCCESS);

        return;

    }

    return;
}

void pipeDream(int start, int end) {

    //first handles case if pipe present
    if(pipeStat.present == 1) {
        int p[2];
        success_status = 1;
        
        if(pipe(p) < 0) {
            fprintf(stderr, "Pipe error.\n");
            return;
        }

        int fd_in = -1;
        int fd_out = -1;


        //FORK 1
        //handles LHS and all possiblities with redirects
        if(fork() == 0) {
            close(p[0]);

            if((inputStat.present == 0 || inputStat.location > pipeStat.location)  && (outputStat.present == 0 || outputStat.location > pipeStat.location)) {

                dup2(p[1], STDOUT_FILENO);
                runCommand(start, pipeStat.location-1);

            }

            else {

                if(inputStat.present == 1 && inputStat.location < pipeStat.location && outputStat.present == 1 && outputStat.location < pipeStat.location) {
                    //both redirects found before the pipe
                    fd_in = open(al_get(&word_in_commands, (inputStat.location + 1)), O_RDONLY);
                    dup2(fd_in, STDIN_FILENO);

                    fd_out = open(al_get(&word_in_commands, (outputStat.location + 1)), O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    dup2(fd_out, STDOUT_FILENO);

                    output_adjusted = 1;



                    if(outputStat.location < inputStat.location) {
                        runCommand(start, outputStat.location - 1);
                    } else {
                        runCommand(start, inputStat.location - 1);
                    }


                } else if(inputStat.present == 1 && inputStat.location < pipeStat.location) {

                    //only input redicrect found before the pipe
                    fd_in = open(al_get(&word_in_commands, inputStat.location + 1), O_RDONLY);
                    dup2(fd_in, STDIN_FILENO);

                    runCommand(start, inputStat.location - 1);

                } else if(outputStat.present == 1 && outputStat.location < pipeStat.location) {

                    //output redicrect found before the pipe
                    fd_out = open(al_get(&word_in_commands, outputStat.location + 1), O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    dup2(fd_out, STDOUT_FILENO);

                    output_adjusted = 1;

                    runCommand(start, outputStat.location - 1);

                } 
                
            }

            if (fd_in != -1) close(fd_in);
            if (fd_out != -1) close(fd_out);

        }


        //FORK 2
        //handles RHS and all possibilites and redirects
        if(fork() == 0) {
            close(p[1]);

            if(output_adjusted == 0) dup2(p[0], STDIN_FILENO);

            if((inputStat.present == 0 || inputStat.location < pipeStat.location)  && (outputStat.present == 0 || outputStat.location < pipeStat.location)) {

                runCommand(pipeStat.location + 1, end);

            }

            else {

                if((inputStat.present == 1 && inputStat.location > pipeStat.location) && (outputStat.present == 1 && outputStat.location >pipeStat.location)) {
                    //input and output redirect present after pipe

                    fd_in = open(al_get(&word_in_commands, inputStat.location + 1), O_RDONLY);
                    dup2(fd_in, STDIN_FILENO);

                    fd_out = open(al_get(&word_in_commands, (outputStat.location + 1)), O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    dup2(fd_out, STDOUT_FILENO);

                    if(outputStat.location < inputStat.location) {
                        runCommand(pipeStat.location + 1, outputStat.location - 1);
                    } else {
                        runCommand(pipeStat.location + 1, inputStat.location - 1);
                    }

                    
                }

                else if(inputStat.present == 1 && inputStat.location > pipeStat.location) {
                    //input redirect found after the pipe

                    fd_in = open(al_get(&word_in_commands, inputStat.location + 1), O_RDONLY);
                    dup2(fd_in, STDIN_FILENO);

                    runCommand(pipeStat.location + 1, inputStat.location - 1);

                }

                else if(outputStat.present == 1 && outputStat.location > pipeStat.location) {
                    //output redirect found after the pipe

                    fd_out = open(al_get(&word_in_commands, outputStat.location + 1), O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    dup2(fd_out, STDOUT_FILENO);

                    runCommand(pipeStat.location + 1, outputStat.location - 1);


                } 

            }

            runCommand(pipeStat.location + 1, end);
            
        }

        close(p[0]);
        close(p[1]);

        wait(NULL);
        wait(NULL);

        if (fd_in != -1) close(fd_in);
        if (fd_out != -1) close (fd_out);
        return;

    }

    //both input and output redirect
    if(inputStat.present == 1 && outputStat.present == 1) {

        success_status = 1;

        pid_t pid = fork();
        if(pid < 0) {
            fprintf(stderr, "Fork error.\n");
            success_status = 0;
            return;
        }

        int fd_in = -1;
        int fd_out =-1;

        if(pid == 0) {

            fd_in = open(al_get(&word_in_commands, (inputStat.location + 1)), O_RDONLY);
            dup2(fd_in, STDIN_FILENO);

            fd_out = open(al_get(&word_in_commands, (outputStat.location + 1)), O_WRONLY | O_CREAT | O_TRUNC, 0640);
            dup2(fd_out, STDOUT_FILENO);

            if(outputStat.location < inputStat.location) {
                runCommand(start, outputStat.location - 1);
            } else {
                runCommand(start, inputStat.location - 1);
            }

        } else {
            int status;
            wait(&status);
            if (fd_in!=-1) close(fd_in);
            if(fd_out!=-1) close(fd_out);
            return;

        }

    }

    //only input redirect
    if(inputStat.present == 1) {

        success_status = 1;

        pid_t pid = fork();
        if(pid < 0) {
            fprintf(stderr, "Fork error.\n");
            success_status = 0;
            return;
        }
        int fd = -1;
        if(pid == 0) {
            fd = open(al_get(&word_in_commands, inputStat.location + 1), O_RDONLY);
            dup2(fd, STDIN_FILENO);
            runCommand(start, inputStat.location - 1);
        } else {
            int status;
            wait(&status);
            if (fd!=-1) close(fd);
            return;

        }

    }

    //only output redirect
    if(outputStat.present == 1) {

        success_status = 1;

        pid_t pid = fork();
        if(pid < 0) {
            fprintf(stderr, "Fork error.\n");
            success_status = 0;
            return;
        }
        int fd = -1;
        if(pid == 0) {
            fd = open(al_get(&word_in_commands, outputStat.location + 1), O_WRONLY | O_CREAT | O_TRUNC, 0640);
            dup2(fd, STDOUT_FILENO);
            runCommand(start, outputStat.location - 1);
        } else {
            int status;
            wait(&status);
            if (fd != -1) close(fd);
            return;

        }

    }

}

