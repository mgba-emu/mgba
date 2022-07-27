/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheckBoxDelegate.h"
#include <QStyle>
#include <QAbstractItemView>
#include <QtDebug>

using namespace QGBA;

CheckBoxDelegate::CheckBoxDelegate(QObject* parent)
	: QStyledItemDelegate(parent)
{
	// initializers only
}

void CheckBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
	QAbstractItemView* view = qobject_cast<QAbstractItemView*>(option.styleObject);
	if (view && (index.flags() & Qt::ItemIsUserCheckable)) {
		// Set up style options
		QStyleOptionViewItem newOption(option);
		initStyleOption(&newOption, index);
		if (view->currentIndex() == index && (newOption.state & QStyle::State_HasFocus)) {
			newOption.state |= QStyle::State_KeyboardFocusChange;
		}
		if (newOption.checkState == Qt::PartiallyChecked) {
			newOption.state |= QStyle::State_NoChange;
		} else if (newOption.checkState == Qt::Checked) {
			newOption.state |= QStyle::State_On;
		} else {
			newOption.state |= QStyle::State_Off;
		}

		// Draw background
		view->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &newOption, painter, view);

		// Draw checkbox
		int checkWidth = view->style()->pixelMetric(QStyle::PM_IndicatorWidth);
		int checkHeight = view->style()->pixelMetric(QStyle::PM_IndicatorHeight);
		int xMargin = (newOption.rect.width() - checkWidth) / 2;
		int yMargin = (newOption.rect.height() - checkHeight) / 2;
		newOption.rect.setRect(newOption.rect.left() + xMargin, newOption.rect.top() + yMargin, checkWidth, checkHeight);
		view->style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &newOption, painter, view);
	} else {
		QStyledItemDelegate::paint(painter, option, index);
	}
}
