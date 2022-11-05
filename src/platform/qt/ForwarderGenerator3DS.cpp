/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderGenerator3DS.h"

#include "ConfigController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <mgba/core/version.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

ForwarderGenerator3DS::ForwarderGenerator3DS()
	: ForwarderGenerator(2)
{
	connect(this, &ForwarderGenerator::buildFailed, this, &ForwarderGenerator3DS::cleanup);
	connect(this, &ForwarderGenerator::buildComplete, this, &ForwarderGenerator3DS::cleanup);
}

QList<QPair<QString, QSize>> ForwarderGenerator3DS::imageTypes() const {
	return {
		{ tr("Icon"), QSize(48, 48) },
		{ tr("Banner"), QSize(256, 128) }
	};
}

void ForwarderGenerator3DS::rebuild(const QString& source, const QString& target) {
	m_cia = source;
	m_target = target;
	extractCia();
}

void ForwarderGenerator3DS::extractCia() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("ctrtool");

	QStringList args;
	args << QString("--contents=%0/cxi").arg(ConfigController::cacheDir());
	args << m_cia;
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::extractCxi);
	m_currentProc->start(QIODevice::ReadOnly);
}

void ForwarderGenerator3DS::extractCxi() {
	QStringList output = QString::fromUtf8(m_currentProc->readAll()).split("\n");
	QString index;
	for (const QString& line : output) {
		if (!line.contains("|- ContentId:")) {
			continue;
		}
		index = line.trimmed().right(8);
	}
	m_cxi = ConfigController::cacheDir() + "/cxi.0000." + index;

	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("3dstool");

	QStringList args;
	init3dstoolArgs(args, m_cxi);
	args << "--exh" << ConfigController::cacheDir() + "/exheader.bin";
	args << "--header" << ConfigController::cacheDir() + "/header.bin";
	args << "--exefs" << ConfigController::cacheDir() + "/exefs.bin";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::extractExefs);
	m_currentProc->start(QIODevice::ReadOnly);
}

void ForwarderGenerator3DS::extractExefs() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("3dstool");

	QStringList args;
	init3dstoolArgs(args, ConfigController::cacheDir() + "/exefs.bin");
	args << "--header" << ConfigController::cacheDir() + "/exeheader.bin";
	args << "--exefs-dir" << ConfigController::cacheDir() + "/exefs";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::processCxi);
	m_currentProc->start(QIODevice::ReadOnly);
}

void ForwarderGenerator3DS::processCxi() {
	QByteArray hash = hashRom();
	QByteArray tid = hash.left(4);
	quint32 tidNum;
	LOAD_32LE(tidNum, 0, tid.data());
	tidNum &= 0x7FFFFFF;
	tidNum += 0x0300000;
	STORE_32LE(tidNum, 0, tid.data());

	QFile header(ConfigController::cacheDir() + "/header.bin");
	if (!header.open(QIODevice::ReadWrite)) {
		emit buildFailed();
		return;
	}
	header.seek(0x108);
	header.write(tid);
	header.seek(0x118);
	header.write(tid);

	QByteArray productCode("MGBA-");
	productCode += base36(hash, 11).toLatin1();
	header.seek(0x150);
	header.write(productCode);

	header.seek(0x18D);
	QByteArray type = header.read(3);
	type[0] = type[0] | 1; // Has romfs
	type[2] = type[2] & ~2; // Can mount romfs
	header.seek(0x18D);
	header.write(type);
	header.close();

	QFile exheader(ConfigController::cacheDir() + "/exheader.bin");
	if (!exheader.open(QIODevice::ReadWrite)) {
		emit buildFailed();
		return;
	}
	exheader.seek(0x1C8);
	exheader.write(tid);
	exheader.seek(0x200);
	exheader.write(tid);
	exheader.seek(0x600);
	exheader.write(tid);
	exheader.close();

	prepareRomfs();
}

void ForwarderGenerator3DS::prepareRomfs() {
	QDir romfsDir(ConfigController::cacheDir());

	romfsDir.mkdir("romfs");
	romfsDir.cd("romfs");

	QFileInfo info(rom());
	QByteArray buffer(info.fileName().toUtf8());
	QFile filename(romfsDir.filePath("filename"));
	if (!filename.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
		emit buildFailed();
		return;
	}
	if (filename.write(buffer) != filename.size()) {
		emit buildFailed();
		return;
	}
	filename.close();

	if (!QFile::copy(info.filePath(), romfsDir.filePath(info.fileName()))) {
		emit buildFailed();
		return;
	}

	buildRomfs();
}

void ForwarderGenerator3DS::buildRomfs() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("3dstool");

	QStringList args;
	init3dstoolArgs(args, ConfigController::cacheDir() + "/romfs.bin", "romfs");
	args << "--romfs-dir";
	args << ConfigController::cacheDir() + "/romfs";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildSmdh);
	m_currentProc->start(QIODevice::NotOpen);
}

