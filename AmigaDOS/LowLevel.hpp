#ifndef LOWLEVEL_HPP
#define LOWLEVEL_HPP

#include <proto/exec.h>
#include <stdint.h>

bool is_readable_address (uint32_t addr);
bool is_writable_address (uint32_t addr);

typedef enum
{
	PPC_BRANCH,
	PPC_BRANCHCOND,
	PPC_BRANCHTOLINK,
	PPC_BRANCHTOLINKCOND,
	PPC_BRANCHTOCTR,
	PPC_BRANCHTOCTRCOND,
	PPC_LWARX,
	PPC_OTHER
} ppctype;

ppctype PPC_DisassembleBranchInstr(uint32 instr, int32 *reladdr);

/* general defines */
#define PPCIDXMASK      0xfc000000
#define PPCIDX2MASK     0x000007fe
#define PPCDMASK        0x03e00000
#define PPCAMASK        0x001f0000
#define PPCBMASK        0x0000f800
#define PPCCMASK        0x000007c0
#define PPCMMASK        0x0000003e
#define PPCCRDMASK      0x03800000
#define PPCCRAMASK      0x001c0000
#define PPCLMASK        0x00600000
#define PPCOE           0x00000400
#define PPCVRC          0x00000400
#define PPCDST          0x02000000
#define PPCSTRM         0x00600000

#define PPCIDXSH        26
#define PPCDSH          21
#define PPCASH          16
#define PPCBSH          11
#define PPCCSH          6
#define PPCMSH          1
#define PPCCRDSH        23
#define PPCCRASH        18
#define PPCLSH          21
#define PPCIDX2SH       1

#define PPCGETIDX(x)    (((x)&PPCIDXMASK)>>PPCIDXSH)
#define PPCGETD(x)      (((x)&PPCDMASK)>>PPCDSH)
#define PPCGETA(x)      (((x)&PPCAMASK)>>PPCASH)
#define PPCGETB(x)      (((x)&PPCBMASK)>>PPCBSH)
#define PPCGETC(x)      (((x)&PPCCMASK)>>PPCCSH)
#define PPCGETM(x)      (((x)&PPCMMASK)>>PPCMSH)
#define PPCGETCRD(x)    (((x)&PPCCRDMASK)>>PPCCRDSH)
#define PPCGETCRA(x)    (((x)&PPCCRAMASK)>>PPCCRASH)
#define PPCGETL(x)      (((x)&PPCLMASK)>>PPCLSH)
#define PPCGETIDX2(x)   (((x)&PPCIDX2MASK)>>PPCIDX2SH)
#define PPCGETSTRM(x)   (((x)&PPCSTRM)>>PPCDSH)

#endif