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
    "comma",
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

#define RESERVED_COUNT 12

char *reserved[RESERVED_COUNT] = {
    "var",
    "fun",
    ",",
    "(",
    ")",
    "{",
    "}",
    ";",
    "=",
    "+",
    "-",
    "$$"};

enum token reserved_t[RESERVED_COUNT] = {
    t_var,
    t_fun,
    t_comma,
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
        buffer[buflen++] = inChar;
        buffer[buflen] = '\0';
        if (feof(infile))
            return currentToken;

        // Iterate all reserved keywords
        for (int i = 0; i < RESERVED_COUNT; i++)
        {
            // if we match a reserved keyword
            if (!strcmp(buffer, reserved[i]))
                return reserved_t[i]; // return its token
        }

        // if on the first char of the token
        if (buflen == 1)
        {
            // determine literal or name based on what we started with
            if (isdigit(inChar))
                currentToken = t_literal;
            else if (isalpha(inChar))
                currentToken = t_name;
        }
        else
        {
            // simple error checking for letters in literals
            if (currentToken == t_literal && isalpha(inChar))
            {
                printf("Error - alphabetical character in literal [%s]!\n", buffer);
                exit(1);
            }
        }

        // if the next input char is whitespace or a single-character token, we're done with this token
        switch (lookahead_dumb())
        {
        case ' ':
        case ',':
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

// return the next token that would be scanned without consuming
enum token lookaheadToken()
{
    long offset = ftell(infile);
    enum token retToken = scan();
    fseek(infile, offset, SEEK_SET);
    return retToken;
}

// error-checked method to consume and return AST node of expected token
struct astNode *match(enum token t)
{
    enum token result = scan();
    if (result != t)
    {
        printf("Error matching - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
        exit(1);
    }
    return newastNode(result, buffer);
}

// error-checked method to consume expected token with no return
void consume(enum token t)
{
    enum token result = scan();
    if (result != t)
    {
        printf("Error consuming - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
        exit(1);
    }
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

        astNode_insertSibling(TLDList, parseTLD());
    }
    //printf("done parsing tld list\n");
    return TLDList;
}

struct astNode *parseTLD()
{
    struct astNode *TLD;
    switch (lookaheadToken())
    {
    // f [function name](){[argument list]}
    case t_fun:
        TLD = match(t_fun);
        struct astNode *functionname = match(t_name);
        consume(t_lParen);
        struct astNode *argList = parseArgList();
        astNode_insertChild(functionname, argList);
        astNode_insertChild(TLD, functionname);
        consume(t_rParen);
        consume(t_lCurly);
        astNode_insertChild(TLD, parseStatementList());
        consume(t_rCurly);
        break;

    // v [variable name];
    // v [variable name] = [expression];
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

struct astNode *parseAssignment(struct astNode *name)
{
    // pre-parsed name node taken as argument
    // [name] = [expression]
    struct astNode *assign = match(t_assign);
    astNode_insertChild(assign, name);
    astNode_insertChild(assign, parseExpression());
    return assign;
}

struct astNode *parseStatementList()
{
    struct astNode *stmtList = NULL;
    // loop until we see something that's not a statement
    int parsing = 1;
    while (parsing)
    {
        struct astNode *stmt = NULL;
        enum token nextToken = lookaheadToken();
        switch (nextToken)
        {

        // v [variable name];
        // v [variable name] = [expression];
        // [variable name] = [expression];
        case t_var:
        case t_name:
            stmt = parseStatement();
            if (stmtList == NULL)
                stmtList = stmt;
            else
                astNode_insertSibling(stmtList, stmt);
            break;

        // currently this is the only token that can terminate a list
        case t_rCurly:
            parsing = 0;
            break;

        default:
            printf("Error parsing statement list - saw token %s with value of [%s]\n", token_names[nextToken], buffer);
            exit(1);
        }
    }

    //printf("done parsing statement list");
    return stmtList;
}

struct astNode *parseStatement()
{
    struct astNode *statement = NULL;
    enum token upcomingToken = lookaheadToken();
    switch (upcomingToken)
    {
    // v [variable name];
    // v [variable name] = [expression];
    case t_var:
    {
        statement = match(t_var);
        struct astNode *name = match(t_name);

        // check whether or not whether this is an assignment or just a declaration
        if (lookaheadToken() == t_assign)
            astNode_insertChild(statement, parseAssignment(name));
        else
        {
            //printf("var with no assignment\n");
            astNode_insertChild(statement, name);
        }

        consume(t_semicolon);
    }
    break;

    // [variable name] = [expression];
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
    //printf("Done parsing statement- heres what we got\n");
    //printAST(statement, 0);
    return statement;
}

struct astNode *parseExpression()
{
    //printf("parsing expression\n");
    struct astNode *expression = NULL;

    // figure out what the left side of the expression is
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

    // now, figure out whether there is a right side
    switch (lookahead())
    {
    // [left side][operator][right side]
    case '+':
    case '-':
        expression = match(t_unOp);
        astNode_insertChild(expression, lSide);
        astNode_insertChild(expression, parseExpression());
        break;

    // end of line or end of expression, there isn't anything more than the left side
    case ';':
    case ')':
        expression = lSide;
        return expression;
        break;

    default:
        printf("Error - expected unary operator or terminator in expression and got [%s]\n", token_names[scan()]);
        exit(1);
        break;
    }

    //printf("done parsing expression - here's what we got:\n");
    //printAST(expression, 0);
    return expression;
}

struct astNode *parseArgList()
{
    struct astNode *argList = NULL;
    int parsing = 1;
    while (parsing)
    {

        switch (lookahead())
        {
        case 'v':
        {
            struct astNode *argument = match(t_var);
            astNode_insertChild(argument, match(t_name));

            if (argList == NULL)
                argList = argument;
            else
                astNode_insertSibling(argList, argument);
        }
        break;

        case ',':
            consume(t_comma);
            break;

        default:
            parsing = 0;
            break;
        }
    }
    return argList;
}