#ifndef PROCESSHANDLER_HPP
#define PROCESSHANDLER_HPP

#include <proto/exec.h>
#include <proto/dos.h>
#include <stdint.h>
#include <string>

#include "Pipe.hpp"

using namespace std;

struct KernelDebugMessage
{
  uint32 type;
  union
  {
    struct ExceptionContext *context;
    struct Library *library;
  } message;
};

class AmigaProcess {
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

	Pipe pipe;

private:
	static ULONG amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg);

public:
	AmigaProcess() { init(); }
	~AmigaProcess() { cleanup(); }

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

	uint32_t ip () { readContext(); return context.ip; }
	uint32_t sp () { readContext(); /*return context.gpr[1]; }*/ return (uint32_t)process->pr_Task.tc_SPReg; }
	uint32_t lr () { readContext(); return context.lr; }

    void go();
	void wait();
	void wakeUp();

	vector<string> emptyPipe() { return pipe.emptyPipe(); }
};
#endif