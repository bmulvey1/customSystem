#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"
#include "util.h"

#define BUF_SIZE 32

void ParserError();

char lookahead_dumb();

void trimWhitespace(char trackPos);

char lookahead();

enum token scan(char trackPos);

struct ASTNode *match(enum token t, struct Dictionary *dict);

void consume(enum token t);

char *getTokenName(enum token t);

struct ASTNode *parseProgram(char *inFileName, struct Dictionary *dict);

struct ASTNode *parseTLDList(struct Dictionary *dict);

struct ASTNode *parseTLD(struct Dictionary *dict);

struct ASTNode *parseAssignment(struct ASTNode *name, struct Dictionary *dict);

struct ASTNode *parseStatementList(struct Dictionary *dict);

struct ASTNode *parseStatement(struct Dictionary *dict);

struct ASTNode *parseExpression(struct Dictionary *dict);

struct ASTNode *parseArgDefinitions(struct Dictionary *dict);

struct ASTNode *parseArgList(struct Dictionary *dict);

struct ASTNode *parseFunctionCall(struct ASTNode *name, struct Dictionary *dict);

struct ASTNode *parseIfStatement(struct Dictionary *dict);

struct ASTNode *parseElseStatement(struct Dictionary *dict);

struct ASTNode* parseWhileLoop(struct Dictionary *dict);

struct ASTNode* parseASM(struct Dictionary *dict);