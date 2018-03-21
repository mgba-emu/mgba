/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/parser.h>

#include <mgba/debugger/debugger.h>
#include <mgba-util/string.h>

DEFINE_VECTOR(LexVector, struct Token);

enum LexState {
	LEX_ERROR = -1,
	LEX_ROOT = 0,
	LEX_EXPECT_IDENTIFIER,
	LEX_EXPECT_BINARY_FIRST,
	LEX_EXPECT_BINARY,
	LEX_EXPECT_DECIMAL,
	LEX_EXPECT_HEX_FIRST,
	LEX_EXPECT_HEX,
	LEX_EXPECT_PREFIX,
	LEX_EXPECT_OPERATOR,
	LEX_EXPECT_OPERATOR2,
};

static void _lexOperator(struct LexVector* lv, char operator, enum LexState* state) {
	if (*state == LEX_EXPECT_OPERATOR2) {
		struct Token* lvNext = LexVectorGetPointer(lv, LexVectorSize(lv) - 1);
		if (lvNext->type != TOKEN_OPERATOR_TYPE) {
			lvNext->type = TOKEN_ERROR_TYPE;
			*state = LEX_ERROR;
			return;
		}
		switch (lvNext->operatorValue) {
		case OP_AND:
			if (operator == '&') {
				lvNext->operatorValue = OP_LOGICAL_AND;
				*state = LEX_ROOT;
				return;
			}
			break;
		case OP_OR:
			if (operator == '|') {
				lvNext->operatorValue = OP_LOGICAL_OR;
				*state = LEX_ROOT;
				return;
			}
			break;
		case OP_LESS:
			if (operator == '=') {
				lvNext->operatorValue = OP_LE;
				*state = LEX_ROOT;
				return;
			}
			if (operator == '<') {
				lvNext->operatorValue = OP_SHIFT_L;
				*state = LEX_ROOT;
				return;
			}
			break;
		case OP_GREATER:
			if (operator == '=') {
				lvNext->operatorValue = OP_GE;
				*state = LEX_ROOT;
				return;
			}
			if (operator == '>') {
				lvNext->operatorValue = OP_SHIFT_R;
				*state = LEX_ROOT;
				return;
			}
			break;
		case OP_ASSIGN:
			if (operator == '=') {
				lvNext->operatorValue = OP_EQUAL;
				*state = LEX_ROOT;
				return;
			}
			break;
		case OP_NOT:
			if (operator == '=') {
				lvNext->operatorValue = OP_NOT_EQUAL;
				*state = LEX_ROOT;
				return;
			}
			break;
		default:
			break;
		}
		*state = LEX_ERROR;
		return;
	}
	struct Token* lvNext = LexVectorAppend(lv);
	lvNext->type = TOKEN_OPERATOR_TYPE;
	*state = LEX_EXPECT_OPERATOR2;
	switch (operator) {
	case '=':
		lvNext->operatorValue = OP_ASSIGN;
		break;
	case '+':
		lvNext->operatorValue = OP_ADD;
		break;
	case '-':
		lvNext->operatorValue = OP_SUBTRACT;
		break;
	case '*':
		lvNext->operatorValue = OP_MULTIPLY;
		break;
	case '/':
		lvNext->operatorValue = OP_DIVIDE;
		break;
	case '%':
		lvNext->operatorValue = OP_MODULO;
		break;
	case '&':
		lvNext->operatorValue = OP_AND;
		break;
	case '|':
		lvNext->operatorValue = OP_OR;
		break;
	case '^':
		lvNext->operatorValue = OP_XOR;
		break;
	case '<':
		lvNext->operatorValue = OP_LESS;
		break;
	case '>':
		lvNext->operatorValue = OP_GREATER;
		break;
	case '!':
		lvNext->operatorValue = OP_NOT;
		break;
	default:
		lvNext->type = TOKEN_ERROR_TYPE;
		break;
	}
}

