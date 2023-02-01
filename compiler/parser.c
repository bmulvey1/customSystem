#include "parser.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
int curLine, curCol;
char inChar;
char *token_names[] = {
	"asm",
	"var",
	"fun",
	"return",
	"if",
	"else",
	"while",
	"do",
	"name",
	"literal",
	"unary operator",
	"binary operator",
	"comparison operator",
	"reference operator",
	"dereference operator",
	"assignment",
	"comma",
	"semicolon",
	"l paren",
	"r paren",
	"l curly",
	"r curly",
	"l bracket",
	"r bracket",
	"call",
	"scope",
	"EOF"};

void ParserError(char *production, char *info)
{
	printf("Error while parsing %s\n", production);
	printf("Error at line %d, col %d\n", curLine, curCol);
	printf("%s\n", info);
	ErrorAndExit(ERROR_CODE, "Parse buffer when error occurred: [%s]\n", buffer);
}

// return the char 'count' characters ahead
// count must be >0, returns null char otherwise
char lookahead_dumb(int count)
{
	long offset = ftell(inFile);
	char returnChar = '\0';
	for (int i = 0; i < count; i++)
	{
		returnChar = fgetc(inFile);
	}
	fseek(inFile, offset, SEEK_SET);
	return returnChar;
}

void trimWhitespace(char trackPos)
{
	int trimming = 1;
	while (trimming)
	{
		switch (lookahead_dumb(1))
		{
		case '\n':
			if (trackPos)
			{
				curLine++;
				curCol = 1;
			}
			fgetc(inFile);
			break;

		case -1:
		case '\0':
		case ' ':
		case '\t':
			if (trackPos)
				curCol++;

			fgetc(inFile);
			break;

		case '/':
			switch (lookahead_dumb(2))
			{
				// single line comment
			case '/':
				while (fgetc(inFile) != '\n')
					;

				if (trackPos)
				{
					curLine++;
					curCol = 1;
				}
				break;

			case '*':
				// skip the comment opener, begin reading the comment
				fgetc(inFile);
				fgetc(inFile);

				char inBlockComment = 1;
				while (inBlockComment)
				{
					switch (fgetc(inFile))
					{
						// look ahead if we see something that could be related to a block comment
					case '*':
						if (lookahead_dumb(1) == '/')
							inBlockComment = 0;

						break;

						// disallow nesting of block comments
					case '/':
						if (lookahead_dumb(1) == '*')
							ParserError("comment", "Error - nested block comments not allowed");

						break;

						// otherwise just track position in the file if necessary
					case '\n':
						if (trackPos)
						{
							curLine++;
							curCol = 1;
							break;
						}
						break;

					default:
						if (trackPos)
							curCol += 1;

						break;
					}
				}
				// catch the trailing slash of the comment closer
				fgetc(inFile);
				break;
			}

			break;

		default:
			trimming = 0;
			break;
		}
	}
}

char lookahead()
{
	trimWhitespace(1);
	char r = lookahead_dumb(1);
	return r;
}

#define RESERVED_COUNT 26

char *reserved[RESERVED_COUNT] = {
	"asm",
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
	"[",
	"]",
	";",
	"=",
	"+",
	"-",
	"*",
	"&",
	">",
	"<",
	">=",
	"<=",
	"==",
	"$$"};

enum token reserved_t[RESERVED_COUNT] = {
	t_asm,
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
	t_lBracket,
	t_rBracket,
	t_semicolon,
	t_assign,
	t_un_add,
	t_un_sub,
	t_dereference,
	t_reference,
	t_compOp,
	t_compOp,
	t_compOp,
	t_compOp,
	t_compOp,
	t_EOF};

enum token scan(char trackPos)
{
	buflen = 0;
	// check if we're looking at whitespace - are we?
	trimWhitespace(trackPos);
	if (feof(inFile))
		return t_EOF;

	enum token currentToken = -1;

