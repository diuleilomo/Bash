#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <assert.h>
#include <fcntl.h>
#include "history.h"
// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(char *);

// Function forward references
void trim(char *);
int strContains(char *, char *);
char **tokenise(char *, char *);
char **fileNameExpand(char **);
void freeTokens(char **);
char *findExecutable(char *, char **);
int isExecutable(char *);
void prompt(void);
void execute(char **args, char **path, char **envp);
char **cutTokens(char ** tokens, int count);
void outexecute(char **args, char **path, char **envp);
void printrun(char **args, char **path, char **envp);

#define HISTFILE ".mymysh_history"

// Global Constants

#define MAXLINE 200


// Main program
// Set up enviroment and then run main loop
// - read command, execute command, repeat

int main(int argc, char *argv[], char *envp[]) {
   pid_t pid;   // pid of child process
   int stat;    // return status of child
   char **path; // array of directory names
   int cmdNo;   // command number
   int i;       // generic index

   // set up command PATH from environment variable
   for (i = 0; envp[i] != NULL; i++) {
      if (strncmp(envp[i], "PATH=", 5) == 0) break;
   }

   if (envp[i] == NULL) 
      path = tokenise("/bin:/usr/bin",":");
   else
   // &envp[i][5] skips over "PATH=" prefix
      path = tokenise(&envp[i][5],":");
    
#ifdef DBUG
for (i = 0; path[i] != NULL;i++)
   printf("path[%d] = %s\n",i,path[i]);
#endif

   // initialise command history
   // - use content of ~/.mymysh_history file if it exists

   cmdNo = initCommandHistory();

   // main loop: print prompt, read line, execute command

   char line[MAXLINE];
   prompt();

   while (fgets(line, MAXLINE, stdin) != NULL) {
      trim(line); // remove leading/trailing space
      // empty string
      if (strcmp(line,"") == 0) { prompt(); continue; }

      // re-execute command
      if (line[0] == '!' && line[1] == '!') {
         int temp = cmdNo - 1;
         char *history1 = getCommandFromHistory(temp);
         strcpy(line, history1);
         printf("%s\n", history1);
      }


      // history subsitution command check
      if (strcmp(line, "!") == 0) {
         printf("Invalid history substitution\n");
         prompt();
         continue;
      }

      // history subsitution command
      char test[2];
      char sub[20];
      memcpy(test, line, 1);
      if (line[0] == '!') {;
         sscanf(line, "!%s", sub);
         char *history2 = getCommandFromHistory(atoi(sub));
         if (history2 == NULL) {
               printf("No command #%s\n", sub);
               prompt();
               continue;
         }
         strcpy(line, history2);
         printf("%s\n", history2);
      }

      // tokenise command
      char **tokens = tokenise(line, " ");

      // filename expansion
      tokens = fileNameExpand(tokens);


      // exit command
      if (strcmp(tokens[0],"exit") == 0) {
         addToCommandHistory(line, cmdNo);
         cmdNo++;
         break;
      }

      // history command
      if (strcmp(tokens[0],"h") == 0 || strcmp(tokens[0],"history") == 0) {
         showCommandHistory();
         addToCommandHistory(line, cmdNo);
         cmdNo++;
         prompt();
         continue;
      }

      // pwd command
      if (strcmp(tokens[0],"pwd") == 0) {
         char cwd[256];
         if (getcwd(cwd, sizeof(cwd)) == NULL) {
               perror("getcwd() error");
         } else {
               printf("%s\n", cwd);
         }
         addToCommandHistory(line, cmdNo);
         cmdNo++;
         prompt();
         continue;
      }

      // cd command
      if (strcmp(tokens[0], "cd") == 0) {
         if (tokens[1] == NULL) {
               chdir(getenv("HOME"));
         } else if (tokens[1] != NULL && strcmp("..", tokens[1]) == 0) {
               chdir("..");
         } else if ((tokens[1] != NULL && strcmp(".", tokens[1]) == 0)) {
               chdir(".");
         } else {
               if (chdir(tokens[1]) != 0) {
               printf("%s: No such file or directory\n", tokens[1]);
               prompt();
               continue;
               }
         }

         char cdpath[256];
         getcwd(cdpath, sizeof(cdpath));
         printf("%s\n", cdpath);
         addToCommandHistory(line, cmdNo);
         cmdNo++;
         prompt();
         continue;
      }

      // i/o redirection check
      int i;
      int checkflag = 0;
      for (i = 0; tokens[i] != NULL; i++) {
         //printf("%s\n", tokens[i]);
      }
      //printf("i = %d\n", i);

      for (int j = 0; tokens[j] != NULL; j++) {
         if ((strcmp(tokens[j], ">") == 0 || strcmp(tokens[j], "<") == 0) && j != i - 2) {
               checkflag = 1;
               break;
         }
      }

      if (checkflag == 1) {
         printf("Invalid i/o redirection\n");
         prompt();
         continue;
      }

      FILE *fp;
      int ioflag = 0;
      if (i >= 2) {
         // Redirect command input
         if (strcmp(tokens[i-2], "<") == 0) {
               fp = fopen(tokens[i-1], "r");
               if (fp == NULL) {
                  printf("Input redirection: No such file or directory\n");
                  prompt();
                  continue;
               }
               ioflag = 1;
               fclose(fp);
         }

         // Redirect command output
         if (strcmp(tokens[i-2], ">") == 0) {
               fp = fopen(tokens[i-1], "r");
               if (fp == NULL) {
                  fp = fopen(tokens[i-1], "w");
               }
               ioflag = 2;
               fclose(fp);
         }
      }

      // i/o execution
      if (ioflag == 1 || ioflag == 2) {
         // Parent process
         pid = fork();
         if (pid != 0) {
               wait(&stat);

               printf("--------------------\n");
               printf("Return %d\n", WEXITSTATUS(stat));
               if (WEXITSTATUS(stat) == 0) {
                  addToCommandHistory(line, cmdNo);
                  cmdNo++;
               }
         // Child process
         } else {

            if (ioflag == 1) {
                  int fd = open(tokens[i-1], O_RDONLY);
                  dup2(fd, 0);
                  close(fd);
                  tokens = cutTokens(tokens, i);
                  execute(tokens, path, envp);
               } else if (ioflag == 2) {
                  printrun(tokens, path, envp);
                  printf("--------------------\n");
                  int fd = open(tokens[i-1], O_TRUNC|O_WRONLY);
                  dup2(fd, 1);
                  close(fd);
                  tokens = cutTokens(tokens, i);
                  outexecute(tokens, path, envp);
               }

               exit(1);
         }
         prompt();
         freeTokens(tokens);
         continue;

      }


      // Parent process
      pid = fork();
      if (pid != 0) {
         wait(&stat);
         if (WEXITSTATUS(stat) >= 2) {
            printf("--------------------\n");
            printf("Return %d\n", WEXITSTATUS(stat));
         }
         if (WEXITSTATUS(stat) == 0) {
            printf("--------------------\n");
            printf("Return %d\n", WEXITSTATUS(stat));
            addToCommandHistory(line, cmdNo);
            cmdNo++;
         }
      // Child process
      } else {
         execute(tokens, path, envp);
         exit(1);
      }

        prompt();
        freeTokens(tokens);

   }

   saveCommandHistory();
   cleanCommandHistory();
   printf("\n");
   return(EXIT_SUCCESS);
}