static void _lexValue(struct LexVector* lv, char token, uint32_t next, enum LexState* state) {
	struct Token* lvNext;

	switch (token) {
	case '=':
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case '<':
	case '>':
	case '!':
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_UINT_TYPE;
		lvNext->uintValue = next;
		_lexOperator(lv, token, state);
		break;
	case ')':
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_UINT_TYPE;
		lvNext->uintValue = next;
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_CLOSE_PAREN_TYPE;
		*state = LEX_EXPECT_OPERATOR;
		break;
	case ' ':
	case '\t':
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_UINT_TYPE;
		lvNext->uintValue = next;
		*state = LEX_EXPECT_OPERATOR;
		break;
	default:
		*state = LEX_ERROR;
		break;
	}
}

size_t lexExpression(struct LexVector* lv, const char* string, size_t length, const char* eol) {
	if (!string || length < 1) {
		return 0;
	}

	uint32_t next = 0;
	size_t adjusted = 0;

	enum LexState state = LEX_ROOT;
	const char* tokenStart = 0;
	struct Token* lvNext;

	if (!eol) {
		eol = " \r\n";
	}

	while (length > 0 && string[0] && !strchr(eol, string[0]) && state != LEX_ERROR) {
		char token = string[0];
		++string;
		++adjusted;
		--length;
		switch (state) {
		case LEX_EXPECT_OPERATOR2:
			switch (token) {
			case '&':
			case '|':
			case '=':
			case '<':
			case '>':
				_lexOperator(lv, token, &state);
				break;
			}
			if (state != LEX_EXPECT_OPERATOR2) {
				break;
			}
			// Fall through
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
				state = LEX_EXPECT_HEX_FIRST;
				next = 0;
				break;
			case '%':
				state = LEX_EXPECT_BINARY_FIRST;
				next = 0;
				break;
			case '(':
				state = LEX_ROOT;
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_OPEN_PAREN_TYPE;
				break;
			case ' ':
			case '\t':
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
			case '=':
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '&':
			case '|':
			case '^':
			case '<':
			case '>':
			case '!':
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_IDENTIFIER_TYPE;
				lvNext->identifierValue = strndup(tokenStart, string - tokenStart - 1);
				_lexOperator(lv, token, &state);
				break;
			case ')':
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_IDENTIFIER_TYPE;
				lvNext->identifierValue = strndup(tokenStart, string - tokenStart - 1);
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_CLOSE_PAREN_TYPE;
				state = LEX_EXPECT_OPERATOR;
				break;
			case ' ':
			case '\t':
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_IDENTIFIER_TYPE;
				lvNext->identifierValue = strndup(tokenStart, string - tokenStart - 1);
				state = LEX_EXPECT_OPERATOR;
				break;
			default:
				break;
			}
			break;
		case LEX_EXPECT_BINARY_FIRST:
			state = LEX_EXPECT_BINARY;
			// Fall through
		case LEX_EXPECT_BINARY:
			switch (token) {
			case '0':
			case '1':
				// TODO: handle overflow
				next <<= 1;
				next += token - '0';
				break;
			default:
				_lexValue(lv, token, next, &state);
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
			default:
				_lexValue(lv, token, next, &state);
				break;
			}
			break;
		case LEX_EXPECT_HEX_FIRST:
			state = LEX_EXPECT_HEX;
			// Fall through
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
			case ':':
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_SEGMENT_TYPE;
				lvNext->uintValue = next;
				next = 0;
				break;
			default:
				_lexValue(lv, token, next, &state);
				break;
			}
			break;
		case LEX_EXPECT_PREFIX:
			switch (token) {
			case 'X':
			case 'x':
				next = 0;
				state = LEX_EXPECT_HEX_FIRST;
				break;
			case 'B':
			case 'b':
				next = 0;
				state = LEX_EXPECT_BINARY_FIRST;
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
				_lexValue(lv, token, next, &state);
				break;
			}
			break;
		case LEX_EXPECT_OPERATOR:
			switch (token) {
			case '=':
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '&':
			case '|':
			case '^':
			case '<':
			case '>':
			case '!':
				_lexOperator(lv, token, &state);
				break;
			case ')':
				lvNext = LexVectorAppend(lv);
				lvNext->type = TOKEN_CLOSE_PAREN_TYPE;
				break;
			case ' ':
			case '\t':
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
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_UINT_TYPE;
		lvNext->uintValue = next;
		break;
	case LEX_EXPECT_IDENTIFIER:
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_IDENTIFIER_TYPE;
		lvNext->identifierValue = strndup(tokenStart, string - tokenStart);
		break;
	case LEX_ROOT:
	case LEX_EXPECT_OPERATOR:
	case LEX_EXPECT_OPERATOR2:
		break;
	case LEX_EXPECT_BINARY_FIRST:
	case LEX_EXPECT_HEX_FIRST:
	case LEX_ERROR:
	default:
		lvNext = LexVectorAppend(lv);
		lvNext->type = TOKEN_ERROR_TYPE;
		break;
	}
	return adjusted;
}

