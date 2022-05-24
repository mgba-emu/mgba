/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/core.h>
#include <mgba/core/scripting.h>
#include <mgba/core/version.h>
#include <mgba/script/context.h>

struct mScriptContext context;
struct Table types;

void explainValue(struct mScriptValue* value, int level);
void explainType(struct mScriptType* type, int level);

void addTypesFromTuple(const struct mScriptTypeTuple*);
void addTypesFromTable(struct Table*);

void addType(const struct mScriptType* type) {
	if (HashTableLookup(&types, type->name) || type->isConst) {
		return;
	}
	HashTableInsert(&types, type->name, (struct mScriptType*) type);
	switch (type->base) {
	case mSCRIPT_TYPE_FUNCTION:
		addTypesFromTuple(&type->details.function.parameters);
		addTypesFromTuple(&type->details.function.returnType);
		break;
	case mSCRIPT_TYPE_OBJECT:
		mScriptClassInit(type->details.cls);
		if (type->details.cls->parent) {
			addType(type->details.cls->parent);
		}
		addTypesFromTable(&type->details.cls->instanceMembers);
		break;
	case mSCRIPT_TYPE_OPAQUE:
		if (type->details.type) {
			addType(type->details.type);
		}
	}
}

void addTypesFromTuple(const struct mScriptTypeTuple* tuple) {
	size_t i;
	for (i = 0; i < tuple->count; ++i) {
		addType(tuple->entries[i]);
	}
}

void addTypesFromTable(struct Table* table) {
	struct TableIterator iter;
	if (!HashTableIteratorStart(table, &iter)) {
		return;
	}
	do {
		struct mScriptClassMember* member = HashTableIteratorGetValue(table, &iter);
		addType(member->type);
	} while(HashTableIteratorNext(table, &iter));
}

void printchomp(const char* string, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	const char* start = string;
	char lineBuffer[1024];
	while (true) {
		const char* end = strchr(start, '\n');
		if (end) {
			size_t size = end - start;
			if (sizeof(lineBuffer) - 1 < size) {
				size = sizeof(lineBuffer) - 1;
			}
			strncpy(lineBuffer, start, size);
			lineBuffer[size] = '\0';
			printf("%s%s\n", indent, lineBuffer);
		} else {
			printf("%s%s\n", indent, start);
			break;
		}
		start = end + 1;
		if (!*end) {
			break;
		}
	}
}

bool printval(const struct mScriptValue* value, char* buffer, size_t bufferSize) {
	struct mScriptValue sval;
	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (value->type->size <= 4) {
			snprintf(buffer, bufferSize, "%"PRId32, value->value.s32);
			return true;
		}
		if (value->type->size == 8) {
			snprintf(buffer, bufferSize, "%"PRId64, value->value.s64);
			return true;
		}
		return false;
	case mSCRIPT_TYPE_UINT:
		if (value->type->size <= 4) {
			snprintf(buffer, bufferSize, "%"PRIu32, value->value.u32);
			return true;
		}
		if (value->type->size == 8) {
			snprintf(buffer, bufferSize, "%"PRIu64, value->value.u64);
			return true;
		}
		return false;
	case mSCRIPT_TYPE_STRING:
		if (!mScriptCast(mSCRIPT_TYPE_MS_CHARP, value, &sval)) {
			return false;
		}
		if (sval.value.opaque) {
			snprintf(buffer, bufferSize, "\"%s\"", sval.value.opaque);
		} else {
			snprintf(buffer, bufferSize, "null");
		}
		return true;
	}
	return false;
}

void explainTable(struct mScriptValue* value, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	struct TableIterator iter;
	if (mScriptTableIteratorStart(value, &iter)) {
		do {
			char keyval[1024];
			struct mScriptValue* k = mScriptTableIteratorGetKey(value, &iter);
			printval(k, keyval, sizeof(keyval));
			printf("%s- key: %s\n", indent, keyval);
			struct mScriptValue* v = mScriptTableIteratorGetValue(value, &iter);
			explainValue(v, level + 1);
		} while (mScriptTableIteratorNext(value, &iter));
	}
}

