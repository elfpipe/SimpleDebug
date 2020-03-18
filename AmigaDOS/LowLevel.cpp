#include <proto/exec.h>

bool is_readable_address (uint32 addr)
{
    uint32 attr, masked;
    APTR stack;
    BOOL ret = FALSE;

      /* Go supervisor */
    stack = IExec->SuperState();
    
	attr = IMMU->GetMemoryAttrs((APTR)addr, 0);

    /* Return to old state */
    if (stack)
        IExec->UserState(stack);

    masked = attr & MEMATTRF_RW_MASK;
    if(masked)
        ret = TRUE;

    return ret;
}

BOOL is_writable_address (uint32 addr)
{
    uint32 attr, oldattr, masked;
    APTR stack;
    BOOL ret = FALSE;

      /* Go supervisor */
    stack = IExec->SuperState();
    
    oldattr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, MEMATTRF_READ_WRITE);
	attr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, oldattr);

    /* Return to old state */
    if (stack)
        IExec->UserState(stack);

    masked = attr & MEMATTRF_RW_MASK;
    if(masked == MEMATTRF_READ_WRITE)
        ret = TRUE;

    return ret;
}