#include "flags.h"

struct VFile;

extern bool mPythonLoadScript(const char*, struct VFile*);
extern void mPythonRunPending();

#ifdef USE_DEBUGGERS
extern void mPythonSetDebugger(struct mDebugger*);
extern void mPythonDebuggerEntered(enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif
