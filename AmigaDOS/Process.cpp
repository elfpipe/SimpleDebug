#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

#include "Process.hpp"
#include "Tracer.hpp"

#include <string>
#include <vector>
#include <string.h>

using namespace std;

ExceptionContext AmigaDOSProcess::context;
struct MsgPort *AmigaDOSProcess::port;
uint32_t AmigaDOSProcess::childSignal = 0x0;

struct DebugIFace *IDebug = 0;
struct MMUIFace *IMMU = 0;

void AmigaDOSProcess::init ()
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

void AmigaDOSProcess::cleanup ()
{
	if (IDebug)
		IExec->DropInterface((struct Interface *)IDebug);
	IDebug = 0;

	if (IMMU)
		IExec->DropInterface((struct Interface *)IMMU);
	IMMU = 0;
}

APTR AmigaDOSProcess::loadChildProcess (const char *path, const char *command, const char *arguments)
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

ULONG AmigaDOSProcess::amigaos_debug_callback (struct Hook *hook, struct Task *currentTask, struct KernelDebugMessage *dbgmsg)
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

void AmigaDOSProcess::hookOn ()
{	
    hook.h_Entry = (ULONG (*)())amigaos_debug_callback;
    hook.h_Data =  0; //(APTR)&_hookData;

	IDebug->AddDebugHook((struct Task *)child, &hook);
}

void AmigaDOSProcess::hookOff ()
{
	IDebug->AddDebugHook((struct Task*)child, 0);
	child = 0;
}

APTR AmigaDOSProcess::attachToProcess (const char *name)
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

void AmigaDOSProcess::detachFromChild ()
{
	hookOff();
	
	childExists = false;
	childIsRunning = false;
	parentIsAttached = false;
}

void AmigaDOSProcess::readTaskContext ()
{
	IDebug->ReadTaskContext  ((struct Task *)child, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

void AmigaDOSProcess::writeTaskContext ()
{
	IDebug->WriteTaskContext ((struct Task *)child, &context, RTCF_SPECIAL|RTCF_STATE|RTCF_VECTOR|RTCF_FPU);
}

// ------------------------------------------------------------------ //

// asmSkip/Step/NoBranch

// ------------------------------------------------------------------ //

void AmigaDOSProcess::asmSkip() {
	context.ip += 4;
	IDebug->WriteTaskContext((struct Task *)child, &context, RTCF_STATE);
}

void AmigaDOSProcess::asmStep()
{
	Tracer tracer(child, &context);
	tracer.activate();
	go();
	wait();
	tracer.suspend();
	wakeUp();
}

// --------------------------------------------------------------------------- //

void AmigaDOSProcess::go()
{
    IExec->RestartTask((struct Task *)child, 0);
}

void AmigaDOSProcess::wait()
{
    //IExec->Wait(SIGF_CHILD);
	IExec->Wait(childSignal);
}

void AmigaDOSProcess::wakeUp()
{
	IExec->Signal((struct Task *)child, childSignal);
}
// ---------------------------------------------------------------------------- //

#if 0
uint32 symbolQuery(APTR handle, const char *name)
{
	IElf->OpenElfTags(OET_ElfHandle, handle, TAG_DONE);

	struct Elf32_SymbolQuery query;
    
	query.Flags      = ELF32_SQ_BYNAME; // | ELF32_SQ_LOAD;
	query.Name       = (STRPTR)name;
	query.NameLength = strlen(name);
	query.Value      = 0;

	IElf->SymbolQuery((Elf32_Handle)handle, 1, &query);

//	IElf->CloseElf((Elf32_Handle)handle, TAG_DONE);

	return query.Value;
}
#endif