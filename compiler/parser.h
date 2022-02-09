#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"
#include "dict.h"

char lookahead_dumb();

void trimWhitespace();

char lookahead();

enum token scan();

struct astNode *match(enum token t, struct Dictionary *dict);

void consume(enum token t);

char *getTokenName(enum token t);

struct astNode *parseProgram(char *inFileName, struct Dictionary *dict);

struct astNode *parseTLDList(struct Dictionary *dict);

struct astNode *parseTLD(struct Dictionary *dict);

struct astNode *parseAssignment(struct astNode *name, struct Dictionary *dict);

struct astNode *parseStatementList(struct Dictionary *dict);

struct astNode *parseStatement(struct Dictionary *dict);

struct astNode *parseExpression(struct Dictionary *dict);

struct astNode *parseArgDefinitions(struct Dictionary *dict);

struct astNode *parseArgList(struct Dictionary *dict);

struct astNode *parseFunctionCall(struct astNode *name, struct Dictionary *dict);

struct astNode *parseIfStatement(struct Dictionary *dict);

struct astNode *parseElseStatement(struct Dictionary *dict);

struct astNode* parseWhileLoop(struct Dictionary *dict);