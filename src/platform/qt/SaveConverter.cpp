/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SaveConverter.h"

#include <QMessageBox>

#include "GBAApp.h"
#include "LogController.h"
#include "VFileDevice.h"
#include "utils.h"

#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba/internal/gba/sharkport.h>
#endif
#ifdef M_CORE_GB
#include <mgba/gb/core.h>
#include <mgba/internal/gb/serialize.h>
#endif

#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

SaveConverter::SaveConverter(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.inputFile, &QLineEdit::textEdited, this, &SaveConverter::refreshInputTypes);
	connect(m_ui.inputBrowse, &QAbstractButton::clicked, this, [this]() {
		QStringList formats{"*.gsv", "*.sav", "*.sgm", "*.sps", "*.ss0", "*.ss1", "*.ss2", "*.ss3", "*.ss4", "*.ss5", "*.ss6", "*.ss7", "*.ss8", "*.ss9", "*.xps"};
		QString filter = tr("Save games and save states (%1)").arg(formats.join(QChar(' ')));
		QString filename = GBAApp::app()->getOpenFileName(this, tr("Select save game or save state"), filter);
		if (!filename.isEmpty()) {
			m_ui.inputFile->setText(filename);
			refreshInputTypes();
		}
	});
	connect(m_ui.inputType, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SaveConverter::refreshOutputTypes);

	connect(m_ui.outputFile, &QLineEdit::textEdited, this, &SaveConverter::checkCanConvert);
	connect(m_ui.outputBrowse, &QAbstractButton::clicked, this, [this]() {
		// TODO: Add gameshark saves here too
		QStringList formats{"*.sav", "*.sgm"};
		QString filter = tr("Save games (%1)").arg(formats.join(QChar(' ')));
		QString filename = GBAApp::app()->getSaveFileName(this, tr("Select save game"), filter);
		if (!filename.isEmpty()) {
			m_ui.outputFile->setText(filename);
			checkCanConvert();
		}
	});
	connect(m_ui.outputType, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SaveConverter::checkCanConvert);
	connect(this, &QDialog::accepted, this, &SaveConverter::convert);

	refreshInputTypes();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setDisabled(true);
}

void SaveConverter::convert() {
	if (m_validSaves.isEmpty() || m_validOutputs.isEmpty()) {
		return;
	}
	const AnnotatedSave& input = m_validSaves[m_ui.inputType->currentIndex()];
	const AnnotatedSave& output = m_validOutputs[m_ui.outputType->currentIndex()];
	QByteArray converted = input.convertTo(output);
	if (converted.isEmpty()) {
		QMessageBox* failure = new QMessageBox(QMessageBox::Warning, tr("Conversion failed"), tr("Failed to convert the save game. This is probably a bug."),
		                                       QMessageBox::Ok, this, Qt::Sheet);
		failure->setAttribute(Qt::WA_DeleteOnClose);
		failure->show();
		return;
	}
	QFile out(m_ui.outputFile->text());
	out.open(QIODevice::WriteOnly | QIODevice::Truncate);
	out.write(converted);
	out.close();
}

void SaveConverter::refreshInputTypes() {
	m_validSaves.clear();
	m_ui.inputType->clear();
	if (m_ui.inputFile->text().isEmpty()) {
		m_ui.inputType->addItem(tr("No file selected"));	
		m_ui.inputType->setEnabled(false);
		return;
	}

	std::shared_ptr<VFileDevice> vf = std::make_shared<VFileDevice>(m_ui.inputFile->text(), QIODevice::ReadOnly);
	if (!vf->isOpen()) {
		m_ui.inputType->addItem(tr("Could not open file"));	
		m_ui.inputType->setEnabled(false);
		return;
	}
	
	detectFromSavestate(*vf);
	detectFromSize(vf);
	detectFromHeaders(vf);

	for (const auto& save : m_validSaves) {
		m_ui.inputType->addItem(save);
	}
	if (m_validSaves.count()) {
		m_ui.inputType->setEnabled(true);
	} else {
		m_ui.inputType->addItem(tr("No valid formats found"));	
		m_ui.inputType->setEnabled(false);
	}
}

void SaveConverter::refreshOutputTypes() {
	m_ui.outputType->clear();
	if (m_validSaves.isEmpty()) {
		m_ui.outputType->addItem(tr("Please select a valid input file"));
		m_ui.outputType->setEnabled(false);
		return;
	}
	m_validOutputs = m_validSaves[m_ui.inputType->currentIndex()].possibleConversions();
	for (const auto& save : m_validOutputs) {
		m_ui.outputType->addItem(save);
	}
	if (m_validOutputs.count()) {
		m_ui.outputType->setEnabled(true);
	} else {
		m_ui.outputType->addItem(tr("No valid conversions found"));	
		m_ui.outputType->setEnabled(false);
	}
	checkCanConvert();
}

