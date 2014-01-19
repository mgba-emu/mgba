#include "gba.h"

struct GBASerializedState;

void GBASerialize(struct GBA* gba, struct GBASerializedState* state);
void GBADeserialize(struct GBA* gba, struct GBASerializedState* state);
