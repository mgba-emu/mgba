/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/debugger/parser.h>

struct LPTest {
	struct LexVector lv;
	struct ParseTree tree;
};

#define PARSE(STR) \
	struct LPTest* lp = *state; \
	lexFree(&lp->lv); \
	LexVectorClear(&lp->lv); \
	size_t adjusted = lexExpression(&lp->lv, STR, strlen(STR), ""); \
	assert_false(adjusted > strlen(STR)); \
	struct ParseTree* tree = &lp->tree; \
	parseLexedExpression(tree, &lp->lv)

M_TEST_SUITE_SETUP(Parser) {
	struct LPTest* lp = malloc(sizeof(struct LPTest));
	LexVectorInit(&lp->lv, 0);
	*state = lp;
	return 0;
}

M_TEST_SUITE_TEARDOWN(Parser) {
	struct LPTest* lp = *state;
	parseFree(&lp->tree); \
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

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(Parser,
	cmocka_unit_test(parseEmpty),
	cmocka_unit_test(parseInt),
	cmocka_unit_test(parseLexError),
	cmocka_unit_test(parseSimpleExpression),
	cmocka_unit_test(parseAddMultplyExpression),
	cmocka_unit_test(parseParentheticalExpression),
	cmocka_unit_test(parseParentheticalAddMultplyExpression))
