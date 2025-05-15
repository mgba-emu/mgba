/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QtDebug>

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(QT);

namespace QGBA {

class LogController;

class Log {
private:
	class Stream {
	public:
		Stream(Log* target, int level, int category);
		~Stream();

		Stream& operator<<(const QString&);
		template <typename T>
		Stream& operator<<(const T& value) {
			QString formatted;
			QDebug dbg(&formatted);
			dbg.noquote() << value;
			*this << formatted;
			return *this;
		}

	private:
		int m_level;
		int m_category;
		Log* m_log;

		QStringList m_queue;
	};

	static Log* s_target;

public:
	static Stream log(int level, int category);
	static void setDefaultTarget(Log* target);

	Log();
	~Log();

	virtual void postLog(int level, int category, const QString& string);
};

}

#define LOG(C, L) (QGBA::Log::log(mLOG_ ## L, _mLOG_CAT_ ## C))
