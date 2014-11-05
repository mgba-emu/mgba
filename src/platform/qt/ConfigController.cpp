#include "ConfigController.h"

#include "GameController.h"

extern "C" {
#include "platform/commandline.h"
}

using namespace QGBA;

ConfigController::ConfigController(QObject* parent)
	: QObject(parent)
	, m_opts()
{
	GBAConfigInit(&m_config, PORT);

	m_opts.audioSync = GameController::AUDIO_SYNC;
	m_opts.videoSync = GameController::VIDEO_SYNC;
	GBAConfigLoadDefaults(&m_config, &m_opts);
	GBAConfigLoad(&m_config);
	GBAConfigMap(&m_config, &m_opts);
}

ConfigController::~ConfigController() {
	write();

	GBAConfigDeinit(&m_config);
	GBAConfigFreeOpts(&m_opts);
}

bool ConfigController::parseArguments(GBAArguments* args, int argc, char* argv[]) {
	return ::parseArguments(args, &m_config, argc, argv, 0);
}

void ConfigController::setOption(const char* key, bool value) {
	setOption(key, (int) value);
}

void ConfigController::setOption(const char* key, int value) {
	GBAConfigSetIntValue(&m_config, key, value);
}

void ConfigController::setOption(const char* key, unsigned value) {
	GBAConfigSetUIntValue(&m_config, key, value);
}

void ConfigController::setOption(const char* key, const char* value) {
	GBAConfigSetValue(&m_config, key, value);
}

void ConfigController::write() {
	GBAConfigSave(&m_config);
}
