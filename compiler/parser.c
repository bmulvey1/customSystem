#include "parser.h"

FILE *infile;
#define BUF_SIZE 16
char buffer[BUF_SIZE];
int buflen;
char inChar;
char *token_names[] = {
    "var",
    "function",
    "return",
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

#define RESERVED_COUNT 21

char *reserved[RESERVED_COUNT] = {
    "var",
    "fun",
    "return",
    "if",
    "else",
    "while",
    ",",
    "(",
    ")",
    "{",
    "}",
    ";",
    "=",
    "+",
    "-",
    ">",
    "<",
    ">=",
    "<=",
    "==",
    "$$"};

enum token reserved_t[RESERVED_COUNT] = {
    t_var,
    t_fun,
    t_return,
    t_if,
    t_else,
    t_while,
    t_comma,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly,
    t_semicolon,
    t_assign,
    t_unOp,
    t_unOp,
    t_compOp,
    t_compOp,
    t_compOp,
    t_compOp,
    t_compOp,
    t_EOF};

enum token scan()
{
    buflen = 0;
    // check if we're looking at whitespace - are we?
    trimWhitespace();
    if (feof(infile))
        return t_EOF;

    enum token currentToken = -1;

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
            {
                // allow catching both '<', '>', '=', and '<=', '>=', '=='
                if (buffer[0] == '<' || buffer[0] == '>' || buffer[0] == '=')
                {
                    if (lookahead() != '=')
                    {
                        return reserved_t[i];
                    }
                }
                else
                {
                    return reserved_t[i]; // return its token
                }
            }
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
struct astNode *match(enum token t, struct Dictionary *dict)
{
    enum token result = scan();
    if (result != t)
    {
        printf("Error matching - expected token [%s], got [%s] with image of [%s] instead!\n", token_names[t], token_names[result], buffer);
        exit(1);
    }
    return newastNode(result, DictionaryLookupOrInsert(dict, buffer));
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

struct astNode *parseProgram(char *inFileName, struct Dictionary *dict)
{
    infile = fopen(inFileName, "rb");
    return parseTLDList(dict);
    fclose(infile);
}

struct astNode *parseTLDList(struct Dictionary *dict)
{
    struct astNode *TLDList = parseTLD(dict);
    while (1)
    {
        if (lookaheadToken() == t_EOF)
            break;

        astNode_insertSibling(TLDList, parseTLD(dict));
    }
    // printf("done parsing tld list\n");
    return TLDList;
}

struct astNode *parseTLD(struct Dictionary *dict)
{
    struct astNode *TLD;
    switch (lookaheadToken())
    {
    // fun [function name]({[argument list]})
    case t_fun:
        TLD = match(t_fun, dict);
        struct astNode *functionname = match(t_name, dict);
        consume(t_lParen);
        struct astNode *argList = parseArgDefinitions(dict);
        astNode_insertChild(functionname, argList);
        astNode_insertChild(TLD, functionname);
        consume(t_rParen);
        consume(t_lCurly);
        astNode_insertChild(TLD, parseStatementList(dict));
        consume(t_rCurly);
        break;

    // var [variable name];
    // var [variable name] = [expression];
    case t_var:
    {
        TLD = match(t_var, dict);
        struct astNode *name = match(t_name, dict);
        if (lookahead() == '=')
        {
            struct astNode *assignment = match(t_assign, dict);
            astNode_insertChild(assignment, name);
            astNode_insertChild(assignment, parseExpression(dict));
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

struct astNode *parseAssignment(struct astNode *name, struct Dictionary *dict)
{
    // pre-parsed name node taken as argument
    // [name] = [expression]
    struct astNode *assign = match(t_assign, dict);
    astNode_insertChild(assign, name);
    astNode_insertChild(assign, parseExpression(dict));
    return assign;
}

struct astNode *parseStatementList(struct Dictionary *dict)
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
        // return [expression];
        case t_var:
        case t_name:
        case t_return:
        case t_if:
        case t_while:
            stmt = parseStatement(dict);
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

    // printf("done parsing statement list");
    return stmtList;
}

struct astNode *parseStatement(struct Dictionary *dict)
{
    struct astNode *statement = NULL;
    enum token upcomingToken = lookaheadToken();
    switch (upcomingToken)
    {
    // v [variable name];
    // v [variable name] = [expression];
    case t_var:
    {
        statement = match(t_var, dict);
        struct astNode *name = match(t_name, dict);

        // check whether or not whether this is an assignment or just a declaration
        if (lookaheadToken() == t_assign)
            astNode_insertChild(statement, parseAssignment(name, dict));
        else
        {
            // printf("var with no assignment\n");
            astNode_insertChild(statement, name);
        }

        consume(t_semicolon);
    }
    break;

    // [variable name] = [expression];
    case t_name:
    {
        struct astNode *name = match(t_name, dict);
        switch (lookahead())
        {
        case '=':
            // printf("assignment\n");
            statement = parseAssignment(name, dict);
            break;

        case '(':
            // printf("function call\n");
            statement = parseFunctionCall(name, dict);
            break;

        default:
            printf("Error - expected '(' or '=' after name\n");
            exit(1);
        }
        consume(t_semicolon);
    }
    break;

    case t_return:
    {
        statement = match(t_return, dict);
        struct astNode *returnAssignment = newastNode(t_assign, "=");
        astNode_insertChild(returnAssignment, newastNode(t_name, ".RETVAL"));
        astNode_insertChild(returnAssignment, parseExpression(dict));
        astNode_insertChild(statement, returnAssignment);
        consume(t_semicolon);
    }
    break;

    case t_if:
        statement = parseIfStatement(dict);
        break;

    case t_while:
        statement = parseWhileLoop(dict);
        break;

    default:
        printf("Error parsing statement - saw token [%s] with value of \n", token_names[upcomingToken]);
        exit(1);
    }
    // printf("Done parsing statement- heres what we got\n");
    // printAST(statement, 0);
    return statement;
}

struct astNode *parseExpression(struct Dictionary *dict)
{
    // printf("parsing expression\n");
    struct astNode *expression = NULL;

    // figure out what the left side of the expression is
    struct astNode *lSide = NULL;
    if (isalpha(lookahead()))
    {
        struct astNode *name = match(t_name, dict);
        if (lookahead() == '(')
        {
            lSide = parseFunctionCall(name, dict);
        }
        else
        {
            lSide = name;
        }
    }
    else if (isdigit(lookahead()))
        lSide = match(t_literal, dict);
    else if (lookahead() == '(')
    {
        consume(t_lParen);
        lSide = parseExpression(dict);
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
        expression = match(t_unOp, dict);
        astNode_insertChild(expression, lSide);
        astNode_insertChild(expression, parseExpression(dict));
        break;

    case '<':
    case '>':
    case '=':
        expression = match(t_compOp, dict);
        astNode_insertChild(expression, lSide);
        astNode_insertChild(expression, parseExpression(dict));
        break;

    // end of line or end of expression, there isn't anything more than the left side
    case ';':
    case ',':
    case ')':
        expression = lSide;
        return expression;
        break;

    default:
        printf("Error - expected unary operator or terminator in expression and got [%s]\n", token_names[scan()]);
        exit(1);

        break;
    }

    // printf("done parsing expression - here's what we got:\n");
    // printAST(expression, 0);
    return expression;
}

struct astNode *parseArgDefinitions(struct Dictionary *dict)
{
    struct astNode *argList = NULL;
    int parsing = 1;
    while (parsing)
    {

        switch (lookahead())
        {
        case 'v':
        {
            struct astNode *argument = match(t_var, dict);
            astNode_insertChild(argument, match(t_name, dict));

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

struct astNode *parseArgList(struct Dictionary *dict)
{
    struct astNode *argList = NULL;
    int parsing = 1;
    while (parsing)
    {
        switch (lookaheadToken())
        {
        case t_rParen:
            parsing = 0;
            break;

        case t_comma:
            consume(t_comma);
            break;

        case t_literal:
            /*if (argList == NULL)
                argList = match(t_literal, dict);
            else
                astNode_insertSibling(argList, match(t_literal, dict));
            break;*/

        default:
            if (argList == NULL)
                argList = parseExpression(dict);
            else
                astNode_insertSibling(argList, parseExpression(dict));
            break;
        }
    }
    return argList;
}

struct astNode *parseFunctionCall(struct astNode *name, struct Dictionary *dict)
{
    consume(t_lParen);
    struct astNode *callNode = newastNode(t_call, "call");
    astNode_insertChild(callNode, name);
    astNode_insertChild(name, parseArgList(dict));
    consume(t_rParen);
    return callNode;
}

struct astNode *parseIfStatement(struct Dictionary *dict)
{
    struct astNode *ifStatement = match(t_if, dict);
    consume(t_lParen);
    astNode_insertChild(ifStatement, parseExpression(dict));
    consume(t_rParen);
    struct astNode *doBlock = newastNode(t_do, "do");
    astNode_insertChild(ifStatement, doBlock);

    if (lookahead() == '{')
    {
        consume(t_lCurly);
        astNode_insertChild(doBlock, parseStatementList(dict));
        consume(t_rCurly);
    }
    else
    {
        astNode_insertChild(doBlock, parseStatement(dict));
    }

    if (lookaheadToken() == t_else)
    {
        astNode_insertChild(ifStatement, parseElseStatement(dict));
    }

    // printf("done parsing if statement - here's what we got!\n");
    // printAST(ifStatement, 0);
    return ifStatement;
}

struct astNode *parseElseStatement(struct Dictionary *dict)
{
    struct astNode *elseStatement = match(t_else, dict);
    struct astNode *doBlock = newastNode(t_do, "do");
    astNode_insertChild(elseStatement, doBlock);
    if (lookahead() == '{')
    {
        consume(t_lCurly);
        astNode_insertChild(doBlock, parseStatementList(dict));
        consume(t_rCurly);
    }
    else
    {
        astNode_insertChild(doBlock, parseStatement(dict));
    }

    return elseStatement;
}

struct astNode *parseWhileLoop(struct Dictionary *dict)
{
    struct astNode *whileLoop = match(t_while, dict);
    consume(t_lParen);
    astNode_insertChild(whileLoop, parseExpression(dict));
    consume(t_rParen);
    struct astNode *doBlock = newastNode(t_do, "do");
    astNode_insertChild(whileLoop, doBlock);
    if (lookahead() == '{')
    {
        consume(t_lCurly);
        astNode_insertChild(doBlock, parseStatementList(dict));
        consume(t_rCurly);
    }
    else
    {
        printf("Expected '{' after while(condition)!\n");
        exit(1);
    }


    // printf("done parsing if statement - here's what we got!\n");
    // printAST(ifStatement, 0);
    return whileLoop;
}
