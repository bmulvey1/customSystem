#include <stdio.h>
#include <stdlib.h>

#pragma once

struct tacLine
{
    char *addresses[3];
    char *operation;
    struct tacLine *nextLine;
    struct tacLine *prevLine;
};

void printTacLine(struct tacLine *it);

struct tacLine *newtacLine();

struct tacLine *appendTAC(struct tacLine *parent, struct tacLine *child);

struct tacLine *prependTAC(struct tacLine *parent, struct tacLine *child);

struct tacLine *findLastTAC(struct tacLine* head);