#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

FILE *infile;

struct instruction
{
    uint8_t *data;
    int length;
    struct instruction *next;
};

struct instruction *newInstruction(int length)
{
    struct instruction *wip = malloc(sizeof(struct instruction));
    wip->data = malloc(length);
    wip->next = NULL;
    return wip;
}

struct instructionList
{
    struct instruction *first, *last;
};

void instructionListInsert(struct instructionList *theList, struct instruction *newInstruction)
{
    if (theList->first == theList->last)
    {
        theList->first = newInstruction;
        theList->last = theList->first;
    }
    else
    {
        theList->last->next = newInstruction;
        theList->last = newInstruction;
    }
}

char lookahead()
{
    long offset = ftell(infile);
    char c = fgetc(infile);
    fseek(infile, offset, SEEK_SET);
    return c;
}

void parseLine(struct instructionList *workingList)
{
    int scanning = 1;
    while (scanning)
    {
        if (lookahead() != '#')
        {
            char buffer[16];
            int buflen = 0;
            char c = fgetc(infile);
            while (!isspace(c) && c != EOF)
            {
                buffer[buflen++] = c;
                buffer[buflen] = '\0';
                c = fgetc(infile);
            }
            printf("done\n");
            scanning = 0;
        }
        else
        {
            while (fgetc(infile) != '\n')
            {
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("Please provide file to assemble!\n");
        exit(1);
    }
    infile = fopen(argv[1], "rb");
    if (infile == NULL)
    {
        printf("Error opening file [%s]\n", argv[1]);
        exit(1);
    }
    fseek(infile, 0, SEEK_END);
    long eof = infile;
    fseek(infile, 0, SEEK_SET);
    //long offset = ftell(infile);
    //char returnChar = fgetc(infile);
    //fseek(infile, offset, SEEK_SET);
    struct instructionList *parsedList = malloc(sizeof(struct instructionList));
    printf("%d\n", eof);
    for (int i = 0; i < 0xffffffff; i++)
    {
    }
    while (infile != eof)
    {
        parseLine(parsedList);
    }
}