void explainClass(struct mScriptTypeClass* cls, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	if (cls->parent) {
		printf("%sparent: %s\n", indent, cls->parent->name);
	}
	if (cls->docstring) {
		if (strchr(cls->docstring, '\n')) {
			printf("%scomment: |-\n", indent);
			printchomp(cls->docstring, level + 1);
		} else {
			printf("%scomment: \"%s\"\n", indent, cls->docstring);
		}
	}

	printf("%smembers:\n", indent);
	const char* docstring = NULL;
	const struct mScriptClassInitDetails* details;
	size_t i;
	for (i = 0; cls->details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		details = &cls->details[i];
		switch (details->type) {
		case mSCRIPT_CLASS_INIT_DOCSTRING:
			docstring = details->info.comment;
			break;
		case mSCRIPT_CLASS_INIT_INSTANCE_MEMBER:
			printf("%s  %s:\n", indent, details->info.member.name);
			if (docstring) {
				printf("%s    comment: \"%s\"\n", indent, docstring);
				docstring = NULL;
			}
			printf("%s    type: %s\n", indent, details->info.member.type->name);
			break;
		}
	}
}

void explainObject(struct mScriptValue* value, int level) {
	char indent[(level + 2) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	struct mScriptTypeClass* cls = value->type->details.cls;
	const struct mScriptClassInitDetails* details;
	size_t i;
	for (i = 0; cls->details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		struct mScriptValue member;
		details = &cls->details[i];

		if (cls->details[i].type != mSCRIPT_CLASS_INIT_INSTANCE_MEMBER) {
			continue;
		}
		printf("%s%s:\n", indent, details->info.member.name);
		addType(details->info.member.type);
		if (mScriptObjectGet(value, details->info.member.name, &member)) {
			struct mScriptValue* unwrappedMember;
			if (member.type->base == mSCRIPT_TYPE_WRAPPER) {
				unwrappedMember = mScriptValueUnwrap(&member);
				explainValue(unwrappedMember, level + 2);
			} else {
				explainValue(&member, level + 2);
			}
		}
	}
}

void explainValue(struct mScriptValue* value, int level) {
	char valstring[1024];
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	value = mScriptContextAccessWeakref(&context, value);
	addType(value->type);
	printf("%stype: %s\n", indent, value->type->name);
	switch (value->type->base) {
	case mSCRIPT_TYPE_TABLE:
		printf("%svalue:\n", indent);
		explainTable(value, level);
		break;
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_STRING:
		printval(value, valstring, sizeof(valstring));
		printf("%svalue: %s\n", indent, valstring);
		break;
	case mSCRIPT_TYPE_OBJECT:
		printf("%svalue:\n", indent);
		explainObject(value, level);
		break;
	default:
		break;
	}
}

void explainTypeTuple(struct mScriptTypeTuple* tuple, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	printf("%svariable: %s\n", indent, tuple->variable ? "yes" : "no");
	printf("%slist:\n", indent);
	size_t i;
	for (i = 0; i < tuple->count; ++i) {
		if (tuple->names[i]) {
			printf("%s- name: %s\n", indent, tuple->names[i]);
			printf("%s  type: %s\n", indent, tuple->entries[i]->name);
		} else {
			printf("%s- type: %s\n", indent, tuple->entries[i]->name);
		}
		if (tuple->defaults && tuple->defaults[i].type) {
			char defaultValue[128];
			printval(&tuple->defaults[i], defaultValue, sizeof(defaultValue));
			printf("%s  default: %s\n", indent, defaultValue);
		}
	}
}

void explainType(struct mScriptType* type, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	printf("%sbase: ", indent);
	switch (type->base) {
	case mSCRIPT_TYPE_SINT:
		puts("sint");
		break;
	case mSCRIPT_TYPE_UINT:
		puts("uint");
		break;
	case mSCRIPT_TYPE_FLOAT:
		puts("float");
		break;
	case mSCRIPT_TYPE_STRING:
		puts("string");
		break;
	case mSCRIPT_TYPE_FUNCTION:
		puts("function");
		printf("%sparameters:\n", indent);
		explainTypeTuple(&type->details.function.parameters, level + 1);
		printf("%sreturn:\n", indent);
		explainTypeTuple(&type->details.function.returnType, level + 1);
		break;
	case mSCRIPT_TYPE_OPAQUE:
		puts("opaque");
		break;
	case mSCRIPT_TYPE_OBJECT:
		puts("object");
		explainClass(type->details.cls, level);
		break;
	case mSCRIPT_TYPE_LIST:
		puts("list");
		break;
	case mSCRIPT_TYPE_TABLE:
		puts("table");
		break;
	case mSCRIPT_TYPE_WRAPPER:
		puts("wrapper");
		break;
	case mSCRIPT_TYPE_WEAKREF:
		puts("weakref");
		break;
	case mSCRIPT_TYPE_VOID:
		puts("void");
		break;
	}
}

bool call(struct mScriptValue* obj, const char* method, struct mScriptFrame* frame) {
	struct mScriptValue fn;
	if (!mScriptObjectGet(obj, method, &fn)) {
		return false;
	}
	struct mScriptValue* this = mScriptListAppend(&frame->arguments);
	this->type = mSCRIPT_TYPE_MS_WRAPPER;
	this->refs = mSCRIPT_VALUE_UNREF;
	this->flags = 0;
	this->value.opaque = obj;
	return mScriptInvoke(&fn, frame);
}

void explainCore(struct mCore* core) {
	struct mScriptValue wrapper;
	mScriptContextAttachCore(&context, core);
	struct mScriptValue* emu = mScriptContextGetGlobal(&context, "emu");
	addType(emu->type);
	if (mScriptObjectGet(emu, "memory", &wrapper)) {
		struct mScriptValue* memory = mScriptValueUnwrap(&wrapper);
		struct TableIterator iter;
		printf("    memory:\n");
		if (mScriptTableIteratorStart(memory, &iter)) {
			do {
				struct mScriptValue* name = mScriptTableIteratorGetKey(memory, &iter);
				struct mScriptValue* value = mScriptTableIteratorGetValue(memory, &iter);

				printf("      %s:\n", name->value.string->buffer);
				value = mScriptContextAccessWeakref(&context, value);

				struct mScriptFrame frame;
				uint32_t baseVal;
				struct mScriptValue* shortName;

				mScriptFrameInit(&frame);
				call(value, "base", &frame);
				mScriptPopU32(&frame.returnValues, &baseVal);
				mScriptFrameDeinit(&frame);

				mScriptFrameInit(&frame);
				call(value, "name", &frame);
				shortName = mScriptValueUnwrap(mScriptListGetPointer(&frame.returnValues, 0));
				mScriptFrameDeinit(&frame);

				printf("        base: 0x%x\n", baseVal);
				printf("        name: \"%s\"\n", shortName->value.string->buffer);

				mScriptValueDeref(shortName);
			} while (mScriptTableIteratorNext(memory, &iter));
		}
	}
	mScriptContextDetachCore(&context);
}

int main(int argc, char* argv[]) {
	mScriptContextInit(&context);
	mScriptContextAttachStdlib(&context);
	mScriptContextSetTextBufferFactory(&context, NULL, NULL);
	HashTableInit(&types, 0, NULL);

	addType(mSCRIPT_TYPE_MS_S8);
	addType(mSCRIPT_TYPE_MS_U8);
	addType(mSCRIPT_TYPE_MS_S16);
	addType(mSCRIPT_TYPE_MS_U16);
	addType(mSCRIPT_TYPE_MS_S32);
	addType(mSCRIPT_TYPE_MS_U32);
	addType(mSCRIPT_TYPE_MS_F32);
	addType(mSCRIPT_TYPE_MS_S64);
	addType(mSCRIPT_TYPE_MS_U64);
	addType(mSCRIPT_TYPE_MS_F64);
	addType(mSCRIPT_TYPE_MS_STR);
	addType(mSCRIPT_TYPE_MS_CHARP);
	addType(mSCRIPT_TYPE_MS_LIST);
	addType(mSCRIPT_TYPE_MS_TABLE);
	addType(mSCRIPT_TYPE_MS_WRAPPER);

	puts("version:");
	printf("  string: \"%s\"\n", projectVersion);
	printf("  commit: \"%s\"\n", gitCommit);
	puts("root:");
	struct TableIterator iter;
	if (HashTableIteratorStart(&context.rootScope, &iter)) {
		do {
			const char* name = HashTableIteratorGetKey(&context.rootScope, &iter);
			printf("  %s:\n", name);
			struct mScriptValue* value = HashTableIteratorGetValue(&context.rootScope, &iter);
			explainValue(value, 1);
		} while (HashTableIteratorNext(&context.rootScope, &iter));
	}
	puts("emu:");
	struct mCore* core;
	core = mCoreCreate(mPLATFORM_GBA);
	if (core) {
		puts("  gba:");
		core->init(core);
		explainCore(core);
		core->deinit(core);
	}
	core = mCoreCreate(mPLATFORM_GB);
	if (core) {
		puts("  gb:");
		core->init(core);
		explainCore(core);
		core->deinit(core);
	}
	puts("types:");
	if (HashTableIteratorStart(&types, &iter)) {
		do {
			const char* name = HashTableIteratorGetKey(&types, &iter);
			printf("  %s:\n", name);
			struct mScriptType* type = HashTableIteratorGetValue(&types, &iter);
			explainType(type, 1);
		} while (HashTableIteratorNext(&types, &iter));
	}

	HashTableDeinit(&types);
	mScriptContextDeinit(&context);
	return 0;
}
