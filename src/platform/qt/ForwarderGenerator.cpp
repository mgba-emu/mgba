/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderGenerator.h"

#include <QCryptographicHash>
#include <QFile>

#include "ForwarderGenerator3DS.h"
#include "ForwarderGeneratorVita.h"
#include "utils.h"
#include "VFileDevice.h"

#include <mgba-util/vfs.h>

using namespace QGBA;

std::unique_ptr<ForwarderGenerator> ForwarderGenerator::createForSystem(System system) {
	switch (system) {
	case System::N3DS:
		return std::make_unique<ForwarderGenerator3DS>();
	case System::VITA:
		return std::make_unique<ForwarderGeneratorVita>();
	}
	return nullptr;
}

ForwarderGenerator::ForwarderGenerator(int imageTypes, QObject* parent)
	: QObject(parent)
{
	m_images.resize(imageTypes);
}

void ForwarderGenerator::setImage(int index, const QImage& image) {
	if (index < 0 || index >= m_images.count()) {
		return;
	}

	m_images[index] = image;
}

QImage ForwarderGenerator::image(int index) const {
	if (index >= m_images.size()) {
		return {};
	}
	return m_images[index];
}

QByteArray ForwarderGenerator::hashRom() const {
	if (m_romPath.isEmpty()) {
		return {};
	}

	QFile romFile(m_romPath);
	if (!romFile.open(QIODevice::ReadOnly)) {
		return {};
	}

	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&romFile)) {
		return {};
	}

	return hash.result();
}

QString ForwarderGenerator::systemName(ForwarderGenerator::System system) {
	switch (system) {
	case ForwarderGenerator::System::N3DS:
		return QLatin1String("3ds");
	case ForwarderGenerator::System::VITA:
		return QLatin1String("vita");
	}

	return {};
}

QString ForwarderGenerator::systemHumanName(ForwarderGenerator::System system) {
	switch (system) {
	case ForwarderGenerator::System::N3DS:
		return tr("3DS");
	case ForwarderGenerator::System::VITA:
		return tr("Vita");
	}

	return {};
}

QString ForwarderGenerator::extract(const QString& archive) {
	VDir* inArchive = VFileDevice::openArchive(archive);
	if (!inArchive) {
		return {};
	}
	bool gotFile = extractMatchingFile(inArchive, [this](VDirEntry* dirent) -> QString {
		if (dirent->type(dirent) != VFS_FILE) {
			return {};
		}
		QString filename(dirent->name(dirent));
		if (!filename.endsWith("." + extension())) {
			return {};
		}
		return "tmp." + extension();
	});
	inArchive->close(inArchive);

	if (gotFile) {
		return QLatin1String("tmp.") + extension();
	}
	return {};
}

QString ForwarderGenerator::base36(const QByteArray& bytes, int length) {
	static const char* alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	QString buffer(length, 'X');
	quint32 running = 0;
	for (int i = 0, j = 0; i < length; ++i) {
		if (running < 36) {
			running <<= 8;
			running |= static_cast<quint8>(bytes[j]);
			++j;
		}
		buffer[i] = alphabet[running % 36];
		running /= 36;
	}
	return buffer;
}
