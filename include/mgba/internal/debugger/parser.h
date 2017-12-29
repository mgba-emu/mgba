/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PARSER_H
#define PARSER_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum Operation {
	OP_ASSIGN,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_LESS,
	OP_GREATER,
	OP_EQUAL,
	OP_LE,
	OP_GE,
};

struct Token {
	enum TokenType {
		TOKEN_ERROR_TYPE,
		TOKEN_UINT_TYPE,
		TOKEN_IDENTIFIER_TYPE,
		TOKEN_OPERATOR_TYPE,
		TOKEN_OPEN_PAREN_TYPE,
		TOKEN_CLOSE_PAREN_TYPE,
		TOKEN_SEGMENT_TYPE,
	} type;
	union {
		uint32_t uintValue;
		char* identifierValue;
		enum Operation operatorValue;
	};
};

struct LexVector {
	struct LexVector* next;
	struct Token token;
};

struct ParseTree {
	struct Token token;
	struct ParseTree* lhs;
	struct ParseTree* rhs;
};

size_t lexExpression(struct LexVector* lv, const char* string, size_t length);
void parseLexedExpression(struct ParseTree* tree, struct LexVector* lv);

void lexFree(struct LexVector* lv);
void parseFree(struct ParseTree* tree);

struct mDebugger;
bool mDebuggerEvaluateParseTree(struct mDebugger* debugger, struct ParseTree* tree, int32_t* value, int* segment);

CXX_GUARD_END

#endif