// fileNameExpand: expand any wildcards in command-line args
// - returns a possibly larger set of tokens
char **fileNameExpand(char **tokens)
{
   char **newTokens = malloc(MAXLINE * sizeof(char *));
   glob_t result;

   glob(tokens[0], GLOB_NOCHECK|GLOB_TILDE, NULL, &result);

   for (int i = 1; tokens[i] != NULL; i++) {
         glob(tokens[i], GLOB_NOCHECK|GLOB_TILDE|GLOB_APPEND, NULL, &result);
   }

   //printf("Glob tokens:\n");
   int i;
   for (i = 0; i < result.gl_pathc; i++) {
         newTokens[i] = strdup(result.gl_pathv[i]);
   }
   newTokens[i] = NULL;

   globfree(&result);


   freeTokens(tokens);
   return newTokens;
}

char **cutTokens(char ** tokens, int count)
{
   char **cut = malloc(MAXLINE * sizeof(char *));

   int i;
   for (i = 0; i < (count - 2); i++) {
         cut[i] = strdup(tokens[i]);
   }
   cut[i] = NULL;
   //printf("Cut tokens:\n");
   //for (i = 0; cut[i] != NULL; i++) {
   //    printf("%s\n",cut[i]);
   //}
   freeTokens(tokens);
   return cut;
}


// findExecutable: look for executable in PATH
char *findExecutable(char *cmd, char **path)
{
      char executable[MAXLINE];
      executable[0] = '\0';
      if (cmd[0] == '/' || cmd[0] == '.') {
         strcpy(executable, cmd);
         if (!isExecutable(executable))
            executable[0] = '\0';
      }
      else {
         int i;
         for (i = 0; path[i] != NULL; i++) {
            sprintf(executable, "%s/%s", path[i], cmd);
            if (isExecutable(executable)) break;
         }
         if (path[i] == NULL) executable[0] = '\0';
      }
      if (executable[0] == '\0')
         return NULL;
      else
         return strdup(executable);
}

// isExecutable: check whether this process can execute a file
int isExecutable(char *cmd)
{
   struct stat s;
   // must be accessible
   if (stat(cmd, &s) < 0)
      return 0;
   // must be a regular file
   //if (!(s.st_mode & S_IFREG))
   if (!S_ISREG(s.st_mode))
      return 0;
   // if it's owner executable by us, ok
   if (s.st_uid == getuid() && s.st_mode & S_IXUSR)
      return 1;
   // if it's group executable by us, ok
   if (s.st_gid == getgid() && s.st_mode & S_IXGRP)
      return 1;
   // if it's other executable by us, ok
   if (s.st_mode & S_IXOTH)
      return 1;
   return 0;
}

