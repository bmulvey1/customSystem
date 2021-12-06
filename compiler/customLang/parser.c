#include "parser.h"

FILE *infile;
#define BUF_SIZE 16
char buffer[BUF_SIZE];
int buflen;
char inChar;
char *token_names[] = {
    "var",
    "function",
    "name",
    "literal",
    "unary operator",
    "binary operator",
    "assignment",
    "semicolon",
    "l paren",
    "r paren",
    "l curly",
    "r curly",
    "EOF"};

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
            fgetc(infile);

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
    "-",
    "$$"};

enum token reserved_t[] = {
    t_var,
    t_fun,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly,
    t_semicolon,
    t_assign,
    t_unOp,
    t_unOp,
    t_EOF};

enum token scan()
{
    buflen = 0;
    // check if we're looking at whitespace - are we?
    trimWhitespace();
    if (feof(infile))
        return t_EOF;

    enum token currentToken;

    while (1)
    {
        inChar = fgetc(infile);
        if (feof(infile))
            return currentToken;

        buffer[buflen++] = inChar;
        buffer[buflen] = '\0';
        // Iterate all reserved keywords
        for (int i = 0; i < 11; i++)
        {
            // if we match a reserved keyword
            if (!strcmp(buffer, reserved[i]))
                return reserved_t[i]; // return its token
        }
        if (buflen == 1)
        {
            if (isdigit(inChar))
                currentToken = t_literal;
            else if (isalpha(inChar))
                currentToken = t_name;
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

enum token lookaheadToken()
{
    long offset = ftell(infile);
    enum token retToken = scan();
    fseek(infile, offset, SEEK_SET);
    return retToken;
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
        printf("Consumed token [%s] with image of [%s]\n", token_names[t], buffer);
    else
        printf("Error consuming - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
}

char *getTokenName(enum token t)
{
    return token_names[t];
}

struct astNode *parseProgram(char *inFileName)
{
    infile = fopen(inFileName, "rb");
    return parseTLDList();
}

struct astNode *parseTLDList()
{
    struct astNode *TLDList = parseTLD();
    while (1)
    {
        if (lookaheadToken() == t_EOF)
            break;
        //printf("NEXT TOKEN IS\n %s\n", token_names[lookaheadToken()]);
        astNode_insertSibling(TLDList, parseTLD());
        //trimWhitespace();
    }
    printf("done parsing tld list\n");
    return TLDList;
}

struct astNode *parseTLD()
{
    struct astNode *TLD;
    printf("IN TLD TOKEN IS %s\n", token_names[lookaheadToken()]);

    switch (lookaheadToken())
    {
    case t_fun:
        TLD = match(t_fun);
        astNode_insertChild(TLD, match(t_name));
        consume(t_lParen);
        consume(t_rParen);
        consume(t_lCurly);
        astNode_insertChild(TLD, parseStatementList());
        consume(t_rCurly);
        break;

    case t_var:
    {
        TLD = match(t_var);
        struct astNode *name = match(t_name);
        if (lookahead() == '=')
        {
            struct astNode *assignment = match(t_assign);
            astNode_insertChild(assignment, name);
            astNode_insertChild(assignment, parseExpression());
            astNode_insertChild(TLD, assignment);
        }
        else
            astNode_insertChild(TLD, name);

        consume(t_semicolon);
    }
    break;
    default:
        printf("Expected function or variable in Top Level Declaration");
        exit(1);
    }
    return TLD;
}

struct astNode *parseVariableInit()
{
    return NULL;
}

struct astNode *parseAssignment(struct astNode *name)
{
    struct astNode *assign = match(t_assign);
    astNode_insertChild(assign, name);
    astNode_insertChild(assign, parseExpression());
    return assign;
}

struct astNode *parseStatementList()
{
    struct astNode *stmtList = NULL;
    int parsing = 1;
    while (parsing)
    {
        struct astNode *stmt = NULL;
        enum token nextToken = lookaheadToken();
        switch (nextToken)
        {
        case t_var:
        case t_name:
            stmt = parseStatement();
            if (stmtList == NULL)
                stmtList = stmt;
            else
                astNode_insertSibling(stmtList, stmt);
            break;
        case t_rCurly:
            parsing = 0;
            break;
        default:
            printf("Error parsing statement list - saw token %s with value of [%s]\n", token_names[nextToken], buffer);
            exit(1);
        }
    }

    printf("done parsing statement list");
    return stmtList;
}

struct astNode *parseStatement()
{
    struct astNode *statement = NULL;
    enum token upcomingToken = lookaheadToken();
    switch (upcomingToken)
    {
    case t_var:
    {
        statement = match(t_var);
        struct astNode *name = match(t_name);
        if (lookaheadToken() == t_assign)
            astNode_insertChild(statement, parseAssignment(name));
        else
            astNode_insertChild(statement, name);
        
        consume(t_semicolon);
        break;
    }
    case t_name:
    {
        struct astNode *name = match(t_name);
        statement = parseAssignment(name);
        consume(t_semicolon);
    }
    break;
    default:
        printf("Error parsing statement - saw token [%s]\n", token_names[upcomingToken]);
        exit(1);
    }
    printf("Done parsing statement- heres what we got\n");
    printAST(statement, 0);
    return statement;
}

struct astNode *parseExpression()
{
    printf("parsing expression\n");
    struct astNode *expression = NULL;
    struct astNode *lSide = NULL;
    if (isalpha(lookahead()))
        lSide = match(t_name);
    else if (isdigit(lookahead()))
        lSide = match(t_literal);
    else if (lookahead() == '(')
    {
        consume(t_lParen);
        lSide = parseExpression();
        consume(t_rParen);
    }
    else
    {
        printf("Error - expected literal or name in expression and got [%s]\n", token_names[scan()]);
        exit(1);
    }

    switch (lookahead())
    {
    case '+':
    case '-':
        expression = match(t_unOp);
        astNode_insertChild(expression, lSide);
        astNode_insertChild(expression, parseExpression());
        break;
    case ';':
    case ')':
        printf("done parsing expression - here's what we got:\n");
        expression = lSide;
        printAST(expression, 0);
        return expression;
        break;
    default:
        printf("Error - expected unary operator in expression and got [%s]\n", token_names[scan()]);
        exit(1);
        break;
    }
    printf("done parsing expression - here's what we got:\n");
    printAST(expression, 0);
    return expression;
}