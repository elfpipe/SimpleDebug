#ifndef PROCESSHANDLER_HPP
#define PROCESSHANDLER_HPP

#include <proto/exec.h>
#include <stdint.h>
#include <string>

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
    struct Process *process;
	struct Hook hook;
	static ExceptionContext context;
	bool exists;
	bool running;
	bool attached;

	static struct MsgPort *port;
	static uint8_t signal;

private:
	static ULONG amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg);

public:
	AmigaDOSProcess() { init(); }
	~AmigaDOSProcess() { cleanup(); }

	bool handleMessages();

    void init();
    void cleanup();

    APTR load(string path, string command, string arguments);
	APTR attach(string name);
	void detach();

	void hookOn();
	void hookOff();

	void readContext ();
	void writeContext ();

	void skip();
	void step();

	static uint32 ip () { return context.ip; }

    void go();
	void wait();
	void wakeUp();
};
#endif