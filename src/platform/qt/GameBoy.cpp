/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GameBoy.h"

#include <QCoreApplication>
#include <QMap>

using namespace QGBA;

static const QList<GBModel> s_gbModelList{
	GB_MODEL_DMG,
	GB_MODEL_SGB,
	GB_MODEL_CGB,
	GB_MODEL_AGB,
	GB_MODEL_SCGB,
};

static const QList<GBMemoryBankControllerType> s_mbcList{
	GB_MBC_NONE,
	GB_MBC1,
	GB_MBC2,
	GB_MBC3,
	GB_MBC3_RTC,
	GB_MBC5,
	GB_MBC5_RUMBLE,
	GB_MBC6,
	GB_MBC7,
	GB_MMM01,
	GB_POCKETCAM,
	GB_TAMA5,
	GB_HuC1,
	GB_HuC3,
	GB_M161,
	GB_UNL_WISDOM_TREE,
	GB_UNL_PKJD,
	GB_UNL_NT_OLD_1,
	GB_UNL_NT_OLD_2,
	GB_UNL_NT_NEW,
	GB_UNL_BBD,
	GB_UNL_HITEK,
	GB_UNL_GGB81,
	GB_UNL_LI_CHENG,
	GB_UNL_SACHEN_MMC1,
	GB_UNL_SACHEN_MMC2,
	GB_UNL_SINTAX,
};

static QMap<GBModel, QString> s_gbModelNames;
static QMap<GBMemoryBankControllerType, QString> s_mbcNames;

#define tr(STR) QCoreApplication::translate("QGBA::GameBoy", STR)

QList<GBModel> GameBoy::modelList() {
	return s_gbModelList;
}

QString GameBoy::modelName(GBModel model) {
	if (s_gbModelNames.isEmpty()) {
		s_gbModelNames[GB_MODEL_AUTODETECT] = tr("Autodetect");
		s_gbModelNames[GB_MODEL_DMG] = tr("Game Boy (DMG)");
		s_gbModelNames[GB_MODEL_MGB] = tr("Game Boy Pocket (MGB)");
		s_gbModelNames[GB_MODEL_SGB] = tr("Super Game Boy (SGB)");
		s_gbModelNames[GB_MODEL_SGB2] = tr("Super Game Boy 2 (SGB)");
		s_gbModelNames[GB_MODEL_CGB] = tr("Game Boy Color (CGB)");
		s_gbModelNames[GB_MODEL_AGB] = tr("Game Boy Advance (AGB)");
		s_gbModelNames[GB_MODEL_SCGB] = tr("Super Game Boy Color (SGB + CGB)");
	}

	return s_gbModelNames[model];
}

QList<GBMemoryBankControllerType> GameBoy::mbcList() {
	return s_mbcList;
}

QString GameBoy::mbcName(GBMemoryBankControllerType mbc) {
	if (s_mbcNames.isEmpty()) {
		s_mbcNames[GB_MBC_AUTODETECT] = tr("Autodetect");
		s_mbcNames[GB_MBC_NONE] = tr("ROM Only");
		s_mbcNames[GB_MBC1] = "MBC1";
		s_mbcNames[GB_MBC2] = "MBC2";
		s_mbcNames[GB_MBC3] = "MBC3";
		s_mbcNames[GB_MBC3_RTC] = tr("%1 + RTC").arg("MBC3");
		s_mbcNames[GB_MBC5] = "MBC5";
		s_mbcNames[GB_MBC5_RUMBLE] = tr("%1 + Rumble").arg("MBC5");
		s_mbcNames[GB_MBC6] = "MBC6";
		s_mbcNames[GB_MBC7] = tr("%1 (Tilt)").arg("MBC7");
		s_mbcNames[GB_MMM01] = "MMM01";
		s_mbcNames[GB_HuC1] = "HuC-1";
		s_mbcNames[GB_HuC3] = "HuC-3";
		s_mbcNames[GB_POCKETCAM] = "Pocket Cam";
		s_mbcNames[GB_TAMA5] = "TAMA5";
		s_mbcNames[GB_M161] = "M161";
		s_mbcNames[GB_UNL_WISDOM_TREE] = "Wisdom Tree";
		s_mbcNames[GB_UNL_NT_OLD_1] = tr("%1 (old 1)").arg("NT");
		s_mbcNames[GB_UNL_NT_OLD_2] = tr("%1 (old 2)").arg("NT");
		s_mbcNames[GB_UNL_NT_NEW] = tr("%1 (new)").arg("NT");
		s_mbcNames[GB_UNL_PKJD] = "Pokémon Jade/Diamond";
		s_mbcNames[GB_UNL_BBD] = "BBD";
		s_mbcNames[GB_UNL_HITEK] = "Hitek";
		s_mbcNames[GB_UNL_GGB81] = "GGB-81";
		s_mbcNames[GB_UNL_LI_CHENG] = "Li Cheng";
		s_mbcNames[GB_UNL_SACHEN_MMC1] = "Sachen (MMC1)";
		s_mbcNames[GB_UNL_SACHEN_MMC2] = "Sachen (MMC2)";
		s_mbcNames[GB_UNL_SINTAX] = "Sintax";
	}

	return s_mbcNames[mbc];
}
