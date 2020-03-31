#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

#include "Process.hpp"
#include "Tracer.hpp"

#include <string>
#include <vector>
#include <string.h>
#include <iostream>

using namespace std;

ExceptionContext AmigaDOSProcess::context;
struct MsgPort *AmigaDOSProcess::port = 0;
uint8_t AmigaDOSProcess::signal = 0x0;

struct DebugIFace *IDebug = 0;
struct MMUIFace *IMMU = 0;

void AmigaDOSProcess::init()
{
	IDebug = (struct DebugIFace *)IExec->GetInterface ((struct Library *)SysBase, "debug", 1, 0);
	if (!IDebug) {
		return;
	}

	IMMU = (struct MMUIFace *)IExec->GetInterface ((struct Library *)SysBase, "mmu", 1, 0);
	if (!IMMU) {
		return;
	}
	if(!port)
		port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, TAG_DONE);
	signal = IExec->AllocSignal(-1);
}

void AmigaDOSProcess::cleanup ()
{
	if (IDebug)
		IExec->DropInterface((struct Interface *)IDebug);
	IDebug = 0;

	if (IMMU)
		IExec->DropInterface((struct Interface *)IMMU);
	IMMU = 0;

	IExec->FreeSignal(signal);
}

APTR AmigaDOSProcess::load(string path, string command, string arguments)
{
	BPTR lock = IDOS->Lock(path.c_str(), SHARED_LOCK);
	if (!lock) {
		return 0;
	}
	BPTR homelock = IDOS->DupLock (lock);

	BPTR seglist = IDOS->LoadSeg (command.c_str());
	
	if (!seglist) {
		IDOS->UnLock(lock);
		return 0;
	}

	IExec->Forbid(); //can we avoid this?

    process = IDOS->CreateNewProcTags(
		NP_Seglist,					seglist,
//		NP_Entry,					foo,
		NP_FreeSeglist,				TRUE,
		NP_Name,					strdup(command.c_str()),
		NP_CurrentDir,				lock,
		NP_ProgramDir,				homelock,
		NP_StackSize,				2000000,
		NP_Cli,						TRUE,
		NP_Child,					TRUE,
		NP_Arguments,				arguments.c_str(),
		NP_Input,					IDOS->Input(),
		NP_CloseInput,				FALSE,
		NP_Output,					IDOS->Output(), //pipe_get_write_end(),
		NP_CloseOutput,				FALSE,
		NP_Error,					IDOS->ErrorOutput(),
		NP_CloseError,				FALSE,
		NP_NotifyOnDeathSigTask,	IExec->FindTask(0),
		TAG_DONE
	);

	if (!process) {
		IExec->Permit();
		return 0;
	} else {
		IExec->SuspendTask ((struct Task *)process, 0L);		
		exists = true;

		hookOn();
		readContext();
		IExec->Permit();
	}

	APTR handle;
	
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &handle,
		TAG_DONE
	);
	
    return handle;
}

ULONG AmigaDOSProcess::amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg)
{

    struct ExecIFace *IExec = (struct ExecIFace *)((struct ExecBase *)SysBase)->MainInterface;
	uint32 traptype = 0;

	HookData *data = (HookData *)hook->h_Data;
	bool sendSignal = false;

	ULONG ret = 0;

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
			sendSignal = true;  //if process has ended, we must signal caller

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
			
			if (traptype == 0x700 || traptype == 0xd00) {
				message->type = MSGTYPE_TRAP;
				sendSignal = true; //it's a trap
			} else {
				message->type = MSGTYPE_EXCEPTION;
			}

			IExec->PutMsg (port, (struct Message *)message);

			sendSignal = true;

			// returning 1 will suspend the task
			ret = 1;
			break;
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

	if(sendSignal) IExec->Signal(data->caller, 1 << data->signal);

	return ret;
}

void AmigaDOSProcess::hookOn()
{
	struct HookData *data = new HookData(IExec->FindTask(0), trapSignal);

    hook.h_Entry = (ULONG (*)())amigaos_debug_callback;
    hook.h_Data =  (APTR)data;

	IDebug->AddDebugHook((struct Task *)process, &hook);
}

void AmigaDOSProcess::hookOff()
{
	IDebug->AddDebugHook((struct Task*)process, 0);
}

bool AmigaDOSProcess::handleMessages() {
	bool exit = false;
	DebugMessage *message = (DebugMessage *)IExec->GetMsg(port);
	while(message) {
		switch(message->type) {
			case AmigaDOSProcess::MSGTYPE_EXCEPTION:
				cout << "==EXCEPTION (ip = 0x" << (void *)ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_TRAP:
				cout << "==TRAP (ip = 0x" << (void *)ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CRASH:
				cout << "==CRASH (ip = 0x" << (void *)ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_OPENLIB:
				cout << "==OPENLIB\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CLOSELIB:
				cout << "==CLOSELIB\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CHILDDIED:
				cout << "Child has DIED (exit)\n";
				exit = true;
				break;
		}
		message = (struct AmigaDOSProcess::DebugMessage *)IExec->GetMsg(port);
	}
	return exit;
}

APTR AmigaDOSProcess::attach(string name)
{
	struct Process *_process = (struct Process *)IExec->FindTask(name.c_str());
	if(!_process) return 0;

	process = _process;
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

	exists = true;
	running = false;
	attached = true;
    
	hookOn ();

//	readTaskContext ();

	APTR handle;
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &handle,
		TAG_DONE
	);
		
	return handle;
}

void AmigaDOSProcess::detach()
{
	hookOff();
	
	exists = false;
	running = false;
	attached = false;
}

void AmigaDOSProcess::readContext ()
{
	IDebug->ReadTaskContext  ((struct Task *)process, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

void AmigaDOSProcess::writeContext ()
{
	IDebug->WriteTaskContext ((struct Task *)process, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

// ------------------------------------------------------------------ //

void AmigaDOSProcess::skip() {
	context.ip += 4;
	IDebug->WriteTaskContext((struct Task *)process, &context, RTCF_STATE);
}

void AmigaDOSProcess::step()
{
	Tracer tracer(process, &context);
	tracer.activate();
	go();
	wait();
	tracer.suspend();
}

// --------------------------------------------------------------------------- //

void AmigaDOSProcess::go()
{
    IExec->RestartTask((struct Task *)process, 0);
}

void AmigaDOSProcess::wait()
{
	IExec->Wait(1 << signal);
}

void AmigaDOSProcess::wakeUp()
{
	IExec->Signal((struct Task *)IExec->FindTask(0), signal);
}