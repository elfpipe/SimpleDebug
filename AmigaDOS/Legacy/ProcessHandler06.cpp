#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string.h>

using namespace std;

template <class Container>
void split2(const std::string& str, Container& cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        cont.push_back(token);
    }
}

string concat(vector<string> strs, int index)
{
	string result;
	for (int i = index; i < strs.size(); i++)
		result.append(strs[i]);
	return result;
}

struct KernelDebugMessage
{
  uint32 type;
  union
  {
    struct ExceptionContext *context;
    struct Library *library;
  } message;
};

class AmigaDOSProcessHandler {
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

private:
    struct Process *child;
	struct Hook hook;
	static ExceptionContext context;
	bool childExists;
	bool childIsRunning;
	bool parentIsAttached;

	static struct MsgPort *port;

private:
	static ULONG amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg);

public:
	AmigaDOSProcessHandler(struct MsgPort *port) { this->port = port; }

    void init();
    void cleanup();

    APTR loadChildProcess(const char *path, const char *command, const char *arguments);
	APTR attachToProcess(const char *name);
	void detachFromChild();

	void hookOn();
	void hookOff();

	void readTaskContext ();
	void writeTaskContext ();
	void setTraceBit ();
	void unsetTraceBit();

	static int memory_insert_break_instruction (uint32 address, uint32 *buffer);
	static int memory_remove_break_instruction (uint32 address, uint32 *buffer);	

	void asmSkip ();
	void asmStep ();

	static uint32 ip () { return context.ip; }

    void go();
    void waitChild();
};

ExceptionContext AmigaDOSProcessHandler::context;
struct MsgPort *AmigaDOSProcessHandler::port;

struct DebugIFace *IDebug = 0;
struct MMUIFace *IMMU = 0;

void AmigaDOSProcessHandler::init ()
{
	IDebug = (struct DebugIFace *)IExec->GetInterface ((struct Library *)SysBase, "debug", 1, 0);
	if (!IDebug) {
		return;
	}

	IMMU = (struct MMUIFace *)IExec->GetInterface ((struct Library *)SysBase, "mmu", 1, 0);
	if (!IMMU) {
		return;
	}
}

void AmigaDOSProcessHandler::cleanup ()
{
	if (IDebug)
		IExec->DropInterface((struct Interface *)IDebug);
	IDebug = 0;

	if (IMMU)
		IExec->DropInterface((struct Interface *)IMMU);
	IMMU = 0;
}

APTR AmigaDOSProcessHandler::loadChildProcess (const char *path, const char *command, const char *arguments)
{
	BPTR lock = IDOS->Lock (path, SHARED_LOCK);
	if (!lock) {
		return 0;
	}
	BPTR homelock = IDOS->DupLock (lock);

	BPTR seglist = IDOS->LoadSeg (command);
	
	if (!seglist) {
		IDOS->UnLock(lock);
		return 0;
	}

	IExec->Forbid(); //can we avoid this?

    child = IDOS->CreateNewProcTags(
		NP_Seglist,					seglist,
//		NP_Entry,					foo,
		NP_FreeSeglist,				TRUE,
		NP_Name,					command,
		NP_CurrentDir,				lock,
		NP_ProgramDir,				homelock,
		NP_StackSize,				2000000,
		NP_Cli,						TRUE,
		NP_Child,					TRUE,
		NP_Arguments,				arguments,
		NP_Input,					IDOS->Input(),
		NP_CloseInput,				FALSE,
		NP_Output,					IDOS->Output(), //pipe_get_write_end(),
		NP_CloseOutput,				FALSE,
		NP_Error,					IDOS->ErrorOutput(),
		NP_CloseError,				FALSE,
		NP_NotifyOnDeathSigTask,	IExec->FindTask(NULL),
		TAG_DONE
	);

	if (!child) {
		IExec->Permit();
		return 0;
	} else {
		IExec->SuspendTask ((struct Task *)child, 0L);		
		childExists = true;

		hookOn();
		readTaskContext();
		IExec->Permit();
	}

	APTR handle;
	
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &handle,
		TAG_DONE
	);
	
    return handle;
}

