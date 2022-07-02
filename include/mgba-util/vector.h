/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VECTOR_H
#define VECTOR_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef vector
#undef vector
#endif

#define DECLARE_VECTOR(NAME, TYPE) \
	struct NAME { \
		TYPE* vector; \
		size_t size; \
		size_t capacity; \
	}; \
	void NAME ## Init(struct NAME* vector, size_t capacity); \
	void NAME ## Deinit(struct NAME* vector); \
	TYPE* NAME ## GetPointer(struct NAME* vector, size_t location); \
	TYPE const* NAME ## GetConstPointer(const struct NAME* vector, size_t location); \
	TYPE* NAME ## Append(struct NAME* vector); \
	void NAME ## Clear(struct NAME* vector); \
	void NAME ## Resize(struct NAME* vector, ssize_t change); \
	void NAME ## Shift(struct NAME* vector, size_t location, size_t difference); \
	void NAME ## Unshift(struct NAME* vector, size_t location, size_t difference); \
	void NAME ## EnsureCapacity(struct NAME* vector, size_t capacity); \
	size_t NAME ## Size(const struct NAME* vector); \
	size_t NAME ## Index(const struct NAME* vector, const TYPE* member); \
	void NAME ## Copy(struct NAME* dest, const struct NAME* src);

#define DEFINE_VECTOR(NAME, TYPE) \
	void NAME ## Init(struct NAME* vector, size_t capacity) { \
		vector->size = 0; \
		if (capacity == 0) { \
			capacity = 4; \
		} \
		vector->capacity = capacity; \
		vector->vector = calloc(capacity, sizeof(TYPE)); \
	} \
	void NAME ## Deinit(struct NAME* vector) { \
		free(vector->vector); \
		vector->vector = 0; \
		vector->capacity = 0; \
		vector->size = 0; \
	} \
	TYPE* NAME ## GetPointer(struct NAME* vector, size_t location) { \
		return &vector->vector[location]; \
	} \
	TYPE const* NAME ## GetConstPointer(const struct NAME* vector, size_t location) { \
		return &vector->vector[location]; \
	} \
	TYPE* NAME ## Append(struct NAME* vector) { \
		NAME ## Resize(vector, 1); \
		return &vector->vector[vector->size - 1]; \
	} \
	void NAME ## Resize(struct NAME* vector, ssize_t change) { \
		if (change > 0) { \
			NAME ## EnsureCapacity(vector, vector->size + change); \
		} \
		vector->size += change; \
	} \
	void NAME ## Clear(struct NAME* vector) { \
		vector->size = 0; \
	} \
	void NAME ## EnsureCapacity(struct NAME* vector, size_t capacity) { \
		if (capacity <= vector->capacity) { \
			return; \
		} \
		while (capacity > vector->capacity) { \
			vector->capacity <<= 1; \
		} \
		vector->vector = realloc(vector->vector, vector->capacity * sizeof(TYPE)); \
	} \
	void NAME ## Shift(struct NAME* vector, size_t location, size_t difference) { \
		memmove(&vector->vector[location], &vector->vector[location + difference], (vector->size - location - difference) * sizeof(TYPE)); \
		vector->size -= difference; \
	} \
	void NAME ## Unshift(struct NAME* vector, size_t location, size_t difference) { \
		NAME ## Resize(vector, difference); \
		memmove(&vector->vector[location + difference], &vector->vector[location], (vector->size - location - difference) * sizeof(TYPE)); \
	} \
	size_t NAME ## Size(const struct NAME* vector) { \
		return vector->size; \
	} \
	size_t NAME ## Index(const struct NAME* vector, const TYPE* member) { \
		return member - (const TYPE*) vector->vector; \
	} \
	void NAME ## Copy(struct NAME* dest, const struct NAME* src) { \
		NAME ## EnsureCapacity(dest, src->size); \
		memcpy(dest->vector, src->vector, src->size * sizeof(TYPE)); \
		dest->size = src->size; \
	} \

DECLARE_VECTOR(StringList, char*);
DECLARE_VECTOR(IntList, int);

CXX_GUARD_END

#endif
