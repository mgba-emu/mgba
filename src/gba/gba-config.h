#ifndef GBA_CONFIG_H
#define GBA_CONFIG_H

#include "util/common.h"

struct Configuration;
struct StartupOptions;
struct GraphicsOpts;

bool GBAConfigLoad(struct Configuration*);

void GBAConfigMapStartupOpts(const struct Configuration*, const char* port, struct StartupOptions*);
void GBAConfigMapGraphicsOpts(const struct Configuration*, const char* port, struct GraphicsOpts*);

#endif
