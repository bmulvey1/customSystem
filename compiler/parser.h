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

struct AST *match(enum token t, struct Dictionary *dict);

void consume(enum token t);

char *getTokenName(enum token t);

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict);

struct AST *parseTLDList(struct Dictionary *dict);

struct AST *parseTLD(struct Dictionary *dict);

struct AST *parseAssignment(struct AST *name, struct Dictionary *dict);

struct AST *parseScope(struct Dictionary *dict);

struct AST *parseName(struct Dictionary *dict);

struct AST *parseDeclaration(struct Dictionary *dict);

struct AST *parseStatement(struct Dictionary *dict);

struct AST *parseExpression(struct Dictionary *dict);

struct AST *parseArgDefinitions(struct Dictionary *dict);

struct AST *parseArgList(struct Dictionary *dict);

struct AST *parseFunctionCall(struct AST *name, struct Dictionary *dict);

struct AST *parseIfStatement(struct Dictionary *dict);

struct AST *parseElseStatement(struct Dictionary *dict);

struct AST* parseWhileLoop(struct Dictionary *dict);

struct AST* parseASM(struct Dictionary *dict);