	while (1)
	{
		inChar = fgetc(inFile);
		if (trackPos)
			curCol++;

		buffer[buflen++] = inChar;
		buffer[buflen] = '\0';
		if (feof(inFile))
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
						return reserved_t[i];
				}
				else
					return reserved_t[i]; // return its token
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
				ParserError("literal", "Error - alphabetical character in literal!");
		}

		// if the next input char is whitespace or a single-character token, we're done with this token
		switch (lookahead_dumb(1))
		{
		case ' ':
		case ',':
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
		case ';':
		case '\n':
		case '\t':
		case '+':
		case '-':
		case '*':
		case '&':
		case '/':
			return currentToken;
			break;
		}
	}
}

// return the next token that would be scanned without consuming
enum token lookaheadToken()
{
	long offset = ftell(inFile);
	enum token retToken = scan(0);
	fseek(inFile, offset, SEEK_SET);
	return retToken;
}

// error-checked method to consume and return AST node of expected token
struct ASTNode *match(enum token t, struct Dictionary *dict)
{
	int line = curLine;
	int col = curCol;
	enum token result = scan(1);
	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ParserError("match", "Error matching a token!");
	}

	struct ASTNode *matched = ASTNode_new(result, DictionaryLookupOrInsert(dict, buffer));
	matched->sourceLine = line;
	matched->sourceCol = col;
	return matched;
}

// error-checked method to consume expected token with no return
void consume(enum token t)
{
	enum token result = scan(1);
	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ParserError("consume", "Error consuming a token!");
	}
}

char *getTokenName(enum token t)
{
	return token_names[t];
}

struct ASTNode *parseProgram(char *inFileName, struct Dictionary *dict)
{
	curLine = 1;
	curCol = 1;
	inFile = fopen(inFileName, "rb");
	struct ASTNode *program = parseTLDList(dict);
	fclose(inFile);
	return program;
}

struct ASTNode *parseTLDList(struct Dictionary *dict)
{
	struct ASTNode *TLDList = parseTLD(dict);
	while (1)
	{
		if (lookaheadToken() == t_EOF)
			break;

		ASTNode_insertSibling(TLDList, parseTLD(dict));
	}
	// printf("done parsing tld list\n");
	return TLDList;
}

struct ASTNode *parseTLD(struct Dictionary *dict)
{
	struct ASTNode *TLD;
	switch (lookaheadToken())
	{
	case t_asm:
		TLD = parseASM(dict);
		break;

	// fun [function name]({[argument list]})
	case t_fun:
		TLD = match(t_fun, dict);
		struct ASTNode *functionname = match(t_name, dict);
		consume(t_lParen);
		struct ASTNode *argList = parseArgDefinitions(dict);
		ASTNode_insertChild(functionname, argList);
		ASTNode_insertChild(TLD, functionname);
		consume(t_rParen);

		ASTNode_insertChild(TLD, parseScope(dict));
		break;

	// var [variable name];
	// var [variable name] = [expression];
	case t_var:
	{
		TLD = match(t_var, dict);
		struct ASTNode *name = parseDeclaration(dict);
		if (lookahead() == '=')
		{
			struct ASTNode *assignment = match(t_assign, dict);
			ASTNode_insertChild(assignment, name);
			ASTNode_insertChild(assignment, parseExpression(dict));
			ASTNode_insertChild(TLD, assignment);
		}
		else
		{
			ASTNode_insertChild(TLD, name);
		}

		consume(t_semicolon);
	}
	break;
	default:
		ParserError("Top level declaration", "Expected function or variable");
		exit(1);
	}
	return TLD;
}

struct ASTNode *parseAssignment(struct ASTNode *name, struct Dictionary *dict)
{
	// pre-parsed name node taken as argument
	// [name] = [expression]
	struct ASTNode *assign = match(t_assign, dict);
	ASTNode_insertChild(assign, name);
	ASTNode_insertChild(assign, parseExpression(dict));
	return assign;
}