static const int _operatorPrecedence[] = {
	[OP_ASSIGN] = 14,
	[OP_ADD] = 4,
	[OP_SUBTRACT] = 4,
	[OP_MULTIPLY] = 3,
	[OP_DIVIDE] = 3,
	[OP_MODULO] = 3,
	[OP_AND] = 8,
	[OP_OR] = 10,
	[OP_XOR] = 9,
	[OP_LESS] = 6,
	[OP_GREATER] = 6,
	[OP_EQUAL] = 7,
	[OP_NOT_EQUAL] = 7,
	[OP_LE] = 6,
	[OP_GE] = 6,
	[OP_LOGICAL_AND] = 11,
	[OP_LOGICAL_OR] = 12,
	[OP_NEGATE] = 2,
	[OP_FLIP] = 2,
	[OP_NOT] = 2,
	[OP_SHIFT_L] = 5,
	[OP_SHIFT_R] = 5,
};

static struct ParseTree* _parseTreeCreate() {
	struct ParseTree* tree = malloc(sizeof(struct ParseTree));
	tree->token.type = TOKEN_ERROR_TYPE;
	tree->rhs = 0;
	tree->lhs = 0;
	return tree;
}

static size_t _parseExpression(struct ParseTree* tree, struct LexVector* lv, size_t i, int precedence, int* openParens) {
	struct ParseTree* newTree = 0;
	while (i < LexVectorSize(lv)) {
		struct Token* token = LexVectorGetPointer(lv, i);
		int newPrecedence;
		switch (token->type) {
		case TOKEN_IDENTIFIER_TYPE:
		case TOKEN_UINT_TYPE:
			if (tree->token.type == TOKEN_ERROR_TYPE) {
				tree->token = *token;
				if (token->type == TOKEN_IDENTIFIER_TYPE) {
					tree->token.identifierValue = strdup(token->identifierValue);
				}
				++i;
			} else {
				tree->token.type = TOKEN_ERROR_TYPE;
				return i + 1;
			}
			break;
		case TOKEN_SEGMENT_TYPE:
			tree->lhs = _parseTreeCreate();
			tree->lhs->token.type = TOKEN_UINT_TYPE;
			tree->lhs->token.uintValue = token->uintValue;
			tree->rhs = _parseTreeCreate();
			tree->token.type = TOKEN_SEGMENT_TYPE;
			i = _parseExpression(tree->rhs, lv, i + 1, precedence, openParens);
			if (tree->token.type == TOKEN_ERROR_TYPE) {
				tree->token.type = TOKEN_ERROR_TYPE;
			}
			break;
		case TOKEN_OPEN_PAREN_TYPE:
			++*openParens;
			i = _parseExpression(tree, lv, i + 1, INT_MAX, openParens);
			break;
		case TOKEN_CLOSE_PAREN_TYPE:
			if (*openParens <= 0) {
				tree->token.type = TOKEN_ERROR_TYPE;
			}
			--*openParens;
			return i + 1;
		case TOKEN_OPERATOR_TYPE:
			newPrecedence = _operatorPrecedence[token->operatorValue];
			if (newPrecedence < precedence) {
				newTree = _parseTreeCreate();
				*newTree = *tree;
				tree->lhs = newTree;
				tree->rhs = _parseTreeCreate();
				tree->token = *token;
				i = _parseExpression(tree->rhs, lv, i + 1, newPrecedence, openParens);
				if (tree->token.type == TOKEN_ERROR_TYPE) {
					tree->token.type = TOKEN_ERROR_TYPE;
				}
			} else {
				return i;
			}
			break;
		case TOKEN_ERROR_TYPE:
			tree->token.type = TOKEN_ERROR_TYPE;
			return i + 1;
		}
	}

	return i;
}

