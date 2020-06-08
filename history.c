#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "history.h"

// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(const char *s);

// Command History
// array of command lines
// each is associated with a sequence number

#define MAXHIST 20
#define MAXSTR  200
#define HISTFILE ".mymysh_history"

typedef struct _history_entry {
   int   seqNumber;
   char *commandLine;
} HistoryEntry;

typedef struct _history_list {
   int nEntries;
   HistoryEntry commands[MAXHIST];
} HistoryList;

HistoryList CommandHistory;

// initCommandHistory()
// - initialise the data structure
// - read from .history if it exists

int initCommandHistory()
{
   // TODO
   // Try to open $HOME/.mymysh_history
   // If you can open it, read values and initialise CommanHistory to those values
   // Otherwise initialise vals to 0
   FILE *fp;
   int num;



   if ((fp = fopen(HISTFILE, "rb"))) {

         fread(&CommandHistory, sizeof(HistoryList), 1, fp);
         for (int i = 0; i < MAXHIST; i++) {
            CommandHistory.commands[i].commandLine = malloc(MAXSTR);
         }
         for (int i = 0; i < MAXHIST; i++) {
            fread(CommandHistory.commands[i].commandLine, MAXSTR, 1, fp);
         }

         if (CommandHistory.nEntries == MAXHIST) {
            num = CommandHistory.commands[MAXHIST-1].seqNumber + 1;
         } else {
            num = CommandHistory.nEntries + 1;
         }


   } else {
         fp = fopen(HISTFILE, "wb");
         CommandHistory.nEntries = 0;
         for (int i = 0; i < MAXHIST; i++) {
            CommandHistory.commands[i].commandLine = malloc(MAXSTR);
         }
         num = 1;
   }

   fclose(fp);
   return num;
}

// addToCommandHistory()
// - add a command line to the history list
// - overwrite oldest entry if buffer is full

void addToCommandHistory(char *cmdLine, int seqNo)
{

   if (CommandHistory.nEntries == MAXHIST) {

         for (int i = 1; i < MAXHIST; i++) {
            CommandHistory.commands[i-1].seqNumber = CommandHistory.commands[i].seqNumber;
            strcpy(CommandHistory.commands[i-1].commandLine, CommandHistory.commands[i].commandLine);
         }
         CommandHistory.commands[MAXHIST-1].seqNumber = seqNo;
         strcpy(CommandHistory.commands[MAXHIST-1].commandLine, cmdLine);
   } else {
         int k = CommandHistory.nEntries;
         CommandHistory.commands[k].seqNumber = seqNo;
         strcpy(CommandHistory.commands[k].commandLine, cmdLine);
         CommandHistory.nEntries++;

   }

}


// showCommandHistory()
// - display the list of

void showCommandHistory()
{


   for (int i = 0; i < CommandHistory.nEntries; i++) {
         printf(" %d  %s\n", CommandHistory.commands[i].seqNumber, CommandHistory.commands[i].commandLine);
   }

}

// getCommandFromHistory()
// - get the command line for specified command
// - returns NULL if no command with this number

char *getCommandFromHistory(int cmdNo)
{

   char *buffer = malloc(MAXSTR);

   for (int i = 0; i < CommandHistory.nEntries; i++) {
         if (CommandHistory.commands[i].seqNumber == cmdNo) {
            strcpy(buffer, CommandHistory.commands[i].commandLine);
            return buffer;
         }
   }

   return NULL;
}


// saveCommandHistory()
// - write history to $HOME/.mymysh_history

void saveCommandHistory()
{
   FILE *fp;
   fp = fopen(HISTFILE, "wb");

   if (fp == NULL) {
      printf("File not found\n");
      abort();
   }

   fwrite(&CommandHistory, sizeof(HistoryList), 1, fp);
   for (int i = 0; i < MAXHIST; i++) {
         fwrite(CommandHistory.commands[i].commandLine, MAXSTR, 1, fp);
   }
   fclose(fp);

}

// cleanCommandHistory
// - release all data allocated to command history

void cleanCommandHistory()
{
   // TODO
   for (int i = 0; i < MAXHIST; i++) {
         free(CommandHistory.commands[i].commandLine);
   }


}