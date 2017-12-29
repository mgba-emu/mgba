/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/debugger/parser.h>

#define LEX(STR) \
	struct LexVector* lv = *state; \
	lexFree(lv); \
	LexVectorClear(lv); \
	size_t adjusted = lexExpression(lv, STR, strlen(STR)); \
	assert_false(adjusted > strlen(STR))

M_TEST_SUITE_SETUP(Lexer) {
	struct LexVector* lv = malloc(sizeof(struct LexVector));
	LexVectorInit(lv, 0);
	*state = lv;
	return 0;
}

M_TEST_SUITE_TEARDOWN(Lexer) {
	struct LexVector* lv = *state;
	lexFree(lv);
	LexVectorDeinit(lv);
	free(lv);
	return 0;
}

M_TEST_DEFINE(lexEmpty) {
	LEX("");

	assert_int_equal(LexVectorSize(lv), 0);
}

M_TEST_DEFINE(lexInt) {
	LEX("0");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 0);
}

M_TEST_DEFINE(lexDecimal) {
	LEX("10");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 10);
}

M_TEST_DEFINE(lexBinary) {
	LEX("0b10");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 2);
}

M_TEST_DEFINE(lexSigilBinary) {
	LEX("%10");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 2);
}

M_TEST_DEFINE(lexHex) {
	LEX("0x10");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 0x10);
}

M_TEST_DEFINE(lexSigilHex) {
	LEX("$10");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 0x10);
}

M_TEST_DEFINE(lexInvalidDecimal) {
	LEX("1a");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexInvalidBinary) {
	LEX("0b12");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexInvalidHex) {
	LEX("0x1g");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexTruncatedBinary) {
	LEX("0b");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexTruncatedSigilBinary) {
	LEX("%");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexTruncatedSigilHex) {
	LEX("$");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexTruncatedHex) {
	LEX("0x");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(lexSigilSegmentHex) {
	LEX("$01:0010");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_SEGMENT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->uintValue, 0x10);
}

M_TEST_DEFINE(lexIdentifier) {
	LEX("x");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
}

M_TEST_DEFINE(lexAddOperator) {
	LEX("1+");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_ADD);
}

M_TEST_DEFINE(lexIdentifierAddOperator) {
	LEX("x+");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_ADD);
}

M_TEST_DEFINE(lexSubOperator) {
	LEX("1-");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_SUBTRACT);
}

M_TEST_DEFINE(lexIdentifierSubOperator) {
	LEX("x-");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_SUBTRACT);
}

M_TEST_DEFINE(lexMulOperator) {
	LEX("1*");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_MULTIPLY);
}

M_TEST_DEFINE(lexIdentifierMulOperator) {
	LEX("x*");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_MULTIPLY);
}

M_TEST_DEFINE(lexDivOperator) {
	LEX("1/");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_DIVIDE);
}

M_TEST_DEFINE(lexIdentifierDivOperator) {
	LEX("x/");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_DIVIDE);
}

M_TEST_DEFINE(lexModOperator) {
	LEX("1%");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_MODULO);
}

M_TEST_DEFINE(lexIdentifierModOperator) {
	LEX("x%");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_MODULO);
}

M_TEST_DEFINE(lexAndOperator) {
	LEX("1&");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_AND);
}

M_TEST_DEFINE(lexIdentifierAndOperator) {
	LEX("x&");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_AND);
}

M_TEST_DEFINE(lexOrOperator) {
	LEX("1|");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_OR);
}

M_TEST_DEFINE(lexIdentifierOrOperator) {
	LEX("x|");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_OR);
}

M_TEST_DEFINE(lexXorOperator) {
	LEX("1^");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_XOR);
}

M_TEST_DEFINE(lexIdentifierXorOperator) {
	LEX("x^");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_XOR);
}

M_TEST_DEFINE(lexLessOperator) {
	LEX("1<");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_LESS);
}

M_TEST_DEFINE(lexIdentifierLessOperator) {
	LEX("x<");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_LESS);
}

M_TEST_DEFINE(lexGreaterOperator) {
	LEX("1>");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_GREATER);
}

M_TEST_DEFINE(lexIdentifierGreaterOperator) {
	LEX("x>");

	assert_int_equal(LexVectorSize(lv), 2);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 0)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_GREATER);
}

M_TEST_DEFINE(lexSimpleExpression) {
	LEX("1+1");

	assert_int_equal(LexVectorSize(lv), 3);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 0)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->operatorValue, OP_ADD);
	assert_int_equal(LexVectorGetPointer(lv, 2)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 2)->uintValue, 1);
}

