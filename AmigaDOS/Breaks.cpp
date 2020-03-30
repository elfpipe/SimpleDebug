//
//
// breakmarkshandler.cpp - handle break markings in running code (Debug 101)
//
#include <proto/exec.h>
#include <proto/dos.h>
#include <string>

#include "Breaks.hpp"

bool Breaks::isBreak(uint32_t address)
{
	for (list <Break *>::iterator it = breaks.begin (); it != breaks.end (); it++)
		if (address == (*it)->address)
			return true;
	return false;
}

void Breaks::activate()
{
	for (list <Break *>::iterator it = breaks.begin(); it != breaks.end(); it++)
		memory_insert_break_instruction((*it)->address, &(*it)->buffer);
	
	activated = true;
}

void Breaks::suspend()
{
	for (list<Break *>::iterator it = breaks.begin(); it != breaks.end(); it++)
		memory_remove_break_instruction((*it)->address, &(*it)->buffer);
	
	activated = false;
}

void Breaks::insert(uint32_t address)
{
	if (isBreak(address))
		return;

	breaks.push_back(new Break(address));
}

void Breaks::remove(uint32_t address)
{
	for (list<Break *>::iterator it = breaks.begin(); it != breaks.end(); it++)
		if (address == (*it)->address) {
			delete *it;
			it = breaks.erase(it);
		}
}

void Breaks::clear()
{
	for (list<Break *>::iterator it = breaks.begin(); it != breaks.end(); it++) {
		delete *it;
		it = breaks.erase(it);
	}
}

// ------------------------------------------------------------------- //

#ifdef __amigaos4__
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

asm (
"	.globl setbreak		\n"
"	.set MSR_DR,0x0010	\n"
"	.set MSR_EE,0x8000	\n"
"setbreak:				\n"

"	mr    %r5, %r3		\n"

"	mfmsr %r11			\n"
"	ori   %r12, %r11, MSR_DR|MSR_EE	\n" //         # Turn off address  translation
"	xori  %r12, %r12, MSR_DR|MSR_EE	\n" //         # and interrupts
"	sync				\n"
"	mtmsr %r12			\n"
"	isync				\n"

//# do modification here, leave r11 intact
"	lwz   %r3, 0(%r5)		\n"
"	stw   %r4, 0(%r5)		\n"

"	sync           		\n"             //          # make sure store is complete
"	mtmsr         %r11	\n"
"	isync				\n"

"	blr					\n"
);

extern unsigned int meth_start, meth_end;
extern "C" uint32_t setbreak(uint32_t, uint32_t); //uint32_t, uint32_t);
//

// breakpoint insertion

//

int Breaks::memory_insert_break_instruction (uint32_t address, uint32_t *buffer)
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

#if 0
		*buffer = *(uint32 *)address;
		*(uint32 *)address = meth_start;
#else
		uint32_t realAddress = (uint32_t)IMMU->GetPhysicalAddress ((APTR)address);
		if (realAddress == 0x0)
			realAddress = address;

		int hallo = meth_start;
		*buffer = setbreak (realAddress, meth_start);
#endif

	/* Set old attributes again */
	IMMU->SetMemoryAttrs ((APTR)address, 4, oldAttr);

	/* Return to old state */
	if (stack)
		IExec->UserState (stack);
  }
IExec->CacheClearE((APTR)address, 0xffffffff, CACRF_ClearI| CACRF_ClearD);
//IDOS->Delay(50);
  return 0;
}

int Breaks::memory_remove_break_instruction (uint32_t address, uint32_t *buffer)
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

#if 0
		*(uint32 *)address = *buffer;	//restore old instruction
#else
		uint32_t realAddress = (uint32_t)IMMU->GetPhysicalAddress ((APTR)address);
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
IExec->CacheClearE((APTR)address, 0xffffffff, CACRF_ClearI| CACRF_ClearD);
//IDOS->Delay(50);
  return 0;
}
#endif