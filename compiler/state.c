#include "state.h"

struct registerState *newRegisterState()
{
    struct registerState *wip = malloc(sizeof(struct registerState));
    wip->live = 0;
    wip->dirty = 0;
    wip->touched = 0;
    wip->contains = NULL;
    return wip;
}

void printRegisterStates(struct registerState **registerStates)
{
    printf("----------\n");

    for (int i = 0; i < 12; i++)
    {
        printf("r%2d:", i);
        if (registerStates[i]->contains == NULL)
        {
            printf("NULL      |");
        }
        else
        {
            for (int j = 0; j < 10; j++)
            {
                if (registerStates[i]->contains[j] == '\0')
                {
                    while (j < 10)
                    {
                        printf(" ");
                        j++;
                    }
                    break;
                }
                else
                {
                    printf("%c", registerStates[i]->contains[j]);
                }
            }
            printf("|");
        }

        if (registerStates[i]->live)
            printf("live ");
        else
            printf("dead ");

        if (registerStates[i]->dirty)
            printf("dirty");
        else
            printf("     ");

        printf("\n");
    }
    printf("----------\n\n");
}

void printRegisterStatesHorizontal(struct registerState **registerStates)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            printf("   r%2d|", i);

    printf("\n");

    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains != NULL)
        {
            int j;
            for (j = 0; j < 6; j++)
            {
                if (registerStates[i]->contains[j] != '\0')
                    printf("%c", registerStates[i]->contains[j]);
                else
                    break;
            }

            while (j++ < 6)
                printf(" ");

            printf("|");
        }
    }
    printf("\n");

    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains != NULL)
        {
            printf("%c%c    |", (registerStates[i]->live ? 'L':' '), (registerStates[i]->dirty ? 'D':' '));
        }
    }
    printf("\n");
}

void freeRegisterStates(struct registerState **s)
{
    for (int i = 0; i < 12; i++)
        free(s[i]);

    free(s);
}

struct registerState **duplicateRegisterStates(struct registerState **theStates)
{
    struct registerState **duplicateStates = malloc(12 * sizeof(struct registerState *));
    for (int i = 0; i < 12; i++)
    {
        duplicateStates[i] = malloc(sizeof(struct registerState));
        memcpy(duplicateStates[i], theStates[i], sizeof(struct registerState));
    }
    return duplicateStates;
}

// find and return the register containing a given temp. variable (since temporaries can be guaranteed to be in a register)
int findInRegisters(struct registerState **registerStates, char *var)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
                return i;

    return -1;
}