ULONG AmigaDOSProcessHandler::amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg)
{

    struct ExecIFace *IExec = (struct ExecIFace *)((struct ExecBase *)SysBase)->MainInterface;

	uint32 traptype = 0;

	/* these are the 4 types of debug msgs: */
	switch (dbgmsg->type)
	{
		case DBHMT_REMTASK: {
			//IDOS->Printf("REMTASK\n");

			struct DebugMessage *message = (struct DebugMessage *)IExec->AllocSysObjectTags (ASOT_MESSAGE,
				ASOMSG_Size, sizeof(struct DebugMessage),
				TAG_DONE
			);
			message->type = MSGTYPE_CHILDDIED;
			
			IExec->PutMsg (port, (struct Message *)message);

			break;
		}
		case DBHMT_EXCEPTION: {
			traptype = dbgmsg->message.context->Traptype;

			memcpy (&context, dbgmsg->message.context, sizeof(struct ExceptionContext));
			
			// IDOS->Printf("EXCEPTION\n");
			// IDOS->Printf("[HOOK] ip = 0x%x\n", context.ip);
			// IDOS->Printf("[HOOK} trap = 0x%x\n", context.Traptype);
			
			struct DebugMessage *message = (struct DebugMessage *)IExec->AllocSysObjectTags (ASOT_MESSAGE,
				ASOMSG_Size, sizeof (struct DebugMessage),
				TAG_DONE
			);
			
			if (traptype == 0x700 || traptype == 0xd00)
				message->type = MSGTYPE_TRAP;
			else
				message->type = MSGTYPE_EXCEPTION;
			
			IExec->PutMsg (port, (struct Message *)message);
			
			// returning 1 will suspend the task
			return 1;
		}
		case DBHMT_OPENLIB: {
			//IDOS->Printf("OPENLIB\n");

			struct DebugMessage *message = (struct DebugMessage *)IExec->AllocSysObjectTags (ASOT_MESSAGE,
				ASOMSG_Size, sizeof(struct DebugMessage),
				TAG_DONE
			);
			message->type = MSGTYPE_OPENLIB;
			message->library = dbgmsg->message.library;
				
			IExec->PutMsg(port, (struct Message *)message);
			
		}
		break;

		case DBHMT_CLOSELIB: {
			// IDOS->Printf("CLOSELIB\n");

			struct DebugMessage *message = (struct DebugMessage *)IExec->AllocSysObjectTags(ASOT_MESSAGE,
				ASOMSG_Size, sizeof(struct DebugMessage),
				TAG_DONE
			);
			message->type = MSGTYPE_CLOSELIB;
			message->library = dbgmsg->message.library;
				
			IExec->PutMsg(port, (struct Message *)message);
		}
		break;

		default:
			break;
	}
	return 0;
}

void AmigaDOSProcessHandler::hookOn ()
{	
    hook.h_Entry = (ULONG (*)())amigaos_debug_callback;
    hook.h_Data =  0; //(APTR)&_hookData;

	IDebug->AddDebugHook((struct Task *)child, &hook);
}

void AmigaDOSProcessHandler::hookOff ()
{
	IDebug->AddDebugHook((struct Task*)child, 0);
	child = 0;
}

APTR AmigaDOSProcessHandler::attachToProcess (const char *name)
{
	struct Process *process = (struct Process *)IExec->FindTask(name);
	if(!process) return 0;

	if (process->pr_Task.tc_Node.ln_Type != NT_PROCESS) {
		return 0;
	}

	BPTR seglist = IDOS->GetProcSegList (process, GPSLF_SEG|GPSLF_RUN);
  
	if (!seglist) {
		return 0;
	}

	if (process->pr_Task.tc_State == TS_READY || process->pr_Task.tc_State == TS_WAIT) {
		IExec->SuspendTask ((struct Task *)process, 0);
//		IExec->Signal ((struct Task *)_me, _eventSignalMask);
	}

	if (process->pr_Task.tc_State == TS_CRASHED) {
		process->pr_Task.tc_State = TS_SUSPENDED;
	}

	child  = process;

	childExists = true;
	childIsRunning = false;
	parentIsAttached = true;
    
	hookOn ();

//	readTaskContext ();

	APTR elfHandle;
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &elfHandle,
		TAG_DONE
	);
		
	return elfHandle;
}

void AmigaDOSProcessHandler::detachFromChild ()
{
	hookOff();
	
	childExists = false;
	childIsRunning = false;
	parentIsAttached = false;
}

