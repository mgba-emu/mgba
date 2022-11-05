/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderGeneratorVita.h"

#include <QFileInfo>
#include <QTemporaryFile>

#include "VFileDevice.h"

#include <mgba-util/sfo.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

ForwarderGeneratorVita::ForwarderGeneratorVita()
	: ForwarderGenerator(3)
{
}

QList<QPair<QString, QSize>> ForwarderGeneratorVita::imageTypes() const {
	return {
		{ tr("Bubble"), QSize(128, 128) },
		{ tr("Background"), QSize(840, 500) },
		{ tr("Startup"), QSize(280, 158) }
	};
}

void ForwarderGeneratorVita::rebuild(const QString& source, const QString& target) {
	QFile vpkFile(source);
	VDir* outdir = VDirOpenZip(target.toLocal8Bit().constData(), O_WRONLY | O_CREAT | O_TRUNC);
	if (outdir && !copyAssets(source, outdir)) {
		outdir->close(outdir);
		outdir = nullptr;
	}
	vpkFile.remove();
	if (!outdir) {
		emit buildFailed();
		return;
	}

	VFile* sfo = outdir->openFile(outdir, "sce_sys/param.sfo", O_WRONLY);
	writeSfo(sfo);
	sfo->close(sfo);

	QFileInfo info(rom());
	QByteArray buffer(info.fileName().toUtf8());
	VFile* filename = outdir->openFile(outdir, "filename", O_WRONLY);
	filename->write(filename, buffer.constData(), buffer.size());
	filename->close(filename);

	VFile* romfileOut = outdir->openFile(outdir, buffer.constData(), O_WRONLY);
	VFileDevice romfileIn(rom(), QIODevice::ReadOnly);
	VFileDevice::copyFile(romfileIn, romfileOut);
	romfileIn.close();
	romfileOut->close(romfileOut);

	if (!image(0).isNull()) {
		injectImage(outdir, "sce_sys/icon0.png", 0);
	}
	if (!image(1).isNull()) {
		injectImage(outdir, "sce_sys/livearea/contents/bg.png", 1);
	}
	if (!image(2).isNull()) {
		injectImage(outdir, "sce_sys/livearea/contents/startup.png", 2);
	}

	outdir->close(outdir);

	emit buildComplete();
}

bool ForwarderGeneratorVita::copyAssets(const QString& vpk, VDir* outdir) {
	VDir* indir = VDirOpenZip(vpk.toLocal8Bit().constData(), O_RDONLY);
	if (!indir) {
		return false;
	}

	bool ok = true;
	for (VDirEntry* dirent = indir->listNext(indir); dirent; dirent = indir->listNext(indir)) {
		if (dirent->name(dirent) == QLatin1String("sce_sys/param.sfo")) {
			continue;
		}
		if (dirent->name(dirent) == QLatin1String("sce_sys/icon0.png") && !image(0).isNull()) {
			continue;
		}
		if (dirent->name(dirent) == QLatin1String("sce_sys/livearea/contents/bg.png") && !image(1).isNull()) {
			continue;
		}
		if (dirent->name(dirent) == QLatin1String("sce_sys/livearea/contents/startup.png") && !image(2).isNull()) {
			continue;
		}
		if (dirent->type(dirent) != VFS_FILE) {
			continue;
		}

		VFile* infile = indir->openFile(indir, dirent->name(dirent), O_RDONLY);
		if (!infile) {
			ok = false;
			break;
		}

		VFile* outfile = outdir->openFile(outdir, dirent->name(dirent), O_WRONLY);
		if (!outfile) {
			infile->close(infile);
			ok = false;
			break;
		}

		VFileDevice::copyFile(infile, outfile);

		infile->close(infile);
		outfile->close(outfile);
	}

	indir->close(indir);
	return ok;
}

QString ForwarderGeneratorVita::makeSerial() const {
	QString serial("MF");
	serial += base36(hashRom(), 7);
	return serial;
}

void ForwarderGeneratorVita::writeSfo(VFile* out) {
	Table sfo;
	QByteArray serial(makeSerial().toLocal8Bit());
	QByteArray titleBytes(title().toUtf8());
	SfoInit(&sfo);
	SfoSetTitle(&sfo, titleBytes.constData());
	SfoAddStrValue(&sfo, "TITLE_ID", serial.constData());
	SfoWrite(&sfo, out);
	SfoDeinit(&sfo);
}

void ForwarderGeneratorVita::injectImage(VDir* out, const char* name, int index) {
	VFile* outfile = out->openFile(out, name, O_WRONLY);
	VFileDevice outdev(outfile);
	image(index).convertToFormat(QImage::Format_Indexed8).save(&outdev, "PNG");
}
