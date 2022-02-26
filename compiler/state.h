#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct registerState
{
    char live;  // whether this var will be used in the future
    char dying; // whether this var will be used in the current instruction but no later
    char dirty;
    char touched;
    char *contains;
};

struct registerState *newRegisterState();

void printRegisterStates(struct registerState **registerStates);

void printRegisterStatesHorizontal(struct registerState **registerStates);

void freeRegisterStates(struct registerState **s);

struct registerState **duplicateRegisterStates(struct registerState **theStates);

int findInRegisters(struct registerState **registerStates, char *var);