void AmigaDOSProcessHandler::readTaskContext ()
{
	IDebug->ReadTaskContext  ((struct Task *)child, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

void AmigaDOSProcessHandler::writeTaskContext ()
{
	IDebug->WriteTaskContext ((struct Task *)child, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

#define    MSR_TRACE_ENABLE           0x00000400

void AmigaDOSProcessHandler::setTraceBit ()
{
	struct ExceptionContext ctx;
	IDebug->ReadTaskContext((struct Task *)child, &ctx, RTCF_STATE);
	//this is not supported on the sam cpu:
	ctx.msr |= MSR_TRACE_ENABLE;
	ctx.ip = ip(); //we must reset this because of a system oddity
	IDebug->WriteTaskContext((struct Task *)child, &ctx, RTCF_STATE);
}

void AmigaDOSProcessHandler::unsetTraceBit ()
{
	struct ExceptionContext ctx;
	IDebug->ReadTaskContext ((struct Task *)child, &ctx, RTCF_STATE);
	//this is not supported on the sam cpu:
	ctx.msr &= ~MSR_TRACE_ENABLE;
	ctx.ip = ip();
	IDebug->WriteTaskContext((struct Task *)child, &ctx, RTCF_STATE);
}

// ------------------------------------------------------------------ //

// asmSkip/Step/NoBranch

// ------------------------------------------------------------------ //

void AmigaDOSProcessHandler::asmSkip() {
	context.ip += 4;
	IDebug->WriteTaskContext((struct Task *)child, &context, RTCF_STATE);
}


void AmigaDOSProcessHandler::asmStep()
{
	// we need to write the context because of ??
	//IDebug->WriteTaskContext ((struct Task *)child, &context, RTCF_STATE|RTCF_GENERAL);
	setTraceBit();
	go();
}

////

// Look out - there is a trap !

// /

#define BIG_ENDIAN

asm (
"	.globl meth_start	\n"
"	.globl meth_end		\n"
"meth_start:			\n"
"	trap				\n"
"meth_end:				\n"
);

extern unsigned int meth_start, meth_end;

//

// breakpoint insertion

//

int AmigaDOSProcessHandler::memory_insert_break_instruction (uint32 address, uint32 *buffer)
{
  uint32 oldAttr;
  APTR stack;

  /* Write the breakpoint.  */
  if (1)
  {
  	/* Go supervisor */
	stack = IExec->SuperState ();

  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs ((APTR)address, 0);
	IMMU->SetMemoryAttrs ((APTR)address, 4, MEMATTRF_READ_WRITE);

#if 1
		*buffer = *(uint32 *)address;
		*(uint32 *)address = meth_start;
#else
		uint32 realAddress = (uint32)IMMU->GetPhysicalAddress ((APTR)address);
		if (realAddress == 0x0)
			realAddress = address;
	  
		*buffer = setbreak (realAddress, meth_start);
#endif

	/* Set old attributes again */
	IMMU->SetMemoryAttrs ((APTR)address, 4, oldAttr);
	
	/* Return to old state */
	if (stack)
		IExec->UserState (stack);
  }
  //IExec->CacheClearU();

  return 0;
}

int AmigaDOSProcessHandler::memory_remove_break_instruction (uint32 address, uint32 *buffer)
{
  uint32 oldAttr;
  APTR stack;

  /* Restore the memory contents.  */
  if (1)
  {
  	/* Go supervisor */
	stack = IExec->SuperState ();
	  
  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs ((APTR)address, 0);
	IMMU->SetMemoryAttrs ((APTR)address, 4, MEMATTRF_READ_WRITE);

#if 1
		*(uint32 *)address = *buffer;	//restore old instruction
#else
		uint32 realAddress = (uint32)IMMU->GetPhysicalAddress ((APTR)addr);
		if (realAddress == 0x0)
			realAddress = address;
		setbreak (realAddress, *buffer);
#endif

	/* Set old attributes again */
	IMMU->SetMemoryAttrs ((APTR)address, 4, oldAttr);
	  
	/* Return to old state */
	if (stack)
		IExec->UserState(stack);
  }
  //IExec->CacheClearU();

  return 0;
}

// --------------------------------------------------------------------------- //

void AmigaDOSProcessHandler::go()
{
    IExec->RestartTask((struct Task *)child, 0);
}

void AmigaDOSProcessHandler::waitChild()
{
    IExec->Wait(SIGF_CHILD);
}

// ---------------------------------------------------------------------------- //

uint32 symbolQuery(APTR handle, const char *name)
{
	IElf->OpenElfTags(OET_ElfHandle, handle, TAG_DONE);

	struct Elf32_SymbolQuery query;
    
	query.Flags      = ELF32_SQ_BYNAME; // | ELF32_SQ_LOAD;
	query.Name       = (STRPTR)name;
	query.NameLength = strlen(name);
	query.Value      = 0;

	IElf->SymbolQuery((Elf32_Handle)handle, 1, &query);

	IElf->CloseElf((Elf32_Handle)handle, TAG_DONE);

	return query.Value;
}

// ---------------------------------------------------------------------------- //

bool handleMessages(struct MsgPort *port, AmigaDOSProcessHandler *handler) {
	bool exit = false;
	struct AmigaDOSProcessHandler::DebugMessage *message = (struct AmigaDOSProcessHandler::DebugMessage *)IExec->GetMsg(port);
	while(message) {
		switch(message->type) {
			case AmigaDOSProcessHandler::MSGTYPE_EXCEPTION:
				cout << "==EXCEPTION (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcessHandler::MSGTYPE_TRAP:
				cout << "==TRAP (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcessHandler::MSGTYPE_CRASH:
				cout << "==CRASH (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcessHandler::MSGTYPE_OPENLIB:
				cout << "==OPENLIB\n";
				break;

			case AmigaDOSProcessHandler::MSGTYPE_CLOSELIB:
				cout << "==CLOSELIB\n";
				break;

			case AmigaDOSProcessHandler::MSGTYPE_CHILDDIED:
				cout << "Child has DIED (exit)\n";
				exit = true;
				break;
		}
		message = (struct AmigaDOSProcessHandler::DebugMessage *)IExec->GetMsg(port);
	}
	return exit;
}
// ---------------------------------------------------------------------------- //

vector<string> getInput()
{
	vector<string> cmdArgs;
	
	while(1) {
		cout << "> ";

		string command;
		getline(cin, command);
		split2(command, cmdArgs);

//		if(cmdArgs[0].length() == 1)
			break;
	}

	return cmdArgs;
}

int main(int argc, char *argv[])
{
	struct MsgPort *port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, TAG_DONE);
	AmigaDOSProcessHandler handler(port);
	APTR handle = 0;
	uint32 buffer = 0x0;
	uint32 address = 0x0;

	handler.init();

	bool exit = false;
	while(!exit) {
		vector<string> args = getInput();

		exit = handleMessages(port, &handler);

		if(args.size() > 0) 
		switch(args[0][0]) {
			case 'l': {
				string shellArgs;
				if(args.size() >= 3) {
					shellArgs = concat(args, 2);
				}
				if(args.size() >= 2) {
					handle = handler.loadChildProcess("", args[1].c_str(), shellArgs.c_str());
					if (handle) {
						cout << "Child process loaded\n";
					}
				}
				if(args.size() < 2)
					cout << "Too few arguments\n";
			}
			break;

			case 'a': {
				if(args.size() < 2) {
					cout << "Not enough arguments\n";
				} else {
					handle = handler.attachToProcess(args[1].c_str());
					if(handle)
						cout << "Attached to process\n";
				}
			}
			break;

			case 'd':
				handler.detachFromChild();
				break;

			case 'b':
				if(args.size() < 2) {
					cout << "Too few arguments";
				} else {
					address = symbolQuery(handle, args[1].c_str());
					cout << "BREAK " << (void *)address << "\n";
					if(address)
						handler.memory_insert_break_instruction(address, &buffer);
				}
				break;

			case 'c':
				handler.memory_remove_break_instruction(address, &buffer);
				break;
				
			case 'r':
				handler.readTaskContext();
				break;

			case 'i':
				cout << "ip: " << (void *)handler.ip() << "\n";
				break;

			case 't':
				handler.setTraceBit();
				break;

			case 'u':
				handler.unsetTraceBit();
				break;

			case 's': {
				handler.go();
				IExec->WaitPort(port);
				exit = handleMessages(port, &handler);
				break;
			}
			case 'q':
				exit = true;
				break;

			case 'h':
				cout << "==HELP==\n";
				cout << "l <file> <args>: load child from file\n";
				cout << "a <name>: attach to process in memory\n";
				cout << "d: detach from child\n";
				cout << "s: start execution\n";
				cout << "t: set trace bit\n";
				cout << "u: unset trace bit\n";
				cout << "r: read task context\n";
				cout << "b <symname>: break at symbol\n";
				cout << "c: clear break\n";
				cout << "i: print ip\n";
				cout << "q: quit debugger\n";
				break;

			default:
				break;

		}
	}

    /*if(handle) {
        handler.go();
		handler.waitChild();
    }*/
	handleMessages(port, &handler);

	handler.cleanup();
	IExec->FreeSysObject(ASOT_PORT, port);

    return 0;
}
