/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/table.h>

M_TEST_DEFINE(basic) {
	struct Table table;
	TableInit(&table, 0, NULL);

	size_t i;
	for (i = 0; i < 5000; ++i) {
		TableInsert(&table, i, (void*) i);
	}

	for (i = 0; i < 5000; ++i) {
		assert_int_equal(i, (size_t) TableLookup(&table, i));
	}

	TableDeinit(&table);
}

M_TEST_DEFINE(iterator) {
	struct Table table;
	struct TableIterator iter;

	TableInit(&table, 0, NULL);
	assert_false(TableIteratorStart(&table, &iter));

	size_t i;
	for (i = 0; i < 32; ++i) {
		TableInsert(&table, i, (void*) i);
	}

	assert_true(TableIteratorStart(&table, &iter));
	uint32_t mask = 0;
	while (true) {
		assert_int_equal(TableIteratorGetKey(&table, &iter), (uintptr_t) TableIteratorGetValue(&table, &iter));
		mask ^= 1 << TableIteratorGetKey(&table, &iter);
		if (!TableIteratorNext(&table, &iter)) {
			break;
		}
	}
	assert_int_equal(mask, 0xFFFFFFFFU);

	TableDeinit(&table);
}

M_TEST_DEFINE(iteratorLookup) {
	struct Table table;
	struct TableIterator iter;

	TableInit(&table, 0, NULL);

	size_t i;
	for (i = 0; i < 500; ++i) {
		TableInsert(&table, (i * 0x5DEECE66D) >> 16, (void*) i);
	}

	for (i = 0; i < 500; ++i) {
		assert_true(TableIteratorLookup(&table, &iter, (i * 0x5DEECE66D) >> 16));
		assert_int_equal(TableIteratorGetKey(&table, &iter), (i * 0x5DEECE66D) >> 16);
		assert_int_equal((uintptr_t) TableIteratorGetValue(&table, &iter), i);
	}
	for (i = 1000; i < 1200; ++i) {
		assert_false(TableIteratorLookup(&table, &iter, (i * 0x5DEECE66D) >> 16));
	}

	TableDeinit(&table);
}

M_TEST_DEFINE(hash) {
	struct Table table;
	HashTableInit(&table, 0, NULL);

	size_t i;
	for (i = 0; i < 5000; ++i) {
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%"PRIz"i", i);
		HashTableInsert(&table, buffer, (void*) i);
	}

	for (i = 0; i < 5000; ++i) {
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%"PRIz"i", i);
		assert_int_equal(i, (size_t) HashTableLookup(&table, buffer));
	}

	HashTableDeinit(&table);
}

M_TEST_DEFINE(hashIterator) {
	struct Table table;
	struct TableIterator iter;
	char buf[18];

	HashTableInit(&table, 0, NULL);
	assert_false(HashTableIteratorStart(&table, &iter));

	size_t i;
	for (i = 0; i < 32; ++i) {
		snprintf(buf, sizeof(buf), "%zu", i);
		HashTableInsert(&table, buf, (void*) i);
	}

	assert_true(TableIteratorStart(&table, &iter));
	uint32_t mask = 0;
	while (true) {
		assert_int_equal(atoi(HashTableIteratorGetKey(&table, &iter)), (uintptr_t) HashTableIteratorGetValue(&table, &iter));
		mask ^= 1 << atoi(HashTableIteratorGetKey(&table, &iter));
		if (!HashTableIteratorNext(&table, &iter)) {
			break;
		}
	}
	assert_int_equal(mask, 0xFFFFFFFFU);

	HashTableDeinit(&table);
}


M_TEST_DEFINE(hashIteratorLookup) {
	struct Table table;
	struct TableIterator iter;
	char buf[18];

	HashTableInit(&table, 0, NULL);

	size_t i;
	for (i = 0; i < 500; ++i) {
		snprintf(buf, sizeof(buf), "%zu", (i * 0x5DEECE66D) >> 4);
		HashTableInsert(&table, buf, (void*) i);
	}

	for (i = 0; i < 500; ++i) {
		snprintf(buf, sizeof(buf), "%zu", (i * 0x5DEECE66D) >> 4);
		assert_true(HashTableIteratorLookup(&table, &iter, buf));
		assert_string_equal(HashTableIteratorGetKey(&table, &iter), buf);
		assert_int_equal((uintptr_t) HashTableIteratorGetValue(&table, &iter), i);
	}
	for (i = 1000; i < 1200; ++i) {
		snprintf(buf, sizeof(buf), "%zu", (i * 0x5DEECE66D) >> 4);
		assert_false(HashTableIteratorLookup(&table, &iter, buf));
	}

	HashTableDeinit(&table);
}

M_TEST_SUITE_DEFINE(Table,
	cmocka_unit_test(basic),
	cmocka_unit_test(iterator),
	cmocka_unit_test(iteratorLookup),
	cmocka_unit_test(hash),
	cmocka_unit_test(hashIterator),
	cmocka_unit_test(hashIteratorLookup),
)