void SaveConverter::checkCanConvert() {
	QAbstractButton* button = m_ui.buttonBox->button(QDialogButtonBox::Save);
	if (m_ui.inputFile->text().isEmpty()) {
		button->setEnabled(false);
		return;
	}
	if (m_ui.outputFile->text().isEmpty()) {
		button->setEnabled(false);
		return;
	}
	if (!m_ui.inputType->isEnabled()) {
		button->setEnabled(false);
		return;
	}
	if (!m_ui.outputType->isEnabled()) {
		button->setEnabled(false);
		return;
	}
	button->setEnabled(true);
}

void SaveConverter::detectFromSavestate(VFile* vf) {
	mPlatform platform = getStatePlatform(vf);
	if (platform == mPLATFORM_NONE) {
		return;
	}

	QByteArray extSavedata = getExtdata(vf, platform, EXTDATA_SAVEDATA);
	if (!extSavedata.size()) {
		return;
	}

	QByteArray state = getState(vf, platform);
	AnnotatedSave save{platform, std::make_shared<VFileDevice>(extSavedata), Endian::NONE, Container::SAVESTATE};
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		save.gba.type = static_cast<SavedataType>(state.at(offsetof(GBASerializedState, savedata.type)));
		if (save.gba.type == SAVEDATA_EEPROM || save.gba.type == SAVEDATA_EEPROM512) {
			save.endianness = Endian::LITTLE;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		// GB savestates don't store the MBC type...should probably fix that
		save.gb.type = GB_MBC_AUTODETECT;
		if (state.size() == 0x100) {
			// MBC2 packed save
			save.endianness = Endian::LITTLE;
			save.gb.type = GB_MBC2;
		}
		break;
#endif
	default:
		break;
	}
	m_validSaves.append(save);
}

void SaveConverter::detectFromSize(std::shared_ptr<VFileDevice> vf) {
#ifdef M_CORE_GBA
	switch (vf->size()) {
	case GBA_SIZE_SRAM:
		m_validSaves.append(AnnotatedSave{SAVEDATA_SRAM, vf});
		break;
	case GBA_SIZE_FLASH512:
		m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH512, vf});
		break;
	case GBA_SIZE_FLASH1M:
		m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH1M, vf});
		break;
	case GBA_SIZE_EEPROM:
		m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM, vf, Endian::LITTLE});
		m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM, vf, Endian::BIG});
		break;
	case GBA_SIZE_EEPROM512:
		m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM512, vf, Endian::LITTLE});
		m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM512, vf, Endian::BIG});
		break;
	}
#endif

#ifdef M_CORE_GB
	switch (vf->size()) {
	case 0x800:
	case 0x82C:
	case 0x830:
	case 0x2000:
	case 0x202C:
	case 0x2030:
	case 0x8000:
	case 0x802C:
	case 0x8030:
	case 0x10000:
	case 0x1002C:
	case 0x10030:
	case 0x20000:
	case 0x2002C:
	case 0x20030:
		m_validSaves.append(AnnotatedSave{GB_MBC_AUTODETECT, vf});
		break;
	case 0x100:
		m_validSaves.append(AnnotatedSave{GB_MBC2, vf, Endian::LITTLE});
		m_validSaves.append(AnnotatedSave{GB_MBC2, vf, Endian::BIG});
		break;
	case 0x200:
		m_validSaves.append(AnnotatedSave{GB_MBC2, vf});
		break;
	case GB_SIZE_MBC6_FLASH: // Flash only
	case GB_SIZE_MBC6_FLASH + 0x8000: // Concatenated SRAM and flash
		m_validSaves.append(AnnotatedSave{GB_MBC6, vf});
		break;
	case 0x20:
		m_validSaves.append(AnnotatedSave{GB_TAMA5, vf});
		break;
	}
#endif
}

