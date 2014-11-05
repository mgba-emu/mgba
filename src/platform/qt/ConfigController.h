#ifndef QGBA_CONFIG_CONTROLLER
#define QGBA_CONFIG_CONTROLLER

#include <QObject>

extern "C" {
#include "gba-config.h"
#include "util/configuration.h"
}

struct GBAArguments;

namespace QGBA {
class ConfigController : public QObject {
Q_OBJECT

public:
	constexpr static const char* const PORT = "qt";

	ConfigController(QObject* parent = nullptr);
	~ConfigController();

	const GBAOptions* options() const { return &m_opts; }
	bool parseArguments(GBAArguments* args, int argc, char* argv[]);

public slots:
	void setOption(const char* key, bool value);
	void setOption(const char* key, int value);
	void setOption(const char* key, unsigned value);
	void setOption(const char* key, const char* value);

	void write();

private:
	GBAConfig m_config;
	GBAOptions m_opts;
};

}

#endif
