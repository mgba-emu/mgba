/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/parser.h>

#include <mgba-util/string.h>

static struct LexVector* _lexOperator(struct LexVector* lv, char operator) {
	struct LexVector* lvNext = malloc(sizeof(struct LexVector));
	lvNext->token.type = TOKEN_OPERATOR_TYPE;
	switch (operator) {
	case '+':
		lvNext->token.operatorValue = OP_ADD;
		break;
	case '-':
		lvNext->token.operatorValue = OP_SUBTRACT;
		break;
	case '*':
		lvNext->token.operatorValue = OP_MULTIPLY;
		break;
	case '/':
		lvNext->token.operatorValue = OP_DIVIDE;
		break;
	default:
		lvNext->token.type = TOKEN_ERROR_TYPE;
		break;
	}
	lvNext->next = lv->next;
	lv->next = lvNext;
	lv = lvNext;
	lvNext = malloc(sizeof(struct LexVector));
	lvNext->next = lv->next;
	lvNext->token.type = TOKEN_ERROR_TYPE;
	lv->next = lvNext;
	return lvNext;
}

size_t lexExpression(struct LexVector* lv, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	uint32_t next = 0;
	size_t adjusted = 0;

	enum LexState state = LEX_ROOT;
	const char* tokenStart = 0;
	struct LexVector* lvNext;

	while (length > 0 && string[0] && string[0] != ' ' && state != LEX_ERROR) {
		char token = string[0];
		++string;
		++adjusted;
		--length;
		switch (state) {
		case LEX_ROOT:
			tokenStart = string - 1;
			switch (token) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				state = LEX_EXPECT_DECIMAL;
				next = token - '0';
				break;
			case '0':
				state = LEX_EXPECT_PREFIX;
				next = 0;
				break;
			case '$':
				state = LEX_EXPECT_HEX;
				next = 0;
				break;
			case '(':
				state = LEX_ROOT;
				lv->token.type = TOKEN_OPEN_PAREN_TYPE;
				lvNext = malloc(sizeof(struct LexVector));
				lvNext->next = lv->next;
				lvNext->token.type = TOKEN_ERROR_TYPE;
				lv->next = lvNext;
				lv = lvNext;
				break;
			default:
				if (tolower(token) >= 'a' && tolower(token <= 'z')) {
					state = LEX_EXPECT_IDENTIFIER;
				} else {
					state = LEX_ERROR;
				}
				break;
			};
			break;
		case LEX_EXPECT_IDENTIFIER:
			switch (token) {
			case '+':
			case '-':
			case '*':
			case '/':
				lv->token.type = TOKEN_IDENTIFIER_TYPE;
				lv->token.identifierValue = strndup(tokenStart, string - tokenStart - 1);
				lv = _lexOperator(lv, token);
				state = LEX_ROOT;
				break;
			case ')':
				lv->token.type = TOKEN_IDENTIFIER_TYPE;
				lv->token.identifierValue = strndup(tokenStart, string - tokenStart - 1);
				state = LEX_EXPECT_OPERATOR;
				break;
			default:
				break;
			}
			break;
		case LEX_EXPECT_BINARY:
			switch (token) {
			case '0':
			case '1':
				// TODO: handle overflow
				next <<= 1;
				next += token - '0';
				break;
			case '+':
			case '-':
			case '*':
			case '/':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				lv = _lexOperator(lv, token);
				state = LEX_ROOT;
				break;
			case ')':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				state = LEX_EXPECT_OPERATOR;
				break;
			default:
				state = LEX_ERROR;
				break;
			}
			break;
		case LEX_EXPECT_DECIMAL:
			switch (token) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				// TODO: handle overflow
				next *= 10;
				next += token - '0';
				break;
			case '+':
			case '-':
			case '*':
			case '/':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				lv = _lexOperator(lv, token);
				state = LEX_ROOT;
				break;
			case ')':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				state = LEX_EXPECT_OPERATOR;
				break;
			default:
				state = LEX_ERROR;
			}
			break;
		case LEX_EXPECT_HEX:
			switch (token) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				// TODO: handle overflow
				next *= 16;
				next += token - '0';
				break;
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
				// TODO: handle overflow
				next *= 16;
				next += token - 'A' + 10;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				// TODO: handle overflow
				next *= 16;
				next += token - 'a' + 10;
				break;
			case '+':
			case '-':
			case '*':
			case '/':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				lv = _lexOperator(lv, token);
				state = LEX_ROOT;
				break;
			case ':':
				lv->token.type = TOKEN_SEGMENT_TYPE;
				lv->token.uintValue = next;
				lvNext = malloc(sizeof(struct LexVector));
				lvNext->next = lv->next;
				lvNext->token.type = TOKEN_UINT_TYPE;
				lv->next = lvNext;
				lv = lvNext;
				next = 0;
				state = LEX_EXPECT_HEX;
				break;
			case ')':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				state = LEX_EXPECT_OPERATOR;
				break;
			default:
				state = LEX_ERROR;
				break;
			}
			break;
		case LEX_EXPECT_PREFIX:
			switch (token) {
			case 'X':
			case 'x':
				next = 0;
				state = LEX_EXPECT_HEX;
				break;
			case 'B':
			case 'b':
				next = 0;
				state = LEX_EXPECT_BINARY;
				break;
			case '+':
			case '-':
			case '*':
			case '/':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				lv = _lexOperator(lv, token);
				state = LEX_ROOT;
				break;
			case ')':
				lv->token.type = TOKEN_UINT_TYPE;
				lv->token.uintValue = next;
				state = LEX_EXPECT_OPERATOR;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				next = token - '0';
				state = LEX_EXPECT_DECIMAL;
				break;
			default:
				state = LEX_ERROR;
			}
			break;
		case LEX_EXPECT_OPERATOR:
			switch (token) {
			case '+':
			case '-':
			case '*':
			case '/':
				lvNext = malloc(sizeof(struct LexVector));
				lvNext->next = lv->next;
				lvNext->token.type = TOKEN_CLOSE_PAREN_TYPE;
				lv->next = lvNext;
				lv = _lexOperator(lv->next, token);
				state = LEX_ROOT;
				break;
			default:
				state = LEX_ERROR;
			}
			break;
		case LEX_ERROR:
			// This shouldn't be reached
			break;
		}
	}

	switch (state) {
	case LEX_EXPECT_BINARY:
	case LEX_EXPECT_DECIMAL:
	case LEX_EXPECT_HEX:
	case LEX_EXPECT_PREFIX:
		lv->token.type = TOKEN_UINT_TYPE;
		lv->token.uintValue = next;
		break;
	case LEX_EXPECT_IDENTIFIER:
		lv->token.type = TOKEN_IDENTIFIER_TYPE;
		lv->token.identifierValue = strndup(tokenStart, string - tokenStart);
		break;
	case LEX_EXPECT_OPERATOR:
		lvNext = malloc(sizeof(struct LexVector));
		lvNext->next = lv->next;
		lvNext->token.type = TOKEN_CLOSE_PAREN_TYPE;
		lv->next = lvNext;
		break;
	case LEX_ERROR:
	default:
		lv->token.type = TOKEN_ERROR_TYPE;
		break;
	}
	return adjusted;
}

