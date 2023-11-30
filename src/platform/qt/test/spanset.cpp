/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "platform/qt/utils.h"

#include <QTest>

using namespace QGBA;

class SpanSetTest : public QObject {
Q_OBJECT

private:
	void debugSpans(const SpanSet& spanSet) {
		QStringList debug;
		for (auto span : spanSet.spans) {
			debug << QStringLiteral("[%1, %2]").arg(span.left).arg(span.right);
		}
		qDebug() << QStringLiteral("SpanSet{%1}").arg(debug.join(", "));
	}

private slots:
	void oneSpan() {
		SpanSet spanSet;
		spanSet.add(1);
		spanSet.add(2);
		spanSet.add(3);
		QCOMPARE(spanSet.spans.size(), 1);
		spanSet.merge();
		QCOMPARE(spanSet.spans.size(), 1);
	}

	void twoSpans() {
		SpanSet spanSet;
		spanSet.add(1);
		spanSet.add(2);
		spanSet.add(4);
		QCOMPARE(spanSet.spans.size(), 2);
		spanSet.merge();
		QCOMPARE(spanSet.spans.size(), 2);
	}

	void mergeSpans() {
		SpanSet spanSet;
		spanSet.add(1);
		spanSet.add(3);
		spanSet.add(2);
		spanSet.add(5);
		spanSet.add(4);
		spanSet.add(7);
		spanSet.add(8);
		QCOMPARE(spanSet.spans.size(), 4);
		spanSet.merge();
		QCOMPARE(spanSet.spans.size(), 2);
	}
};

QTEST_APPLESS_MAIN(SpanSetTest)
#include "spanset.moc"
