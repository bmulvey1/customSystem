#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *infile;

enum token
{
    t_constant,
    t_name,
    t_auto,
    t_if,
    t_unEquals,
    t_unMinus,
    t_unNot,
    t_binOr,
    t_binAnd,
    t_binEquals,
    t_binNotEquals,
    t_binLThan,
    t_binLThanE,
    t_binGThan,
    t_binGThanE,
    t_binLShift,
    t_binRShift,
    t_binMinus,
    t_binPlus,
    t_binMod,
    t_binMultiply,
    t_binDivide,
    t_singleQuote,
    t_doubleQuote,
    t_lCurly,
    t_rCurly,
    t_lParen,
    t_rParen,
    t_lSquare,
    t_rSquare

};

char *token_names[] = {
    "Constant",
    "Name",
    "Auto",
    "if",
    "Unary Equals",
    "Unary Minus",
    "Unary Not",
    "-",
    "Binary And",
    "Binary Equals",
    "Binary Not Equals",
    "Binary Less Than",
    "Binary Less Than or Equal",
    "Binary Greater Than",
    "Binary Greater Than or Equal",
    "Binary Left Shift",
    "Binary Right Shift",
    "Binary Minus",
    "Binary Plus",
    "Binary Modulo",
    "Binary Multiply",
    "Binary Divide",
    "Single Quote",
    "Double Quote",
    "Left Curly",
    "Right Curly",
    "Left Paren",
    "Right Paren",
    "Left Square Bracket",
    "Right Square Bracket"};

enum production
{
    p_program,
    p_definition,
    p_name,
    p_constant,
    p_ival,
    p_statement,
    p_auto,
    p_semicolon,
    p_rvalue,
    p_lvalue,
    p_assign,
    p_unary,
    p_binary,
    p_alpha,
    p_digit,
    p_alphadigit,
    p_lparen,
    p_rparen,
    p_lcurly,
    p_rcurly,
    p_lsquare,
    p_rsquare
};

char *production_names[] = {
    "Program",
    "Definition",
    "Name",
    "Constant",
    "Ival",
    "Statement",
    "Auto",
    ";",
    "rvalue",
    "lvalue",
    "=",
    "unary op",
    "binary op",
    "alpha",
    "digit",
    "alphadigit",
    "(",
    ")",
    "{",
    "}",
    "[",
    "]"};

#define BUF_SIZE 16
char buffer[BUF_SIZE];
int buflen;
enum token currentToken;
char inChar;

struct parseStack *upcomingStack;
struct parseStack *inProgressStack;
struct ASTStack *astStack;

char lookahead_dumb()
{
    long offset = ftell(infile);
    char returnChar = fgetc(infile);
    fseek(infile, offset, SEEK_SET);
    return returnChar;
}