void SaveConverter::detectFromHeaders(std::shared_ptr<VFileDevice> vf) {
	const QByteArray sharkport("\xd\0\0\0SharkPortSave", 0x11);
	const QByteArray gsv("ADVSAVEG", 8);
	QByteArray buffer;

	vf->seek(0);
	buffer = vf->read(sharkport.size());
	if (buffer == sharkport) {
		size_t size;
		void* data = GBASavedataSharkPortGetPayload(*vf, &size, nullptr, false);
		if (data) {
			QByteArray bytes = QByteArray::fromRawData(static_cast<const char*>(data), size);
			bytes.data(); // Trigger a deep copy before we delete the backing
			if (size == GBA_SIZE_FLASH1M) {
				m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH1M, std::make_shared<VFileDevice>(bytes), Endian::NONE, Container::SHARKPORT});
			} else {
				m_validSaves.append(AnnotatedSave{SAVEDATA_SRAM, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_SRAM)), Endian::NONE, Container::SHARKPORT});
				m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH512, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_FLASH512)), Endian::NONE, Container::SHARKPORT});
				m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_EEPROM)), Endian::BIG, Container::SHARKPORT});
				m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM512, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_EEPROM512)), Endian::BIG, Container::SHARKPORT});
			}
			free(data);
		}
	} else if (buffer.left(gsv.count()) == gsv) {
		size_t size;
		void* data = GBASavedataGSVGetPayload(*vf, &size, nullptr, false);
		if (data) {
			QByteArray bytes = QByteArray::fromRawData(static_cast<const char*>(data), size);
			bytes.data(); // Trigger a deep copy before we delete the backing
			switch (size) {
			case GBA_SIZE_FLASH1M:
				m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH1M, std::make_shared<VFileDevice>(bytes), Endian::NONE, Container::GSV});
				break;
			case GBA_SIZE_FLASH512:
				m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH512, std::make_shared<VFileDevice>(bytes), Endian::NONE, Container::GSV});
				m_validSaves.append(AnnotatedSave{SAVEDATA_FLASH1M, std::make_shared<VFileDevice>(bytes), Endian::NONE, Container::GSV});
				break;
			case GBA_SIZE_SRAM:
				m_validSaves.append(AnnotatedSave{SAVEDATA_SRAM, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_SRAM)), Endian::NONE, Container::GSV});
				break;
			case GBA_SIZE_EEPROM:
				m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_EEPROM)), Endian::BIG, Container::GSV});
				break;
			case GBA_SIZE_EEPROM512:
				m_validSaves.append(AnnotatedSave{SAVEDATA_EEPROM512, std::make_shared<VFileDevice>(bytes.left(GBA_SIZE_EEPROM512)), Endian::BIG, Container::GSV});
				break;
			}
			free(data);
		}
	}
}

mPlatform SaveConverter::getStatePlatform(VFile* vf) {
	uint32_t magic;
	void* state = nullptr;
	struct mCore* core = nullptr;
	mPlatform platform = mPLATFORM_NONE;
#ifdef M_CORE_GBA
	if (platform == mPLATFORM_NONE) {
		core = GBACoreCreate();
		core->init(core);
		state = mCoreExtractState(core, vf, nullptr);
		core->deinit(core);
		if (state) {
			LOAD_32LE(magic, 0, state);
			if (magic - GBASavestateMagic <= GBASavestateVersion) {
				platform = mPLATFORM_GBA;
			}
			mappedMemoryFree(state, core->stateSize(core));
		}
	}
#endif
#ifdef M_CORE_GB
	if (platform == mPLATFORM_NONE) {
		core = GBCoreCreate();
		core->init(core);
		state = mCoreExtractState(core, vf, nullptr);
		core->deinit(core);
		if (state) {
			LOAD_32LE(magic, 0, state);
			if (magic - GBSavestateMagic <= GBSavestateVersion) {
				platform = mPLATFORM_GB;
			}
			mappedMemoryFree(state, core->stateSize(core));
		}
	}
#endif

	return platform;
}

QByteArray SaveConverter::getState(VFile* vf, mPlatform platform) {
	QByteArray bytes;
	struct mCore* core = mCoreCreate(platform);
	core->init(core);
	void* state = mCoreExtractState(core, vf, nullptr);
	if (state) {
		size_t size = core->stateSize(core);
		bytes = QByteArray::fromRawData(static_cast<const char*>(state), size);
		bytes.data(); // Trigger a deep copy before we delete the backing
		mappedMemoryFree(state, size);
	}
	core->deinit(core);
	return bytes;
}

QByteArray SaveConverter::getExtdata(VFile* vf, mPlatform platform, mStateExtdataTag extdataType) {
	mStateExtdata extdata;
	mStateExtdataInit(&extdata);
	QByteArray bytes;
	struct mCore* core = mCoreCreate(platform);
	core->init(core);
	if (mCoreExtractExtdata(core, vf, &extdata)) {
		mStateExtdataItem extitem;
		if (mStateExtdataGet(&extdata, extdataType, &extitem) && extitem.size) {
			bytes = QByteArray::fromRawData(static_cast<const char*>(extitem.data), extitem.size);
			bytes.data(); // Trigger a deep copy before we delete the backing
		}
	}
	core->deinit(core);
	mStateExtdataDeinit(&extdata);
	return bytes;
}

