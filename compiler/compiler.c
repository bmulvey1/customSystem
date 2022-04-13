#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "asm.h"
#include "codegen.h"
#include "serialize.h"

int compareBasicBlockStartIndices(struct BasicBlock *a, struct BasicBlock *b)
{
    return ((struct TACLine *)a->TACList->head->data)->index < ((struct TACLine *)b->TACList->head->data)->index;
}

// this thing causes constant problems
// probably makes sense to do this externally to the compiler
void prettyPrintBasicBlocks(struct symbolTable *theTable)
{
    printf("Basic Blocks for %s\n", theTable->name);
    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct functionEntry *thisEntry = theTable->entries[i]->entry;

            struct Stack *blockStack = Stack_new();
            for (struct LinkedListNode *runner = thisEntry->table->BasicBlockList->tail; runner != NULL; runner = runner->prev)
            {
                struct BasicBlock *thisBlock = runner->data;
                if (thisBlock->containsEffectiveCode)
                    Stack_push(blockStack, thisBlock);
            }

            for (struct LinkedListNode *runner = thisEntry->table->BasicBlockList->head; runner != NULL; runner = runner->next)
                printBasicBlock(runner->data, 0);

            int blockCount = blockStack->size;
            struct LinkedListNode **printArray = malloc(blockCount * sizeof(struct BasicBlock *));
            int printArraySize = 0;
            char **blockTitles = malloc(blockCount * sizeof(char *));
            for (int i = 0; i < blockCount; i++)
            {
                printArray[i] = NULL;
                blockTitles[i] = NULL;
            }

            for (int i = 0; i < blockStack->size; i++)
                for (int j = 0; j < blockStack->size - i - 1; j++)
                    if (compareBasicBlockStartIndices(blockStack->data[j], blockStack->data[j + 1]))
                    {
                        void *temp = blockStack->data[j];
                        blockStack->data[j] = blockStack->data[j + 1];
                        blockStack->data[j + 1] = temp;
                    }

            int TACIndex = -1;

            // printf("THERE ARE %d blocks\n", blockCount);
            while (blockStack->size > 0 || printArraySize > 0)
            // while (1)
            {
                // if it's time to bring in the next basic block
                // printf("TACINDEX %d\n", TACIndex);

                while (blockStack->size > 0)
                {
                    struct BasicBlock *peekedBlock = Stack_peek(blockStack);

                    if (peekedBlock->TACList->head == NULL)
                    {
                        Stack_pop(blockStack);
                        continue;
                    }
                    if (((struct TACLine *)peekedBlock->TACList->head->data)->index <= TACIndex + 1)
                    {
                        for (int i = 0; i < blockCount; i++)
                            if (printArray[i] == NULL)
                            {
                                struct BasicBlock *introducedBlock = Stack_pop(blockStack);
                                blockTitles[i] = malloc(16);
                                sprintf(blockTitles[i], "Block %d", introducedBlock->labelNum);
                                printArray[i] = introducedBlock->TACList->head;

                                // printf("[BASIC BLOCK %d]\n", introducedBlock->labelNum);
                                // printf("introduce block %d (start index of %d) to array index %d\n", introducedBlock->labelNum, ((struct TACLine *)printArray[i]->data)->index, i);

                                if (i > printArraySize)
                                    printArraySize = i;

                                break;
                            }
                    }
                    else
                        break;
                }
                for (int i = 0; i <= printArraySize; i++)
                {
                    if (printArray[i] == NULL && blockTitles[i] != NULL)
                    {
                        printf("                        |");
                    }
                    else
                    {
                        while (printArray[i] != NULL && !TACLine_isEffective(printArray[i]->data))
                        {
                            printArray[i] = printArray[i]->next;
                        }
                    }

                    if (printArray[i] != NULL)
                    {
                        if (((struct TACLine *)printArray[i]->data)->index == TACIndex)
                        {
                            printTACLine(printArray[i]->data);
                            printf("|");
                            printArray[i] = printArray[i]->next;
                        }
                        else
                        {
                            if (blockTitles[i] != NULL)
                            {
                                printf("%16s        |", blockTitles[i]);
                                free(blockTitles[i]);
                                blockTitles[i] = NULL;
                            }
                            /*else
                            {
                                printf("                        ");
                            }*/
                        }
                    }
                    else
                    {
                        printf("------------------------|");
                    }
                }

                printf("\n");

                printArraySize = 0;
                for (int i = blockCount - 1; i >= 0; i--)
                {
                    if (printArray[i] != NULL)
                    {
                        printArraySize = i + 1;
                        break;
                    }
                }

                TACIndex++;
            }

            free(printArray);
            free(blockTitles);
            Stack_free(blockStack);
        }
    }
    printf("\n\n");
}

void printBasicBlocks(struct symbolTable *theTable)
{
    printf("Basic Blocks for %s\n", theTable->name);
    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct functionEntry *thisEntry = theTable->entries[i]->entry;

            for (struct LinkedListNode *runner = thisEntry->table->BasicBlockList->head; runner != NULL; runner = runner->next)
            {
                printBasicBlock(runner->data, 0);
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Error - please specify an input and output file!\n");
        exit(1);
    }
    else if (argc < 3)
    {
        printf("Error - please specify an output file!\n");
        exit(1);
    }
    printf("Parsing program from %s\n", argv[1]);

    printf("Generating output to %s\n", argv[2]);
    struct Dictionary *parseDict = newDictionary(10);
    struct astNode *program = parseProgram(argv[1], parseDict);

    serializeAST("astdump", program);
    printf("\n");

    printAST(program, 0);
    printf("Generating symbol table from AST");
    struct symbolTable *theTable = walkAST(program);
    printf("\n");

    printf("Linearizing code to TAC");
    linearizeProgram(program, theTable);
    printf("\n\n");

    printBasicBlocks(theTable);

    printSymTab(theTable, 1);
    printf("\n\n");

    FILE *outFile = fopen(argv[2], "wb");
    // struct Lifetime *theseLifetimes = findLifetimes(theTable);
    struct ASMblock *output;
    output = generateCode(theTable, outFile);
    struct ASMline *L1 = output->head;
    struct ASMline *L2 = L1->next;
    output->head = L2->next;
    free(L1->data);
    free(L1);
    free(L2->data);
    free(L2);
    ASMblock_output(output, outFile);
    ASMblock_free(output);
    // exit(1);
    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct functionEntry *thisEntry = theTable->entries[i]->entry;
            output = generateCode(thisEntry->table, outFile);
            // for (struct Lifetime *ltRunner = theseLifetimes; ltRunner != NULL; ltRunner = ltRunner->next)
            // {
            // printf("Var [%8s]: %2x - %2x\n", ltRunner->variable, ltRunner->start, ltRunner->end);
            // }

            // run along all the lines of asm output from this funtcion, printing and freeing as we go
            ASMblock_output(output, outFile);
            ASMblock_free(output);
        }
    }
    

    fclose(outFile);
    freeDictionary(parseDict);
    freeAST(program);
    freeSymTab(theTable);

    printf("done printing\n");
}