M_TEST_DEFINE(lexOpenParen) {
	LEX("(");

	assert_int_equal(LexVectorSize(lv), 1);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_OPEN_PAREN_TYPE);
}

M_TEST_DEFINE(lexCloseParen) {
	LEX("(0)");

	assert_int_equal(LexVectorSize(lv), 3);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_OPEN_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->uintValue, 0);
	assert_int_equal(LexVectorGetPointer(lv, 2)->type, TOKEN_CLOSE_PAREN_TYPE);
}

M_TEST_DEFINE(lexIdentifierCloseParen) {
	LEX("(x)");

	assert_int_equal(LexVectorSize(lv), 3);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_OPEN_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_IDENTIFIER_TYPE);
	assert_string_equal(LexVectorGetPointer(lv, 1)->identifierValue, "x");
	assert_int_equal(LexVectorGetPointer(lv, 2)->type, TOKEN_CLOSE_PAREN_TYPE);
}

M_TEST_DEFINE(lexParentheticalExpression) {
	LEX("(1+1)");

	assert_int_equal(LexVectorSize(lv), 5);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_OPEN_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 2)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 2)->operatorValue, OP_ADD);
	assert_int_equal(LexVectorGetPointer(lv, 3)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 3)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 4)->type, TOKEN_CLOSE_PAREN_TYPE);
}

M_TEST_DEFINE(lexNestedParentheticalExpression) {
	LEX("(1+(2+3))");

	assert_int_equal(LexVectorSize(lv), 9);
	assert_int_equal(LexVectorGetPointer(lv, 0)->type, TOKEN_OPEN_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 1)->uintValue, 1);
	assert_int_equal(LexVectorGetPointer(lv, 2)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 2)->operatorValue, OP_ADD);
	assert_int_equal(LexVectorGetPointer(lv, 3)->type, TOKEN_OPEN_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 4)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 4)->uintValue, 2);
	assert_int_equal(LexVectorGetPointer(lv, 5)->type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 5)->operatorValue, OP_ADD);
	assert_int_equal(LexVectorGetPointer(lv, 6)->type, TOKEN_UINT_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 6)->uintValue, 3);
	assert_int_equal(LexVectorGetPointer(lv, 7)->type, TOKEN_CLOSE_PAREN_TYPE);
	assert_int_equal(LexVectorGetPointer(lv, 8)->type, TOKEN_CLOSE_PAREN_TYPE);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(Lexer,
	cmocka_unit_test(lexEmpty),
	cmocka_unit_test(lexInt),
	cmocka_unit_test(lexDecimal),
	cmocka_unit_test(lexBinary),
	cmocka_unit_test(lexSigilBinary),
	cmocka_unit_test(lexHex),
	cmocka_unit_test(lexSigilHex),
	cmocka_unit_test(lexSigilSegmentHex),
	cmocka_unit_test(lexInvalidDecimal),
	cmocka_unit_test(lexInvalidHex),
	cmocka_unit_test(lexInvalidBinary),
	cmocka_unit_test(lexTruncatedHex),
	cmocka_unit_test(lexTruncatedSigilHex),
	cmocka_unit_test(lexTruncatedBinary),
	cmocka_unit_test(lexTruncatedSigilBinary),
	cmocka_unit_test(lexIdentifier),
	cmocka_unit_test(lexAddOperator),
	cmocka_unit_test(lexIdentifierAddOperator),
	cmocka_unit_test(lexSubOperator),
	cmocka_unit_test(lexIdentifierSubOperator),
	cmocka_unit_test(lexMulOperator),
	cmocka_unit_test(lexIdentifierMulOperator),
	cmocka_unit_test(lexDivOperator),
	cmocka_unit_test(lexIdentifierDivOperator),
	cmocka_unit_test(lexModOperator),
	cmocka_unit_test(lexIdentifierModOperator),
	cmocka_unit_test(lexAndOperator),
	cmocka_unit_test(lexIdentifierAndOperator),
	cmocka_unit_test(lexOrOperator),
	cmocka_unit_test(lexIdentifierOrOperator),
	cmocka_unit_test(lexXorOperator),
	cmocka_unit_test(lexIdentifierXorOperator),
	cmocka_unit_test(lexLessOperator),
	cmocka_unit_test(lexIdentifierLessOperator),
	cmocka_unit_test(lexGreaterOperator),
	cmocka_unit_test(lexIdentifierGreaterOperator),
	cmocka_unit_test(lexSimpleExpression),
	cmocka_unit_test(lexOpenParen),
	cmocka_unit_test(lexCloseParen),
	cmocka_unit_test(lexIdentifierCloseParen),
	cmocka_unit_test(lexParentheticalExpression),
	cmocka_unit_test(lexNestedParentheticalExpression))