struct ASTNode *parseScope(struct Dictionary *dict)
{
	struct ASTNode *scope = ASTNode_new(t_scope, "scope");
	consume(t_lCurly);
	// parse statements until we see the end of this scope
	while (lookahead() != '}')
	{
		if (lookahead() == '{')
		{
			ASTNode_insertChild(scope, parseScope(dict));
		}
		else
		{
			ASTNode_insertChild(scope, parseStatement(dict));
		}
	}
	consume(t_rCurly);

	// printf("done parsing statement list");
	return scope;
}

struct ASTNode *parseName(struct Dictionary *dict)
{
	struct ASTNode *name = match(t_name, dict);
	if (lookahead() == '[')
	{
		struct ASTNode *bracketOp = ASTNode_new(t_un_add, "+");
		ASTNode_insertChild(bracketOp, name);
		consume(t_lBracket);
		switch (lookaheadToken())
		{
		case t_name:
		case t_literal:
			ASTNode_insertChild(bracketOp, parseExpression(dict));
			break;

		default:
			ParserError("statement", "Expected name or literal in square brackets after identifier");
		}


		consume(t_rBracket);

		name = ASTNode_new(t_dereference, "*");
		ASTNode_insertChild(name, bracketOp);
		// name = newName;
	}
	return name;
}

struct ASTNode *parseDeclaration(struct Dictionary *dict)
{
	struct ASTNode *declared = NULL;
	struct ASTNode *declaredRunner = NULL;
	enum token upcomingToken = lookaheadToken();
	while (upcomingToken == t_dereference)
	{
		if (declaredRunner == NULL)
		{
			declared = match(t_dereference, dict);
			declaredRunner = declared;
		}
		else
		{
			ASTNode_insertChild(declaredRunner, match(t_dereference, dict));
			declaredRunner = declaredRunner->child;
		}
		upcomingToken = lookaheadToken();
	}

	struct ASTNode *identifier = match(t_name, dict);

	if (lookahead() == '[')
	{
		consume(t_lBracket);
		struct ASTNode *arrayDecl = ASTNode_new(t_array, "[]");
		ASTNode_insertChild(arrayDecl, identifier);
		ASTNode_insertChild(arrayDecl, match(t_literal, dict));
		if(declaredRunner != NULL)
		{
			ASTNode_insertChild(declaredRunner, arrayDecl);

		}
		else
		{
			declared = arrayDecl;
		}
		consume(t_rBracket);
	}
	else
	{
		if(declaredRunner != NULL)
		{
			ASTNode_insertChild(declaredRunner, identifier);
		}
		else
		{
			declared = identifier;
		}
	}
	return declared;
}

struct ASTNode *parseStatement(struct Dictionary *dict)
{
	struct ASTNode *statement = NULL;
	enum token upcomingToken = lookaheadToken();
	switch (upcomingToken)
	{

	case t_asm:
		statement = parseASM(dict);
		break;

	// v [variable name];
	// v [variable name] = [expression];
	case t_var:
	{
		struct ASTNode *type = match(upcomingToken, dict);
		statement = type;
		// 'var' will obviously be a declaration

		// declared type (including any '*'s)
		struct ASTNode *declaredType = parseDeclaration(dict);
		
		/*
		if (lookahead() == '[')
		{
		}
		*/

		ASTNode_insertChild(type, declaredType);

		if (lookaheadToken() == t_assign)
		{
			struct ASTNode *assignedName = malloc(sizeof(struct ASTNode));
			struct ASTNode *declaredName = declaredType;
			while(declaredName->type == t_dereference)
			{
				declaredName = declaredName->child;
			}
			printf("Declared name is %s\n", declaredType->value);
			memcpy(assignedName, declaredName, sizeof(struct ASTNode));
			ASTNode_insertSibling(statement, parseAssignment(assignedName, dict));
		}

		// ASTNode_insertChild(statement, type);

		consume(t_semicolon);
	}
	break;

	// [variable name] = [expression];
	case t_name:
	{
		struct ASTNode *name = parseName(dict);

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
			ParserError("statement", "expected '(' or '=' after name");
		}
		consume(t_semicolon);
	}
	break;

