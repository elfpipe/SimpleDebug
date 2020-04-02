#include "Stacktracer.hpp"
#include "Strings.hpp"

vector<string> Stacktracer::trace;

int32 Stacktracer::stacktrace_callback(struct Hook *hook, struct Task *task, struct StackFrameMsg *frame) {
	string entry;
	switch (frame->State) {
		case STACK_FRAME_DECODED: {
			struct DebugSymbol *symbol = IDebug->ObtainDebugSymbol(frame->MemoryAddress, NULL);

			if(symbol) {
				if(symbol->Type == DEBUG_SYMBOL_MODULE_STABS && symbol->SourceFileName)
					entry = printStringFormat("[%s: line %d]: %s", symbol->SourceFileName, symbol->SourceLineNumber, symbol->SourceFunctionName);
				else if(symbol->SourceFunctionName)
					entry = printStringFormat("%s", symbol->SourceFunctionName);
				else
					entry = printStringFormat("[%s]", symbol->Name);
			
				IDebug->ReleaseDebugSymbol(symbol);
			}
		}
		break;

		case STACK_FRAME_INVALID_BACKCHAIN_PTR:
			entry = printStringFormat("(%p) invalid backchain pointer", frame->StackPointer);
			break;

		case STACK_FRAME_TRASHED_MEMORY_LOOP:
			entry = printStringFormat("(%p) trashed memory loop", frame->StackPointer);
			break;

		case STACK_FRAME_BACKCHAIN_PTR_LOOP:
			entry = printStringFormat("(%p) backchain pointer loop", frame->StackPointer);
			break;

		default:
			entry = printStringFormat("Unknown state=%lu", frame->State);
			break;
	}
	trace.push_back(entry);
	return 0;  // Continue tracing.
}

vector<string> Stacktracer::stacktrace(Task *task, uint32_t sp) {
	Hook *hook = (Hook *)IExec->AllocSysObjectTags(
		ASOT_HOOK,
		ASOHOOK_Entry, stacktrace_callback,
		TAG_END);

	trace.clear();

	if (hook) {
		IDebug->StackTrace(task, hook);
		IExec->FreeSysObject(ASOT_HOOK, hook);
	}
	return trace;
}