SaveConverter::AnnotatedSave::AnnotatedSave()
	: container(Container::NONE)
	, platform(mPLATFORM_NONE)
	, size(0)
	, backing()
	, endianness(Endian::NONE)
{
}

SaveConverter::AnnotatedSave::AnnotatedSave(mPlatform platform, std::shared_ptr<VFileDevice> vf, Endian endianness, Container container)
	: container(container)
	, platform(platform)
	, size(vf->size())
	, backing(vf)
	, endianness(endianness)
{
}

#ifdef M_CORE_GBA
SaveConverter::AnnotatedSave::AnnotatedSave(SavedataType type, std::shared_ptr<VFileDevice> vf, Endian endianness, Container container)
	: container(container)
	, platform(mPLATFORM_GBA)
	, size(vf->size())
	, backing(vf)
	, endianness(endianness)
	, gba({type})
{
}
#endif

#ifdef M_CORE_GB
SaveConverter::AnnotatedSave::AnnotatedSave(GBMemoryBankControllerType type, std::shared_ptr<VFileDevice> vf, Endian endianness, Container container)
	: container(container)
	, platform(mPLATFORM_GB)
	, size(vf->size())
	, backing(vf)
	, endianness(endianness)
	, gb({type})
{
}
#endif

SaveConverter::AnnotatedSave SaveConverter::AnnotatedSave::asRaw() const {
	AnnotatedSave raw;
	raw.platform = platform;
	raw.size = size;
	raw.endianness = endianness;
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		raw.gba = gba;
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		raw.gb = gb;
		break;
#endif
	default:
		break;
	}
	return raw;
}

SaveConverter::AnnotatedSave::operator QString() const {
	QString sizeStr(niceSizeFormat(size));
	QString typeFormat("%1");
	QString endianStr;
	QString saveType;
	QString format = QCoreApplication::translate("QGBA::SaveConverter", "%1 %2 save game");

	switch (endianness) {
	case Endian::LITTLE:
		endianStr = QCoreApplication::translate("QGBA::SaveConverter", "little endian");
		break;
	case Endian::BIG:
		endianStr = QCoreApplication::translate("QGBA::SaveConverter", "big endian");
		break;
	default:
		break;
	}

	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		switch (gba.type) {
		case SAVEDATA_SRAM:
			typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "SRAM");
			break;
		case SAVEDATA_FLASH512:
		case SAVEDATA_FLASH1M:
			typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "%1 flash");
			break;
		case SAVEDATA_EEPROM:
		case SAVEDATA_EEPROM512:
			typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "%1 EEPROM");
			break;
		default:
			break;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		switch (gb.type) {
		case GB_MBC_AUTODETECT:
			if (size & 0xFF) {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "%1 SRAM + RTC");
			} else {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "%1 SRAM");				
			}
			break;
		case GB_MBC2:
			if (size == 0x100) {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "packed MBC2");
			} else {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "unpacked MBC2");				
			}
			break;
		case GB_MBC6:
			if (size == GB_SIZE_MBC6_FLASH) {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "MBC6 flash");
			} else if (size > GB_SIZE_MBC6_FLASH) {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "MBC6 combined SRAM + flash");				
			} else {
				typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "MBC6 SRAM");
			}
			break;
		case GB_TAMA5:
			typeFormat = QCoreApplication::translate("QGBA::SaveConverter", "TAMA5");
			break;
		default:
			break;
		}
		break;
#endif
	default:
		break;
	}
	saveType = typeFormat.arg(sizeStr);
	if (!endianStr.isEmpty()) {
		saveType = QCoreApplication::translate("QGBA::SaveConverter", "%1 (%2)").arg(saveType).arg(endianStr);
	}
	switch (container) {
	case Container::SAVESTATE:
		format = QCoreApplication::translate("QGBA::SaveConverter", "%1 save state with embedded %2 save game");
		break;
	case Container::SHARKPORT:
		format = QCoreApplication::translate("QGBA::SaveConverter", "%1 SharkPort %2 save game");
		break;
	case Container::GSV:
		format = QCoreApplication::translate("QGBA::SaveConverter", "%1 GameShark Advance SP %2 save game");
		break;
	case Container::NONE:
		break;
	}
	return format.arg(nicePlatformFormat(platform)).arg(saveType);
}

bool SaveConverter::AnnotatedSave::operator==(const AnnotatedSave& other) const {
	if (other.container != container || other.platform != platform || other.size != size || other.endianness != endianness) {
		return false;
	}
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		if (other.gba.type != gba.type) {
			return false;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (other.gb.type != gb.type) {
			return false;
		}
		break;
#endif
	default:
		break;
	}
	return true;
}