static const int _operatorPrecedence[] = {
	2,
	1,
	1,
	0,
	0
};

static struct ParseTree* _parseTreeCreate() {
	struct ParseTree* tree = malloc(sizeof(struct ParseTree));
	tree->token.type = TOKEN_ERROR_TYPE;
	tree->rhs = 0;
	tree->lhs = 0;
	return tree;
}

static struct LexVector* _parseExpression(struct ParseTree* tree, struct LexVector* lv, int precedence, int openParens) {
	struct ParseTree* newTree = 0;
	while (lv) {
		int newPrecedence;
		switch (lv->token.type) {
		case TOKEN_IDENTIFIER_TYPE:
		case TOKEN_UINT_TYPE:
			if (tree->token.type == TOKEN_ERROR_TYPE) {
				tree->token = lv->token;
				lv = lv->next;
			} else {
				tree->token.type = TOKEN_ERROR_TYPE;
				return 0;
			}
			break;
		case TOKEN_SEGMENT_TYPE:
			tree->lhs = _parseTreeCreate();
			tree->lhs->token.type = TOKEN_UINT_TYPE;
			tree->lhs->token.uintValue = lv->token.uintValue;
			tree->rhs = _parseTreeCreate();
			tree->token.type = TOKEN_SEGMENT_TYPE;
			lv = _parseExpression(tree->rhs, lv->next, precedence, openParens);
			if (tree->token.type == TOKEN_ERROR_TYPE) {
				tree->token.type = TOKEN_ERROR_TYPE;
			}
			break;
		case TOKEN_OPEN_PAREN_TYPE:
			lv = _parseExpression(tree, lv->next, INT_MAX, openParens + 1);
			break;
		case TOKEN_CLOSE_PAREN_TYPE:
			if (openParens <= 0) {
				tree->token.type = TOKEN_ERROR_TYPE;
				return 0;
			}
			return lv->next;
			break;
		case TOKEN_OPERATOR_TYPE:
			newPrecedence = _operatorPrecedence[lv->token.operatorValue];
			if (newPrecedence < precedence) {
				newTree = _parseTreeCreate();
				*newTree = *tree;
				tree->lhs = newTree;
				tree->rhs = _parseTreeCreate();
				tree->token = lv->token;
				lv = _parseExpression(tree->rhs, lv->next, newPrecedence, openParens);
				if (tree->token.type == TOKEN_ERROR_TYPE) {
					tree->token.type = TOKEN_ERROR_TYPE;
				}
			} else {
				return lv;
			}
			break;
		case TOKEN_ERROR_TYPE:
			tree->token.type = TOKEN_ERROR_TYPE;
			return 0;
		}
	}

	return 0;
}

void parseLexedExpression(struct ParseTree* tree, struct LexVector* lv) {
	if (!tree) {
		return;
	}

	tree->token.type = TOKEN_ERROR_TYPE;
	tree->lhs = 0;
	tree->rhs = 0;

	_parseExpression(tree, lv, _operatorPrecedence[OP_ASSIGN], 0);
}

void lexFree(struct LexVector* lv) {
	while (lv) {
		struct LexVector* lvNext = lv->next;
		free(lv);
		lv = lvNext;
	}
}

void parseFree(struct ParseTree* tree) {
	if (!tree) {
		return;
	}

	parseFree(tree->lhs);
	parseFree(tree->rhs);

	if (tree->token.type == TOKEN_IDENTIFIER_TYPE) {
		free(tree->token.identifierValue);
	}
	free(tree);
}
