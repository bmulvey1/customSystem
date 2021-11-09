#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *infile;

enum token
{
    t_id,
    t_equals,
    t_literal,
    t_assignment,
    t_notEquals,
    t_lThan,
    t_gThan,
    t_lThanE,
    t_gThanE,
    t_plus,
    t_minus,
    t_times,
    t_divide,
    t_openParen,
    t_closeParen,
    t_openCurly,
    t_closeCurly,
    t_if
};

char *token_names[] = {
    "ID",
    "Literal",
    "Assignment",
    "Equals",
    "NotEquals",
    "LessThan",
    "GreaterThan",
    "LessThanEquals",
    "GreaterThanEquals",
    "Plus",
    "minus",
    "Times",
    "Divide",
    "OpenParen",
    "CloseParen",
    "OpenCurly",
    "CloseCurly",
    "If"};

enum production
{
    p_program,
    p_stmt_list,
    p_stmt,
    p_condition,
    p_expression,
    p_term,
    p_factor,
    p_expression_tail,
    p_term_tail,
    p_factor_tail,
    p_assignment,
    p_condition_op,
    p_addition_op,
    p_multiplication_op,
    p_id,
    p_literal
};

char *production_names[] = {
    "Program",
    "Statement List",
    "Statement",
    "Condition",
    "Expression",
    "Term",
    "Factor",
    "Expression Tail",
    "Term Tail",
    "Factor Tail",
    "Assignment",
    "Condition Operator",
    "Addition Operator",
    "Multiplication Operator",
    "ID",
    "Literal"};

#define BUF_SIZE 16
char buffer[BUF_SIZE];
int buflen;
char inChar;

char lookahead()
{
    long offset = ftell(infile);
    char returnChar = fgetc(infile);
    fseek(infile, offset, SEEK_SET);
    return returnChar;
}

void trimWhitespace()
{
    int whitespace;
    switch (lookahead())
    {
    case ' ':
    case '\n':
    case '\t':
        whitespace = 1;
        break;
    case '#':
        // recursively handling comments is easy
        // will this come at a cost later?
        while (lookahead() != '\n')
        {
            fgetc(infile);
        }
        fgetc(infile);
        trimWhitespace();
    default:
        whitespace = 0;
        break;
    }
    while (whitespace)
    {
        fgetc(infile);
        switch (lookahead())
        {
        case -1:
        case '\0':
        case ' ':
        case '\n':
        case '\t':
            break;
        case '#':
            trimWhitespace();
        default:
            whitespace = 0;
            break;
        }
    }
}

enum token scan()
{
    printf("called scan\n");
    enum token currentToken;
    buflen = 0;
    // check if we're looking for whitespace - are we?
    trimWhitespace();
    int scanning;
    if (infile != EOF)
    {
        scanning = 1;
    }
    else
    {
        scanning = 0;
    }
    while (scanning)
    {
        inChar = fgetc(infile);
        buffer[buflen++] = inChar;
        buffer[buflen] = '\0';
        if (!strcmp(buffer, "if"))
        {
            return t_if;
        }

        switch (inChar)
        {
        case '(':
            currentToken = t_openParen;
            scanning = 0;
            break;
        case ')':
            currentToken = t_closeParen;
            scanning = 0;
            break;
        case '{':
            currentToken = t_openCurly;
            scanning = 0;
            break;
        case '}':
            currentToken = t_closeCurly;
            scanning = 0;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
            currentToken = t_literal;
            break;
        case '+':
            currentToken = t_plus;
            scanning = 0;
            break;
        case '-':
            currentToken = t_minus;
            scanning = 0;
            break;
        case '*':
            currentToken = t_times;
            scanning = 0;
            break;
        case '/':
            currentToken = t_divide;
            scanning = 0;
            break;
        case '=':
            if (lookahead() != '=')
            {
                if (buflen > 2)
                {
                    printf("ERROR: More than 2 [=] in a row!\n");
                    exit(1);
                }
                else if (buflen == 2)
                {
                    currentToken = t_equals;
                    scanning = 0;
                }
                else
                {
                    currentToken = t_assignment;
                    scanning = 0;
                }
            }
            break;
        case -1:
        //case '\0':
        //case ' ':
        //case '\n':
        //case '\t':
            buflen--;
            buffer[buflen] = '\0';
        // don't include whitespace in the buffer
            scanning = 0;
            break;
        default:
            currentToken = t_id;
            break;
        }

        // if there's a paren or bracket coming up
        // terminate the scanning, our token is done
        printf("LOOKAHEAD IS %c : %d\n", lookahead());
        switch (lookahead())
        {
        case '(':
        case ')':
        case '{':
        case '}':
        case ' ':
        case '\t':
        case '\n':
        case '\0':
            scanning = 0;
            break;
        }
    }
    return currentToken;
}

struct PSNode
{
    enum production productionType;
    struct PSNode *next;
    struct PSNode *prev;
};

struct parseStack
{
    struct PSNode *bottom;
    struct PSNode *top;
    int size;
};

void parseStackPush(struct parseStack *it, enum production theProduction)
{
    struct PSNode *newNode = malloc(sizeof(struct PSNode));
    newNode->productionType = theProduction;
    if (it->size == 0)
    {
        it->bottom = newNode;
        it->top = newNode;
    }
    else
    {
        it->top->next = newNode;
        newNode->prev = it->top;
        it->top = newNode;
    }
    it->size++;
}
enum production parseStackPop(struct parseStack *it)
{
    if (it->size == 0)
    {
        printf("popped from empty stack\n");
        exit(1);
    }
    struct PSNode *poppedNode;
    if (it->size == 1)
    {
        poppedNode = it->top;
        it->top = NULL;
        it->bottom = NULL;
    }
    else
    {
        poppedNode = it->top;
        it->top = it->top->prev;
    }
    it->size--;
    enum production retType = poppedNode->productionType;
    free(poppedNode);
    return retType;
}

void stackParse()
{
    struct parseStack *stack = malloc(sizeof(struct parseStack));
    parseStackPush(stack, p_program);
    while (stack->size > 0)
    {
        switch (parseStackPop(stack))
        {
        case p_program:
            parseStackPush(stack, p_stmt_list);
            // need to include EOF token or not?
            break;
        case p_stmt_list:
            switch (scan())
            {
            case t_id:
            case t_if:
            }
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);
    infile = fopen(argv[1], "rb");
    //struct parseStack *stack = malloc(sizeof(struct parseStack));
    //parseStackPush(stack, p_program);
    //stackParse();
    //char inc = fgetc(infile);
    while (infile != EOF)
    {

        //printf("%c\n", lookahead());
        //printf("%c %d\n", inc, inc);
        printf("Scanned %s with token of %s\n", &buffer, token_names[scan()]);
        for (int i = 0; i < 0xffffff; i++)
        {
        }
        //inc = fgetc(infile);
    }
}