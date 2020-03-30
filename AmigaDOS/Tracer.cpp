#include <proto/exec.h>

#include "Tracer.hpp"
#include "LowLevel.hpp"

#include <iostream>

Tracer::Tracer(Process *process, ExceptionContext *context) {
    this->process = process;
    this->context = context;
}

void Tracer::activate(bool branching) {
    if(hasTraceBit()) {
        setTraceBit();
    } else {
        breaks.insert(context->ip + 4);
        uint32_t baddr = branch();
        if(baddr && branching)
            breaks.insert(baddr);
        breaks.activate();
    }
}

void Tracer::suspend() {
    if(hasTraceBit()) {
        unsetTraceBit();
    } else {
        breaks.suspend();
        breaks.clear();
    }
}

uint32_t Tracer::branch()
{
	int32 offset;
	switch(PPC_DisassembleBranchInstr(*(uint32 *)context->ip, &offset))
	{
		case PPC_OTHER:
			return 0x0;
		case PPC_BRANCHTOLINK:
		case PPC_BRANCHTOLINKCOND:
			return context->lr;
		case PPC_BRANCHTOCTR:
		case PPC_BRANCHTOCTRCOND:
			return context->ctr;
		case PPC_BRANCH:
		case PPC_BRANCHCOND:
			return context->ip + offset;
	}
	return 0x0;
}

#define    MSR_TRACE_ENABLE           0x00000400

void Tracer::setTraceBit ()
{
	struct ExceptionContext ctx;
//    cout << "ip: 0x" << (void *)context->ip << "\n";
	IDebug->ReadTaskContext((struct Task *)process, &ctx, RTCF_STATE);
    IExec->CacheClearE((APTR)&ctx, sizeof(ctx), CACRF_ClearI| CACRF_ClearD|CACRF_InvalidateD);

    //IDOS->Delay(5);
	//this is not supported on the sam cpu:
	ctx.msr |= MSR_TRACE_ENABLE;
	ctx.ip = context->ip; //we must reset this because of a system oddity
//    cout << "ip: 0x" << (void *)ctx.ip << "\n";
	IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
    IExec->CacheClearE((APTR)&ctx, sizeof(ctx), CACRF_ClearI| CACRF_ClearD|CACRF_InvalidateD);
//    IDOS->Delay(5);
}

void Tracer::unsetTraceBit ()
{
	struct ExceptionContext ctx;
	IDebug->ReadTaskContext ((struct Task *)process, &ctx, RTCF_STATE);
    IExec->CacheClearE((APTR)&ctx, sizeof(ctx), CACRF_ClearI| CACRF_ClearD|CACRF_InvalidateD);
//    IDOS->Delay(5);
	//this is not supported on the sam cpu:
	ctx.msr &= ~MSR_TRACE_ENABLE;
	ctx.ip = context->ip;
//    cout << "ip: 0x" << (void *)ctx.ip << "\n";
	IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
    IExec->CacheClearE((APTR)&ctx, sizeof(ctx), CACRF_ClearI| CACRF_ClearD|CACRF_InvalidateD);
//    IDOS->Delay(5);
}

bool Tracer::hasTraceBit()
{
	uint32 family;
	IExec->GetCPUInfoTags(GCIT_Family, &family, TAG_DONE);
	if (family == CPUFAMILY_4XX)
		return false;
	return true;
}