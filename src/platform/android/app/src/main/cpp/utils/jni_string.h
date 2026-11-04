/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#pragma once

#include <jni.h>

class JniString {
public:
	JniString(JNIEnv* env, jstring jstr)
	    : m_env(env), m_jstr(jstr), m_str(jstr ? env->GetStringUTFChars(jstr, nullptr) : nullptr) {}

	~JniString() {
		if (m_str && m_jstr) {
			m_env->ReleaseStringUTFChars(m_jstr, m_str);
		}
	}

	operator const char*() const { return m_str; }
	[[nodiscard]] const char* c_str() const { return m_str; }

	JniString(const JniString&) = delete;
	JniString& operator=(const JniString&) = delete;

private:
	JNIEnv* m_env;
	jstring m_jstr;
	const char* m_str;
};