void trimWhitespace()
{
    int whitespace;
    switch (lookahead_dumb())
    {
    case ' ':
    case '\n':
    case '\t':
        whitespace = 1;
        break;
    case '#':
        // recursively handling comments is easy
        // will this come at a cost later?
        while (lookahead_dumb() != '\n')
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
        switch (lookahead_dumb())
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

char lookahead()
{
    trimWhitespace();
    return lookahead_dumb();
}

void scan()
{
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
            currentToken = t_if;
            return;
        }
        else if (!strcmp(buffer, "auto"))
        {
            currentToken = t_auto;
            return;
        }

        switch (inChar)
        {
        case '(':
            currentToken = t_lParen;
            scanning = 0;
            break;
        case ')':
            currentToken = t_rParen;
            scanning = 0;
            break;
        case '{':
            currentToken = t_lCurly;
            scanning = 0;
            break;
        case '}':
            currentToken = t_rCurly;
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
            if (buflen == 1)
            {
                currentToken = t_constant;
            }
            else if (currentToken != t_name)
            {
                printf("Error - unexpected char [%c] in token [%s] \n", inChar, buffer);
            }
            break;
        case '=':
            switch (currentToken)
            {
            case t_binEquals:
                currentToken = t_binEquals;
                scanning = 0;
                break;
            case t_unNot:
                currentToken = t_binNotEquals;
                scanning = 0;
                break;
            case t_binLThan:
                currentToken = t_binLThanE;
                scanning = 0;
                break;
            case t_binGThan:
                currentToken = t_binGThanE;
                scanning = 0;
                break;

            default:
                if (lookahead() != '=')
                {
                    if (buflen > 2)
                    {
                        printf("ERROR: More than 2 [=] in a row!\n");
                        exit(1);
                    }
                    else if (buflen == 2)
                    {
                        currentToken = t_binEquals;
                        scanning = 0;
                    }
                    else
                    {
                        currentToken = t_unEquals;
                        scanning = 0;
                    }
                }
                break;
            }
            break;
        case '|':
            currentToken = t_binOr;
            scanning = 0;
            break;

        case '&':
            currentToken = t_binAnd;
            scanning = 0;
            break;

        case '!':
            currentToken = t_unNot;
            if (lookahead() != '=') // only thing that can extend ! in a token is an =
            {
                scanning = 0;
            }
            break;

        case '<':
            if (buflen == 1) // only 1 '<' so far
            {
                currentToken = t_binLThan;
                if (lookahead() != '=' && lookahead() != '<') // only thing that can extend < in a token is an = or <
                {
                    scanning = 0;
                }
            }
            else // only way to have a < as the second char is to have lShift token
            {
                currentToken = t_binLShift;
                scanning = 0;
            }
            break;

        case '>':
            if (buflen == 1)
            {
                currentToken = t_binGThan;
                if (lookahead() != '=' && lookahead() != '>') // only thing that can extend > in a token is an = or >
                {
                    scanning = 0;
                }
            }
            else // only way to have a > as the second char is to have rShift token
            {
                currentToken = t_binRShift;
                scanning = 0;
            }
            break;

        case '-':
            currentToken = t_binMinus;
            scanning = 0;
            break;

        case '+':
            currentToken = t_binPlus;
            scanning = 0;
            break;

        case '%':
            currentToken = t_binMod;
            scanning = 0;
            break;

        case '*':
            currentToken = t_binMultiply;
            scanning = 0;
            break;

        case '/':
            currentToken = t_binDivide;
            scanning = 0;
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
            currentToken = t_name;
            break;
        }

        // if there's a paren or bracket coming up
        // terminate the scanning, our token is done
        //printf("LOOKAHEAD IS %c : %d\n", lookahead());
        switch (lookahead_dumb())
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

struct ASTNode
{
    char *value;
    enum token type;
    struct ASTNode *child;
    struct ASTNode *sibling;
};

void ASTNode_InsertSibling(struct ASTNode *it, struct ASTNode *newSibling)
{
    while (it->sibling != NULL)
    {
        it = it->sibling;
    }
    it->sibling = newSibling;
}

void printAST(struct ASTNode *it)
{
    if (it->child != NULL)
    {
        printAST(it->child);
    }
    printf("%s ", it->value);
    if (it->sibling != NULL)
    {
        printAST(it->sibling);
    }
}

struct ASTStackNode
{
    struct ASTNode *data;
    struct ASTStackNode *next;
    struct ASTStackNode *prev;
};

struct ASTStack
{
    struct ASTStackNode *bottom;
    struct ASTStackNode *top;
    int size;
};

void ASTStackPush()
{
    struct ASTNode *newNode = malloc(sizeof(struct ASTNode));
    newNode->value = malloc(strlen(buffer));
    strcpy(newNode->value, buffer);
    newNode->type = currentToken;

    struct ASTStackNode *newStackNode = malloc(sizeof(struct ASTStackNode));
    newStackNode->data = newNode;

    if (astStack->size == 0)
    {
        astStack->bottom = newStackNode;
        astStack->top = newStackNode;
    }
    else
    {
        astStack->top->next = newStackNode;
        newStackNode->prev = astStack->top;
        astStack->top = newStackNode;
    }
    astStack->size++;
}

struct ASTNode *ASTStackPop()
{
    if (astStack->size == 0)
    {
        printf("popped from empty stack\n");
        exit(1);
    }
    struct ASTNode *poppedData;
    struct ASTStackNode *poppedNode;
    if (astStack->size == 1)
    {
        poppedNode = astStack->top;
        astStack->top = NULL;
        astStack->bottom = NULL;
    }
    else
    {
        poppedNode = astStack->top;
        astStack->top = astStack->top->prev;
    }
    astStack->size--;
    poppedData = poppedNode->data;
    free(poppedNode);
    return poppedData;
}

void match()
{
    ASTStackPush(astStack);
    scan();
}

void stackParse()
{
    parseStackPush(upcomingStack, p_program);
    scan();
    while (upcomingStack->size > 0)
    {
        scan();
        switch (parseStackPop(upcomingStack))
        {
        case p_program:
            parseStackPush(upcomingStack, p_definition);
            // need to include EOF token or not?
            break;
        case p_definition:
            switch (currentToken)
            {
            case t_name:
                switch (lookahead())
                {
                case '[': // name {[{constant01}]} {ival {, ival}0}01;
                    ASTStackPush();
                    scan();

                    if (currentToken == t_constant) // constant is present
                    {
                        ASTStackPush();
                        scan();
                    }

                    if (lookahead() == ']') // must see close square bracket, otherwise error
                    {
                        ASTStackPush();
                    }
                    else
                    {
                        printf("Error - expected ']', got %s instead!\n", buffer);
                        exit(1);
                    }
                    break;
                default:
                    break;
                }
                break;
            default:
                printf("Unexpected token %s [%s], expected [%s]\n", token_names[currentToken], buffer, token_names[t_name]);
                exit(1);
            }
            break;
            enum production workingOn = parseStackPop(upcomingStack);
            // keep track of what's being worked on so we can shift and reduce
            parseStackPush(inProgressStack, workingOn);
            switch (workingOn)
            {
            case p_program:
                parseStackPush(upcomingStack, p_definition);
                // need to include EOF token or not?
                break;
            }
            break;
        }
    }
}

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);
    infile = fopen(argv[1], "rb");
    struct parseStack *stack = malloc(sizeof(struct parseStack));
    parseStackPush(stack, p_program);
    stackParse();
    //char inc = fgetc(infile);

    /*while (lookahead() != EOF)
    {

        //printf("%c\n", lookahead());
        //printf("%c %d\n", inc, inc);
        scan();
        printf("Scanned %s with token of %s\n", &buffer, token_names[currentToken]);
        for (int i = 0; i < 0xffffff; i++)
        {
        }
        //inc = fgetc(infile);
    }*/
}