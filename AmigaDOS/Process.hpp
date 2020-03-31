#ifndef PROCESSHANDLER_HPP
#define PROCESSHANDLER_HPP

#include <proto/exec.h>
#include <stdint.h>

struct KernelDebugMessage
{
  uint32 type;
  union
  {
    struct ExceptionContext *context;
    struct Library *library;
  } message;
};

class AmigaDOSProcess {
public:
	typedef enum {
		MSGTYPE_EXCEPTION,
		MSGTYPE_TRAP,
		MSGTYPE_CRASH,
		MSGTYPE_OPENLIB,
		MSGTYPE_CLOSELIB,
		MSGTYPE_CHILDDIED
	} DebugMessageType;

	struct DebugMessage {
		struct Message msg;
		DebugMessageType type;
		struct Library *library;
	};

	struct HookData {
		struct Task *caller;
		int8_t signal;
		HookData(struct Task *caller, int8_t signal) {
			this->caller = caller;
			this->signal = signal;
		}
	};

private:
    struct Process *child;
	struct Hook hook;
	static ExceptionContext context;
	bool childExists;
	bool childIsRunning;
	bool parentIsAttached;

	static struct MsgPort *port;
	static uint8_t trapSignal;

private:
	static ULONG amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg);

public:
	AmigaDOSProcess(struct MsgPort *port) { this->port = port; init(); }
	~AmigaDOSProcess() { cleanup(); }

    void init();
    void cleanup();

    APTR loadChildProcess(const char *path, const char *command, const char *arguments);
	APTR attachToProcess(const char *name);
	void detachFromChild();

	void hookOn();
	void hookOff();

	void readTaskContext ();
	void writeTaskContext ();

	void asmSkip();
	void asmStep();

	static uint32 ip () { return context.ip; }

    void go();
    void waitPort();
	void waitTrap();
	void wakeUp();
};
#endif