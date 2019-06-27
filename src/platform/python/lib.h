#include <mgba/flags.h>

struct VFile;

extern bool mPythonLoadScript(const char*, struct VFile*);
extern void mPythonRunPending();
extern bool mPythonLookupSymbol(const char* name, int32_t* out);

#ifdef USE_DEBUGGERS
extern void mPythonSetDebugger(struct mDebugger*);
extern void mPythonDebuggerEntered(enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif
