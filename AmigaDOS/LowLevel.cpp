#include <proto/exec.h>
#include "LowLevel.hpp"

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

ppctype PPC_DisassembleBranchInstr(uint32 instr, int32 *reladdr)
{
	unsigned char *p = (unsigned char *)&instr;
	uint32 in = p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3];
	switch (PPCGETIDX(in))
	{
		case 16:
		{
			int d = (int)(in & 0xfffc);

			if (d >= 0x8000)
				d -= 0x10000;
			*reladdr = d;
			return PPC_BRANCHCOND;
		}
		case 18:
		{
			int d = (int)(in & 0x3fffffc);

			if (d >= 0x2000000)
				d -= 0x4000000;
    		*reladdr = d;
		    return PPC_BRANCH;
		}
		case 19:
			switch (PPCGETIDX2(in))
			{
				case 16:
				{
					//return branch(dp,in,"lr",0,0);  /* bclr */
					int bo = (int)PPCGETD(in);
					if((bo & 4) && (bo & 16))
						return PPC_BRANCHTOLINK;
					else
						return PPC_BRANCHTOLINKCOND;
				}

				case 528:
				{
					//return branch(dp,in,"ctr",0,0);  /* bcctr */
					int bo = (int)PPCGETD(in);
					if((bo & 4) && (bo & 16))
						return PPC_BRANCHTOCTR;
					else
						return PPC_BRANCHTOCTRCOND;
				}	
				default:
					return PPC_OTHER;
			}
			break;
		case 31:
			switch(PPCGETIDX2(in))
			{
				case 20:
					return PPC_LWARX;
				default:
					return PPC_OTHER;
			}
		default:
			return PPC_OTHER;
	}
}