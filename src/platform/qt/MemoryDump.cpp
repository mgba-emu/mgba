/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryDump.h"

#include "CoreController.h"
#include "GBAApp.h"
#include "LogController.h"

using namespace QGBA;

MemoryDump::MemoryDump(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(this, &QDialog::accepted, this, &MemoryDump::save);
}

void MemoryDump::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Save memory region"));
	if (filename.isNull()) {
		return;
	}
	QFile outfile(filename);
	if (!outfile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		LOG(QT, WARN) << tr("Failed to open output file: %1").arg(filename);
		return;
	}
	QByteArray out(serialize());
	outfile.write(out);
}

void MemoryDump::setAddress(uint32_t start) {
	m_ui.address->setValue(start);
}

void MemoryDump::setSegment(int seg) {
	m_ui.segment->setValue(seg);
}

void MemoryDump::setByteCount(uint32_t count) {
	m_ui.bytes->setValue(count);
}

QByteArray MemoryDump::serialize() {
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);

	int size = m_ui.bytes->value();
	uint32_t start = m_ui.address->value();
	int segment = m_ui.segment->value();
	bool spanSegments = m_ui.spanSegments->isChecked();

	QByteArray mem;
	while (size) {
		const mCoreMemoryBlock* bestMatch = NULL;
		const char* block = NULL;
		size_t blockSize = 0;
		for (size_t i = 0; i < nBlocks; ++i) {
			if (blocks[i].end <= start) {
				continue;
			}
			if (blocks[i].start > start) {
				continue;
			}
			block = static_cast<const char*>(core->getMemoryBlock(core, blocks[i].id, &blockSize));
			if (block) {
				bestMatch = &blocks[i];
				break;
			} else if (!bestMatch) {
				bestMatch = &blocks[i];
				blockSize = 0;
			}
		}
		if (!spanSegments) {
			blockSize = bestMatch->end - bestMatch->start;
		} else if (!blockSize) {
			blockSize = bestMatch->size;
		}
		size_t segmentSize = bestMatch->end - bestMatch->start;
		if (bestMatch->segmentStart) {
			segmentSize = bestMatch->segmentStart - bestMatch->start;
		}
		if (segment > 0) {
			start += segment * segmentSize;
		}
		int maxFromRegion = blockSize - (start - bestMatch->start);
		if (maxFromRegion <= 0) {
			continue;
		}
		int fromRegion = std::min(size, maxFromRegion);
		if (block && (segment >= 0 || segmentSize == blockSize)) {
			block = &block[start - bestMatch->start];
			mem.append(QByteArray::fromRawData(block, fromRegion));
			size -= fromRegion;
			start += fromRegion;
			if (spanSegments) {
				break;
			}
		} else {
			for (int i = 0; i < 16; ++i) {
				char datum = core->rawRead8(core, start, segment);
				mem.append(datum);
				++start;
				--size;
				if (!size) {
					break;
				}
			}
		}
	}

	return mem;
}