// tokenise: split a string around a set of separators
// create an array of separate strings
// final array element contains NULL
char **tokenise(char *str, char *sep)
{
   // temp copy of string, because strtok() mangles it
   char *tmp;
   // count tokens
   tmp = strdup(str);
   int n = 0;
   strtok(tmp, sep); n++;
   while (strtok(NULL, sep) != NULL) n++;
   free(tmp);
   // allocate array for argv strings
   char **strings = malloc((n+1)*sizeof(char *));
   assert(strings != NULL);
   // now tokenise and fill array
   tmp = strdup(str);
   char *next; int i = 0;
   next = strtok(tmp, sep);
   strings[i++] = strdup(next);
   while ((next = strtok(NULL,sep)) != NULL)
      strings[i++] = strdup(next);
   strings[i] = NULL;
   free(tmp);
   return strings;
}

// freeTokens: free memory associated with array of tokens
void freeTokens(char **toks)
{
   for (int i = 0; toks[i] != NULL; i++)
      free(toks[i]);
      free(toks);
}

// trim: remove leading/trailing spaces from a string
void trim(char *str)
{
   int first, last;
   first = 0;
   while (isspace(str[first])) first++;
   last  = strlen(str)-1;
   while (isspace(str[last])) last--;
   int i, j = 0;
   for (i = first; i <= last; i++) str[j++] = str[i];
   str[j] = '\0';
}

// strContains: does the first string contain any char from 2nd string?
int strContains(char *str, char *chars)
{
   for (char *s = str; *s != '\0'; s++) {
      for (char *c = chars; *c != '\0'; c++) {
         if (*s == *c) return 1;
      }
   }
   return 0;
}

// prompt: print a shell prompt
// done as a function to allow switching to $PS1
void prompt(void)
{
   printf("mymysh$ ");
}



// execute: run a program, given command-line args, path and envp
void execute(char **args, char **path, char **envp)
{
   // TODO: implement the find-the-executable and execve() it code
   // command string
   char *command = NULL;
   // copy first char
   char *arg = args[0];
   // form command string
   if ((arg[0] == '/') || (arg[0] == '.')) {
      //check if excecutable
      if (isExecutable(args[0])) {
         command = args[0];
      }
   } else {
      //loop through to find executable
      for (int i = 0; path[i] != NULL; i++) {
         char *cmd = malloc(strlen(args[0]) + strlen(path[i])+1);
         strcpy(cmd, path[i]);   //'dir'[0]
         strcat(cmd, "/");
         strcat(cmd, args[0]);
         // only if exceutable is found
         if (isExecutable(cmd)) {
            command = cmd;
            break;
         }
      }
   }

   //no command
   if (command == NULL) {
      printf("%s: Command not found\n", args[0]);
   // execute
   } else {

      printf("Running %s ...\n", command);
      printf("--------------------\n");
      int stat = execve(command, args, envp);

      // fail
      if(stat == -1) perror("Exec failed");
   }
}

// outexecute: run a program, given command-line args, path and envp
void outexecute(char **args, char **path, char **envp)
{
   // TODO: implement the find-the-executable and execve() it code
   // command string
   char *command = NULL;
   // copy first char
   char *arg = args[0];
   // form command string
   if ((arg[0] == '/') || (arg[0] == '.')) {
      //check if excecutable
      if (isExecutable(args[0])) {
         command = args[0];
      }
   } else {
      //loop through to find executable
      for (int i = 0; path[i] != NULL; i++) {
         char *cmd = malloc(strlen(args[0]) + strlen(path[i])+1);
         strcpy(cmd, path[i]);   //'dir'[0]
         strcat(cmd, "/");
         strcat(cmd, args[0]);
         // only if exceutable is found
         if (isExecutable(cmd)) {
            command = cmd;
            break;
         }
      }
   }

   //no command
   if (command == NULL) {
      printf("%s: Command not found\n", command);
   // execute
   } else {

      int stat = execve(command, args, envp);

      // fail
      if(stat == -1) perror("Exec failed");
   }
}

void printrun(char **args, char **path, char **envp)
{
   // TODO: implement the find-the-executable and execve() it code
   // command string
   char *command = NULL;
   // copy first char
   char *arg = args[0];
   // form command string
   if ((arg[0] == '/') || (arg[0] == '.')) {
      //check if excecutable
      if (isExecutable(args[0])) {
         command = args[0];
      }
   } else {
      //loop through to find executable
      for (int i = 0; path[i] != NULL; i++) {
         char *cmd = malloc(strlen(args[0]) + strlen(path[i])+1);
         strcpy(cmd, path[i]);   //'dir'[0]
         strcat(cmd, "/");
         strcat(cmd, args[0]);
         // only if exceutable is found
         if (isExecutable(cmd)) {
            command = cmd;
            break;
         }
      }
   }
   //no command
   if (command == NULL) {
      printf("%s: Command not found\n", command);
   // execute
   } else {
      printf("Running %s ...\n", command);
   }
}