	case t_dereference:
		struct ASTNode *deref = match(t_dereference, dict);
		switch (lookahead())
		{
		case '*':
		case '(':
			ASTNode_insertChild(deref, parseExpression(dict));
			break;

		default:
			ASTNode_insertChild(deref, parseName(dict));
			break;
		}
		statement = parseAssignment(deref, dict);
		consume(t_semicolon);

		break;

	case t_return:
	{
		statement = match(t_return, dict);
		ASTNode_insertChild(statement, parseExpression(dict));
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
		ParserError("statement", "expected 'var', 'if', 'while', '}', or name");
	}
	// printf("Done parsing statement- heres what we got\n");
	// printAST(statement, 0);
	return statement;
}

struct ASTNode *parseExpression(struct Dictionary *dict)
{
	// printf("parsing expression\n");
	struct ASTNode *expression = NULL;

	// figure out what the left side of the expression is
	struct ASTNode *lSide = NULL;
	if (isalpha(lookahead()))
	{
		struct ASTNode *name = parseName(dict);
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
	else if (lookahead() == '*' || lookahead() == '&')
	{
		lSide = match(lookaheadToken(), dict);

		/* need to explicitly handle parens so that reference/dereference operators bind properly
		 * if handled by simply letting the recursive call to parseExpression() deal with the parens
		 * the reference/dereference will bind as a parent to the entire subtree generated
		 */
		switch (lookahead())
		{
		case '(':
		case '*':
			consume(t_lParen);
			ASTNode_insertChild(lSide, parseExpression(dict));
			consume(t_rParen);
			break;

		default:
			ASTNode_insertChild(lSide, parseName(dict));
		}
	}
	else
	{
		ParserError("expression", "expected literal or name");
	}

	// now, figure out whether there is a right side
	switch (lookahead())
	{
	// [left side][operator][right side]
	case '+':
		expression = match(t_un_add, dict);
		ASTNode_insertChild(expression, lSide);
		ASTNode_insertChild(expression, parseExpression(dict));
		break;

	case '-':
		expression = match(t_un_sub, dict);
		ASTNode_insertChild(expression, lSide);
		ASTNode_insertChild(expression, parseExpression(dict));
		break;

	case '=':
		// only continue to match comparison operator if we have '=='
		if (lookaheadToken() == t_assign)
		{
			expression = lSide;
			break;
		}
	case '<':
	case '>':
		expression = match(t_compOp, dict);
		ASTNode_insertChild(expression, lSide);
		ASTNode_insertChild(expression, parseExpression(dict));
		break;

	// end of line or end of expression, there isn't anything more than the left side
	case ';':
	case ',':
	case ')':
	case ']':
		expression = lSide;
		break;

	default:
		printf("\n[%c]\n", lookahead());
		ParserError("expression", "expected unary operator or terminator");
	}

	// printf("done parsing expression - here's what we got:\n");
	// printAST(expression, 0);
	return expression;
}

struct ASTNode *parseArgDefinitions(struct Dictionary *dict)
{
	struct ASTNode *argList = NULL;
	int parsing = 1;
	while (parsing)
	{
		enum token next = lookaheadToken();
		switch (next)
		{
		case t_var:
		{
			struct ASTNode *argument = match(next, dict);
			struct ASTNode *declaration = argument;
			while (lookaheadToken() == t_dereference)
			{
				ASTNode_insertChild(argument, match(t_dereference, dict));
				declaration = declaration->child;
			}
			ASTNode_insertChild(declaration, match(t_name, dict));

			if (argList == NULL)
				argList = argument;
			else
				ASTNode_insertSibling(argList, argument);
		}
		break;

		case t_comma:
			consume(next);
			break;

		default:
			parsing = 0;
			break;
		}
	}
	return argList;
}

struct ASTNode *parseArgList(struct Dictionary *dict)
{
	struct ASTNode *argList = NULL;
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
				ASTNode_insertSibling(argList, match(t_literal, dict));
			break;*/

