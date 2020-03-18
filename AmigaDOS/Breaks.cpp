//
//
// breakmarkshandler.cpp - handle break markings in running code (Debug 101)
//
#include <proto/dos.h>

#include <proto/exec.h>
#include <string>

#include "Breaks.hpp"

bool Breaks::isBreak(uint32 address)
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

void Breaks::insert(uint32 address)
{
	if (isBreak(address))
		return;

	breaks.push_back(new Break(address));
}

void Breaks::remove(uint32 address)
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

extern unsigned int meth_start, meth_end;

//

// breakpoint insertion

//

int Breaks::memory_insert_break_instruction (uint32 address, uint32 *buffer)
{
	IDOS->Printf("INSERT break: 0x%x\n", address);

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

int Breaks::memory_remove_break_instruction (uint32 address, uint32 *buffer)
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
#endif