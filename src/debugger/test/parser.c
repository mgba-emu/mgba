/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/debugger/parser.h>

struct LPTest {
	struct LexVector lv;
	struct ParseTree* tree;
};

#define PARSE(STR) \
	struct LPTest* lp = *state; \
	lexFree(&lp->lv); \
	LexVectorClear(&lp->lv); \
	size_t adjusted = lexExpression(&lp->lv, STR, strlen(STR), ""); \
	assert_false(adjusted > strlen(STR)); \
	lp->tree = malloc(sizeof(*lp->tree)); \
	struct ParseTree* tree = lp->tree; \
	parseLexedExpression(tree, &lp->lv)

static int parseSetup(void** state) {
	struct LPTest* lp = malloc(sizeof(struct LPTest));
	LexVectorInit(&lp->lv, 0);
	*state = lp;
	return 0;
}

static int parseTeardown(void** state) {
	struct LPTest* lp = *state;
	parseFree(lp->tree);
	lexFree(&lp->lv);
	LexVectorDeinit(&lp->lv);
	free(lp);
	return 0;
}

M_TEST_DEFINE(parseEmpty) {
	PARSE("");

	assert_int_equal(tree->token.type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(parseInt) {
	PARSE("0");

	assert_int_equal(tree->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->token.uintValue, 0);
}

M_TEST_DEFINE(parseLexError) {
	PARSE("@");

	assert_int_equal(tree->token.type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(parseError) {
	PARSE("1 2");

	assert_int_equal(tree->token.type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(parseSimpleExpression) {
	PARSE("1+2");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->token.operatorValue, OP_ADD);
	assert_int_equal(tree->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->token.uintValue, 1);
	assert_int_equal(tree->rhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->token.uintValue, 2);
}

M_TEST_DEFINE(parseAddMultplyExpression) {
	PARSE("1+2*3");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->token.operatorValue, OP_ADD);
	assert_int_equal(tree->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->token.uintValue, 1);
	assert_int_equal(tree->rhs->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->rhs->token.uintValue, OP_MULTIPLY);
	assert_int_equal(tree->rhs->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->lhs->token.uintValue, 2);
	assert_int_equal(tree->rhs->rhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->rhs->token.uintValue, 3);
}

M_TEST_DEFINE(parseParentheticalExpression) {
	PARSE("(1+2)");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->token.operatorValue, OP_ADD);
	assert_int_equal(tree->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->token.uintValue, 1);
	assert_int_equal(tree->rhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->token.uintValue, 2);
}

M_TEST_DEFINE(parseParentheticalAddMultplyExpression) {
	PARSE("(1+2)*3");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->token.operatorValue, OP_MULTIPLY);
	assert_int_equal(tree->lhs->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->lhs->token.uintValue, OP_ADD);
	assert_int_equal(tree->lhs->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->lhs->token.uintValue, 1);
	assert_int_equal(tree->lhs->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->rhs->token.uintValue, 2);
	assert_int_equal(tree->rhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->token.uintValue, 3);
}

M_TEST_DEFINE(parseIsolatedOperator) {
	PARSE("+");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->lhs->token.type, TOKEN_ERROR_TYPE);
	assert_int_equal(tree->rhs->token.type, TOKEN_ERROR_TYPE);
}

M_TEST_DEFINE(parseUnaryChainedOperator) {
	PARSE("1+*2");

	assert_int_equal(tree->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->token.operatorValue, OP_ADD);
	assert_int_equal(tree->lhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->lhs->token.uintValue, 1);
	assert_int_equal(tree->rhs->token.type, TOKEN_OPERATOR_TYPE);
	assert_int_equal(tree->rhs->token.operatorValue, OP_DEREFERENCE);
	assert_int_equal(tree->rhs->rhs->token.type, TOKEN_UINT_TYPE);
	assert_int_equal(tree->rhs->rhs->token.uintValue, 2);
}

M_TEST_SUITE_DEFINE(Parser,
	cmocka_unit_test_setup_teardown(parseEmpty, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseInt, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseLexError, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseError, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseSimpleExpression, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseAddMultplyExpression, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseParentheticalExpression, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseParentheticalAddMultplyExpression, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseIsolatedOperator, parseSetup, parseTeardown),
	cmocka_unit_test_setup_teardown(parseUnaryChainedOperator, parseSetup, parseTeardown))
