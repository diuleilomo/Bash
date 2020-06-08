#include <stdio.h>

// Functions on the Command History object

int initCommandHistory();
void addToCommandHistory(char *cmdLine, int seqNo);
void showCommandHistory();
char *getCommandFromHistory(int cmdNo);
void saveCommandHistory();
void cleanCommandHistory();
