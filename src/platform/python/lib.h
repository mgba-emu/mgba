struct mDebugger;
struct VFile;

extern void mPythonSetDebugger(struct mDebugger*);
extern bool mPythonLoadScript(const char*, struct VFile*);
extern void mPythonRunPending();
