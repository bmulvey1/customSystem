#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

FILE *infile;

enum token
{
    t_var,
    t_fun,
    t_name,
    t_literal,
    t_unOp,
    t_binOp,
    t_unAssign,
    t_assign,
    t_semicolon,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly
};

char *token_names[] = {
    "var",
    "function",
    "name",
    "unary operator",
    "binary operator",
    "unary assignment",
    "assignment",
    "semicolon",
    "l paren",
    "r paren",
    "l curly",
    "r curly"};

enum production
{
    p_program,
    p_tldList,
    p_tld,
    p_variableInit,
    p_assignment,
    p_statementList,
    p_statement,
    p_expression,
    p_unOp
};

char *production_names[] = {
    "Program",
    "TLD List",
    "Top Level Definition",
    "Variable Initialization",
    "Assignment",
    "Statement List",
    "Statement",
    "Expression",
    "Unary Operator"

};

#define BUF_SIZE 16
char buffer[BUF_SIZE];
int buflen;
char inChar;

/*
void parseStackPush(struct parseStack *it, enum production theProduction)
{
    printf("Pushing production of type [%s]\n", production_names[theProduction]);
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
        printf("Error - tried to pop from empty stack\n");
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
*/
struct astNode
{
    char *value;
    enum token type;
    struct astNode *child;
    struct astNode *sibling;
};

struct astNode *newastNode(enum token t, char *newdata)
{
    struct astNode *retNode = malloc(sizeof(struct astNode));
    retNode->child = NULL;
    retNode->sibling = NULL;
    retNode->type = t;
    retNode->value = malloc(buflen);
    strcpy(retNode->value, newdata);
    return retNode;
}

void astNode_insertSibling(struct astNode *it, struct astNode *newSibling)
{
    struct astNode *runner = it;
    while (runner->sibling != NULL)
    {
        runner = runner->sibling;
    }

    runner->sibling = newSibling;
}

void astNode_insertChild(struct astNode *it, struct astNode *newChild)
{

    if (it->child == NULL)
    {
        it->child = newChild;
    }
    else
    {
        astNode_insertSibling(it->child, newChild);
    }
}

void printAST(struct astNode *it, int depth)
{
    if (it->child != NULL)
    {
        printAST(it->child, depth + 1);
    }
    for (int i = 0; i < depth; i++)
    {
        printf("\t");
    }
    printf("%s \n", it->value);
    if (it->sibling != NULL)
    {
        printAST(it->sibling, depth);
    }
}

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
    for (int i = 0; i < 0xffffff; i++)
    {
    }
    trimWhitespace();
    char r = lookahead_dumb();
    return r;
}

char *reserved[] = {
    "v",
    "f",
    "(",
    ")",
    "{",
    "}",
    ";",
    "=",
    "+",
    "-"};

enum token reserved_t[] = {
    t_var,
    t_fun,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly,
    t_semicolon,
    t_unAssign,
    t_unOp,
    t_unOp};

enum token scan()
{
    buflen = 0;
    // check if we're looking for whitespace - are we?
    trimWhitespace();
    int scanning;
    if (!feof(infile))
    {
        scanning = 1;
    }
    else
    {
        scanning = 0;
    }
    enum token currentToken;
    while (scanning)
    {
        inChar = fgetc(infile);
        buffer[buflen++] = inChar;
        buffer[buflen] = '\0';
        // Iterate all reserved keywords
        for (int i = 0; i < 10; i++)
        {
            // if we match a reserved keyword
            if (!strcmp(buffer, reserved[i]))
            {
                // return its token
                return reserved_t[i];
            }
        }
        if (buflen == 1)
        {
            if (isdigit(inChar))
            {
                currentToken = t_literal;
            }
            else if (isalpha(inChar))
            {
                currentToken = t_name;
            }
        }
        else
        {
            if (currentToken == t_literal && isalpha(inChar))
            {
                printf("Error - alphabetical character in literal!");
                exit(1);
            }
        }
        // if the next input char is whitespace or a single-character token, we're done
        switch (lookahead_dumb())
        {
        case ' ':
        case '(':
        case ')':
        case '{':
        case '}':
        case ';':
        case '\n':
        case '\t':
        case '+':
        case '-':
            return currentToken;
            break;
        }
    }
}