void parseLexedExpression(struct ParseTree* tree, struct LexVector* lv) {
	if (!tree) {
		return;
	}

	tree->token.type = TOKEN_ERROR_TYPE;
	tree->lhs = 0;
	tree->rhs = 0;

	int openParens = 0;
	_parseExpression(tree, lv, 0, INT_MAX, &openParens);
	if (openParens) {
		if (tree->token.type == TOKEN_IDENTIFIER_TYPE) {
			free(tree->token.identifierValue);
		}
		tree->token.type = TOKEN_ERROR_TYPE;
	}
}

void lexFree(struct LexVector* lv) {
	size_t i;
	for (i = 0; i < LexVectorSize(lv); ++i) {
		struct Token* token = LexVectorGetPointer(lv, i);
		if (token->type == TOKEN_IDENTIFIER_TYPE) {
			free(token->identifierValue);
		}
	}
}

void parseFree(struct ParseTree* tree) {
	if (!tree) {
		return;
	}

	if (tree->lhs) {
		parseFree(tree->lhs);
		free(tree->lhs);
	}
	if (tree->rhs) {
		parseFree(tree->rhs);
		free(tree->rhs);
	}

	if (tree->token.type == TOKEN_IDENTIFIER_TYPE) {
		free(tree->token.identifierValue);
	}
}

static bool _performOperation(enum Operation operation, int32_t current, int32_t next, int32_t* value) {
	switch (operation) {
	case OP_ASSIGN:
		current = next;
		break;
	case OP_ADD:
		current += next;
		break;
	case OP_SUBTRACT:
		current -= next;
		break;
	case OP_MULTIPLY:
		current *= next;
		break;
	case OP_DIVIDE:
		if (next != 0) {
			current /= next;
		} else {
			return false;
		}
		break;
	case OP_MODULO:
		if (next != 0) {
			current %= next;
		} else {
			return false;
		}
		break;
	case OP_AND:
		current &= next;
		break;
	case OP_OR:
		current |= next;
		break;
	case OP_XOR:
		current ^= next;
		break;
	case OP_LESS:
		current = current < next;
		break;
	case OP_GREATER:
		current = current > next;
		break;
	case OP_EQUAL:
		current = current == next;
		break;
	case OP_NOT_EQUAL:
		current = current != next;
		break;
	case OP_LOGICAL_AND:
		current = current && next;
		break;
	case OP_LOGICAL_OR:
		current = current || next;
		break;
	case OP_LE:
		current = current <= next;
		break;
	case OP_GE:
		current = current >= next;
		break;
	case OP_SHIFT_L:
		current <<= next;
		break;
	case OP_SHIFT_R:
		current >>= next;
		break;
	default:
		return false;
	}
	*value = current;
	return true;
}

bool mDebuggerEvaluateParseTree(struct mDebugger* debugger, struct ParseTree* tree, int32_t* value, int* segment) {
	if (!value) {
		return false;
	}
	int32_t lhs, rhs;
	switch (tree->token.type) {
	case TOKEN_UINT_TYPE:
		if (segment) {
			*segment = -1;
		}
		*value = tree->token.uintValue;
		return true;
	case TOKEN_SEGMENT_TYPE:
		if (!mDebuggerEvaluateParseTree(debugger, tree->rhs, value, segment)) {
			return false;
		}
		return mDebuggerEvaluateParseTree(debugger, tree->lhs, segment, NULL);
	case TOKEN_OPERATOR_TYPE:
		if (!mDebuggerEvaluateParseTree(debugger, tree->lhs, &lhs, segment)) {
			return false;
		}
		if (!mDebuggerEvaluateParseTree(debugger, tree->rhs, &rhs, segment)) {
			return false;
		}
		return _performOperation(tree->token.operatorValue, lhs, rhs, value);
	case TOKEN_IDENTIFIER_TYPE:
		return mDebuggerLookupIdentifier(debugger, tree->token.identifierValue, value, segment);
	case TOKEN_ERROR_TYPE:
	default:
		break;
	}
	return false;
}