QList<SaveConverter::AnnotatedSave> SaveConverter::AnnotatedSave::possibleConversions() const {
	QList<AnnotatedSave> possible;
	AnnotatedSave same = asRaw();
	same.backing.reset();
	same.container = Container::NONE;

	if (container != Container::NONE) {
		possible.append(same);
	}

	AnnotatedSave endianSwapped = same;
	switch (endianness) {
	case Endian::LITTLE:
		endianSwapped.endianness = Endian::BIG;
		possible.append(endianSwapped);
		break;
	case Endian::BIG:
		endianSwapped.endianness = Endian::LITTLE;
		possible.append(endianSwapped);
		break;
	default:
		break;
	}

	switch (platform) {
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		switch (gb.type) {
		case GB_MBC2:
			if (size == 0x100) {
				AnnotatedSave unpacked = same;
				unpacked.size = 0x200;
				unpacked.endianness = Endian::NONE;
				possible.append(unpacked);
			} else {
				AnnotatedSave packed = same;
				packed.size = 0x100;
				packed.endianness = Endian::LITTLE;
				possible.append(packed);
				packed.endianness = Endian::BIG;
				possible.append(packed);
			}
			break;
		case GB_MBC6:
			if (size > GB_SIZE_MBC6_FLASH) {
				AnnotatedSave separated = same;
				separated.size = size - GB_SIZE_MBC6_FLASH;
				possible.append(separated);
				separated.size = GB_SIZE_MBC6_FLASH;
				possible.append(separated);
			}
			break;
		default:
			break;
		}
		break;
#endif
	default:
		break;
	}

	return possible;
}

QByteArray SaveConverter::AnnotatedSave::convertTo(const SaveConverter::AnnotatedSave& target) const {
	QByteArray converted;
	QByteArray buffer;
	backing->seek(0);
	if (target == asRaw()) {
		return backing->readAll();
	}

	if (platform != target.platform) {
		LOG(QT, ERROR) << tr("Cannot convert save games between platforms");
		return {};
	}

	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		switch (gba.type) {
		case SAVEDATA_EEPROM:
		case SAVEDATA_EEPROM512:
			if (endianness == target.endianness) {
				break;
			}
			converted.resize(target.size);
			buffer = backing->readAll();
			for (int i = 0; i < size; i += 8) {
				uint64_t word;
				const uint64_t* in = reinterpret_cast<const uint64_t*>(buffer.constData());
				uint64_t* out = reinterpret_cast<uint64_t*>(converted.data());
				LOAD_64LE(word, i, in);
				STORE_64BE(word, i, out);
			}
			break;
		default:
			break;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		switch (gb.type) {
		case GB_MBC2:
			converted.reserve(target.size);
			buffer = backing->readAll();
			if (size == 0x100 && target.size == 0x200) {
				if (endianness == Endian::LITTLE) {
					for (uint8_t byte : buffer) {
						converted.append(0xF0 | (byte & 0xF));
						converted.append(0xF0 | (byte >> 4));
					}
				} else if (endianness == Endian::BIG) {
					for (uint8_t byte : buffer) {
						converted.append(0xF0 | (byte >> 4));
						converted.append(0xF0 | (byte & 0xF));
					}
				}
			} else if (size == 0x200 && target.size == 0x100) {
				uint8_t byte;
				if (target.endianness == Endian::LITTLE) {
					for (int i = 0; i < target.size; ++i) {
						byte = buffer[i * 2] & 0xF;
						byte |= (buffer[i * 2 + 1] & 0xF) << 4;
						converted.append(byte);
					}
				} else if (target.endianness == Endian::BIG) {
					for (int i = 0; i < target.size; ++i) {
						byte = (buffer[i * 2] & 0xF) << 4;
						byte |= buffer[i * 2 + 1] & 0xF;
						converted.append(byte);
					}
				}
			} else if (size == 0x100 && target.size == 0x100) {
				for (uint8_t byte : buffer) {
					converted.append((byte >> 4) | (byte << 4));
				}
			}
			break;
		case GB_MBC6:
			if (size == target.size + GB_SIZE_MBC6_FLASH) {
				converted = backing->read(target.size);
			} else if (target.size == GB_SIZE_MBC6_FLASH) {
				backing->seek(size - GB_SIZE_MBC6_FLASH);
				converted = backing->read(GB_SIZE_MBC6_FLASH);
			}
			break;
		default:
			break;
		}
		break;
#endif
	default:
		break;
	}

	return converted;
}