struct astNode *match(enum token t)
{
    enum token result = scan();
    if (result == t)
    {
        printf("Matched token [%s] with image of [%s]\n", token_names[t], buffer);
        struct astNode *retNode = newastNode(t, buffer);
        return retNode;
    }
    else
    {
        printf("Error matching - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
        return NULL;
    }
}

void consume(enum token t)
{
    enum token result = scan();
    if (result == t)
    {
        printf("Consumed token [%s] with image of [%s]\n", token_names[t], buffer);
    }
    else
    {
        printf("Error consuming - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
    }
}
struct astNode *parseProgram();
struct astNode *parseTLDList();
struct astNode *parseTLD();
struct astNode *parseVariableInit();
struct astNode *parseAssignment();
struct astNode *parseStatementList();
struct astNode *parseStatement();
struct astNode *parseExpression();

struct astNode *parseProgram()
{
    return parseTLDList();
}

struct astNode *parseTLDList()
{
    struct astNode *TLDList = parseTLD();
    while (!feof(infile))
    {
        astNode_insertSibling(TLDList, parseTLD());
    }
    return TLDList;
}

struct astNode *parseTLD()
{
    struct astNode *TLD;
    switch (scan())
    {
    case t_fun:
        TLD = newastNode(t_fun, buffer);
        astNode_insertChild(TLD, match(t_name));
        match(t_lParen);
        match(t_rParen);
        match(t_lCurly);
        astNode_insertChild(TLD, parseStatementList());
        match(t_rCurly);
    case t_var:
        TLD = newastNode(t_var, buffer);
        astNode_insertChild(TLD, match(t_name));
        if (lookahead() == '=')
        {
            astNode_insertChild(TLD, match(t_unAssign));
            astNode_insertChild(TLD, parseExpression());
        }
        consume(t_semicolon);
    }
    return TLD;
}

struct astNode *parseVariableInit()
{
}

struct astNode *parseAssignment()
{
}

struct astNode *parseStatementList()
{
}

struct astNode *parseStatement()
{
}

struct astNode *parseExpression()
{
    printf("parsing expression\n");
    struct astNode *expression = NULL;
    if (isalpha(lookahead()))
        {
            expression = match(t_name);
        }
        else if (isdigit(lookahead()))
        {
            expression = match(t_literal);
        }
        else
        {
            printf("Error - expected literal or name in expression and got [%s]\n", token_names[scan()]);
            exit(1);
        }
    while (1)
    {
        switch (lookahead())
        {
        case '+':
        case '-':
            astNode_insertSibling(expression, match(t_unOp));
            break;
        case ';':
            printf("done parsing expression - here's what we got:\n");
            printAST(expression, 0);
            return expression;
            break;
        default:
            printf("Error - expected unary operator in expression and got [%s]\n", token_names[scan()]);
            exit(1);
            break;
        }

        if (isalpha(lookahead()))
        {
            astNode_insertSibling(expression, match(t_name));
        }
        else if (isdigit(lookahead()))
        {
            astNode_insertSibling(expression, match(t_literal));
        }
        else
        {
            printf("Error - expected literal or name in expression and got [%s]\n", token_names[scan()]);
            exit(1);
        }
    }
}


int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);
    infile = fopen(argv[1], "rb");
    struct astNode *program = parseProgram();
    printAST(program, 0);

    /*while (lookahead() != EOF)
    {
        scan();
        printf("Scanned %s with token of %s\n", &buffer, token_names[currentToken]);
    }*/
    printf("done printing\n");
}