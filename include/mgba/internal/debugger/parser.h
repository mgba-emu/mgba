/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PARSER_H
#define PARSER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>

enum LexState {
	LEX_ERROR = -1,
	LEX_ROOT = 0,
	LEX_EXPECT_IDENTIFIER,
	LEX_EXPECT_BINARY,
	LEX_EXPECT_DECIMAL,
	LEX_EXPECT_HEX,
	LEX_EXPECT_PREFIX,
	LEX_EXPECT_OPERATOR
};

enum Operation {
	OP_ASSIGN,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE
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

CXX_GUARD_END

#endif