void ForwarderGenerator3DS::buildSmdh() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("bannertool");

	if (image(0).isNull()) {
		QFile::copy(":/res/mgba-48.png", ConfigController::cacheDir() + "/smdh.png");
	} else {
		image(0).save(ConfigController::cacheDir() + "/smdh.png", "PNG");
	}

	QStringList args;
	args << "makesmdh";

	args << "-s" << title();
	args << "-l" << title();
	args << "-p" << projectName + QString(" Forwarder");
	args << "-i" << ConfigController::cacheDir() + "/smdh.png";
	args << "-o" << ConfigController::cacheDir() + "/exefs/icon.icn";
	m_currentProc->setArguments(args);

	if (image(1).isNull()) {
		connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildExefs);
	} else {
		connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildBanner);
	}
	m_currentProc->start(QIODevice::ReadOnly);
}

void ForwarderGenerator3DS::buildBanner() {
	QFile banner(ConfigController::cacheDir() + "/exefs/banner.bnr");
	if (!banner.open(QIODevice::ReadOnly)) {
		emit buildFailed();
		return;
	}

	banner.seek(0x84);
	QByteArray bcwavOffsetBuffer(banner.read(4));
	qint64 bcwavOffset;
	LOAD_64LE(bcwavOffset, 0, bcwavOffsetBuffer.data());
	banner.seek(bcwavOffset);
	QByteArray bcwav(banner.readAll());
	QFile bcwavFile(ConfigController::cacheDir() + "/banner.bcwav");
	if (!bcwavFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		emit buildFailed();
		return;
	}
	bcwavFile.write(bcwav);
	banner.close();

	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("bannertool");

	image(1).save(ConfigController::cacheDir() + "/banner.png", "PNG");

	QStringList args;
	args << "makebanner";

	args << "-i" << ConfigController::cacheDir() + "/banner.png";
	args << "-ca" << ConfigController::cacheDir() + "/banner.bcwav";
	args << "-o" << ConfigController::cacheDir() + "/exefs/banner.bnr";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildExefs);
	m_currentProc->start(QIODevice::ReadOnly);
}

void ForwarderGenerator3DS::buildExefs() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("3dstool");

	QStringList args;
	init3dstoolArgs(args, ConfigController::cacheDir() + "/exefs.bin", "exefs");
	args << "--header" << ConfigController::cacheDir() + "/exeheader.bin";
	args << "--exefs-dir" << ConfigController::cacheDir() + "/exefs";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildCxi);
	m_currentProc->start(QIODevice::NotOpen);
}

void ForwarderGenerator3DS::buildCxi() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("3dstool");

	QFile cxi(m_cxi);
	cxi.remove();

	QStringList args;
	init3dstoolArgs(args, m_cxi, "cxi");
	args << "--exh" << ConfigController::cacheDir() + "/exheader.bin";
	args << "--header" << ConfigController::cacheDir() + "/header.bin";
	args << "--exefs" << ConfigController::cacheDir() + "/exefs.bin";
	args << "--romfs" << ConfigController::cacheDir() + "/romfs.bin";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildCia);
	m_currentProc->start(QIODevice::NotOpen);
}

void ForwarderGenerator3DS::buildCia() {
	m_currentProc = std::make_unique<QProcess>();
	m_currentProc->setProgram("makerom");

	QStringList args;
	args << "-f" << "cia";
	args << "-o" << m_target;
	args << "-content" << m_cxi + ":0:0";
	m_currentProc->setArguments(args);

	connect(m_currentProc.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ForwarderGenerator3DS::buildComplete);
	m_currentProc->start(QIODevice::NotOpen);
}

void ForwarderGenerator3DS::cleanup() {
	for (const QString& path : {m_cia, m_cxi}) {
		QFile file(path);
		if (file.exists()) {
			file.remove();
		}
	}

	QDir cacheDir(ConfigController::cacheDir());
	QStringList files{
		"romfs.bin",
		"exefs.bin",
		"exheader.bin",
		"exeheader.bin",
		"header.bin",
		"smdh.png",
		"banner.png",
		"banner.bcwav",
	};
	for (QString path : files) {
		QFile file(cacheDir.filePath(path));
		if (file.exists()) {
			file.remove();
		}
	}

	for (QString path : {"romfs", "exefs"}) {
		QDir dir(cacheDir.filePath(path));
		dir.removeRecursively();
	}
}

void ForwarderGenerator3DS::init3dstoolArgs(QStringList& args, const QString& file, const QString& createType) {
	if (createType.isEmpty()) {
		args << "-xf" << file;
	} else {
		args << "-cf" << file;
		args << "-t" << createType;
	}
}