		default:
			if (argList == NULL)
				argList = parseExpression(dict);
			else
				ASTNode_insertSibling(argList, parseExpression(dict));
			break;
		}
	}
	return argList;
}

struct ASTNode *parseFunctionCall(struct ASTNode *name, struct Dictionary *dict)
{
	consume(t_lParen);
	struct ASTNode *callNode = ASTNode_new(t_call, "call");
	ASTNode_insertChild(callNode, name);
	ASTNode_insertChild(name, parseArgList(dict));
	consume(t_rParen);
	return callNode;
}

struct ASTNode *parseIfStatement(struct Dictionary *dict)
{
	struct ASTNode *ifStatement = match(t_if, dict);
	consume(t_lParen);
	ASTNode_insertChild(ifStatement, parseExpression(dict));
	consume(t_rParen);
	struct ASTNode *doBlock = ASTNode_new(t_do, "do");
	ASTNode_insertChild(ifStatement, doBlock);

	if (lookahead() == '{')
	{
		ASTNode_insertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct ASTNode *ifScope = ASTNode_new(t_scope, "scope");
		ASTNode_insertChild(ifScope, parseStatement(dict));
		ASTNode_insertChild(doBlock, ifScope);
	}

	if (lookaheadToken() == t_else)
	{
		ASTNode_insertChild(ifStatement, parseElseStatement(dict));
	}

	// printf("done parsing if statement - here's what we got!\n");
	// printAST(ifStatement, 0);
	return ifStatement;
}

struct ASTNode *parseElseStatement(struct Dictionary *dict)
{
	struct ASTNode *elseStatement = match(t_else, dict);
	struct ASTNode *doBlock = ASTNode_new(t_do, "do");
	ASTNode_insertChild(elseStatement, doBlock);
	if (lookahead() == '{')
	{
		ASTNode_insertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct ASTNode *elseScope = ASTNode_new(t_scope, "scope");
		ASTNode_insertChild(elseScope, parseStatement(dict));
		ASTNode_insertChild(doBlock, elseScope);
	}

	return elseStatement;
}

struct ASTNode *parseWhileLoop(struct Dictionary *dict)
{
	struct ASTNode *whileLoop = match(t_while, dict);
	consume(t_lParen);
	ASTNode_insertChild(whileLoop, parseExpression(dict));
	consume(t_rParen);
	struct ASTNode *doBlock = ASTNode_new(t_do, "do");
	ASTNode_insertChild(whileLoop, doBlock);
	if (lookahead() == '{')
	{
		ASTNode_insertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct ASTNode *whileScope = ASTNode_new(t_scope, "scope");
		ASTNode_insertChild(whileScope, parseStatement(dict));
		ASTNode_insertChild(doBlock, whileScope);
	}
	// ParserError("while loop", "Expected '{' after 'while([condition])'");

	// printf("done parsing if statement - here's what we got!\n");
	// printAST(ifStatement, 0);
	return whileLoop;
}

struct ASTNode *parseASM(struct Dictionary *dict)
{
	struct ASTNode *asmNode = match(t_asm, dict);
	consume(t_lCurly);
	trimWhitespace(1);
	char inASMblock = 1;
	int lineLen = 0;
	char asmLine[32];
	while (inASMblock)
	{
		switch (lookahead_dumb(1))
		{
		case '}':
			if (lineLen > 0)
			{
				asmLine[lineLen] = '\0';
				ASTNode_insertChild(asmNode, ASTNode_new(t_asm, DictionaryLookupOrInsert(dict, asmLine)));
			}
			curCol++;
			inASMblock = 0;
			break;

		case '\n':
			asmLine[lineLen] = '\0';
			curCol = 0;
			ASTNode_insertChild(asmNode, ASTNode_new(t_asm, DictionaryLookupOrInsert(dict, asmLine)));
			trimWhitespace(1);
			lineLen = 0;
			break;

		default:
			asmLine[lineLen++] = fgetc(inFile);
			curCol++;
		}
	}
	consume(t_rCurly);
	consume(t_semicolon);
	return asmNode;
}