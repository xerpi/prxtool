/***************************************************************
 * PRXTool : Utility for PSP executables.
 * (c) TyRaNiD 2k6
 *
 * disasm.h - Implementation of a MIPS disassembler
 ***************************************************************/
#ifndef __DISASM_H__
#define __DISASM_H__

#include <map>
#include <string>
#include <vector>
#include "prxtypes.h"

enum SymbolType
{
	SYMBOL_NOSYM = 0,
	SYMBOL_UNK,
	SYMBOL_FUNC,
	SYMBOL_LOCAL,
	SYMBOL_DATA,
};

typedef std::vector<unsigned int> RefMap;
typedef std::vector<std::string> AliasMap;

struct SymbolEntry
{
	unsigned int addr;
	SymbolType type;
	unsigned int size;
	std::string name;
	RefMap refs;
	AliasMap alias;
	std::vector<PspLibExport *> exported;
	std::vector<PspLibImport *> imported;
};

typedef std::map<unsigned int, SymbolEntry*> SymbolMap;

struct ImmEntry
{
	unsigned int addr;
	unsigned int target;
	/* Does this entry point to a text section ? */
	int text;
};

typedef std::map<unsigned int, ImmEntry *> ImmMap;

#include <capstone/capstone.h>

struct DisasmEntry
{
	cs_insn *insn;
};

typedef std::map<unsigned int, DisasmEntry *> DisasmMap;

#define DISASM_OPT_MAX       8
#define DISASM_OPT_HEXINTS   'x'
#define DISASM_OPT_MREGS     'r'
#define DISASM_OPT_SYMADDR   's'
#define DISASM_OPT_MACRO     'm'
#define DISASM_OPT_PRINTREAL 'p'
#define DISASM_OPT_PRINTREGS 'g'
#define DISASM_OPT_PRINTSWAP 'w'
#define DISASM_OPT_SIGNEDHEX 'd'

#define INSTR_TYPE_LOCAL 1
#define INSTR_TYPE_FUNC  2

void SetThumbMode(bool mode);

/* Enable hexadecimal integers for immediates */
void disasmSetHexInts(int hexints);
/* Enable mnemonic MIPS registers */
void disasmSetMRegs(int mregs);
/* Enable resolving of PC to a symbol if available */
void disasmSetSymAddr(int symaddr);
/* Enable instruction macros */
void disasmSetMacro(int macro);
void disasmSetPrintReal(int printreal);
void disasmSetOpts(const char *opts, int set);
const char *disasmGetOpts(void);
void disasmPrintOpts(void);
const char *disasmInstruction(unsigned int opcode, unsigned int *PC, unsigned int *realregs, unsigned int *regmask, int nothumb);
const char *disasmInstructionXML(unsigned int opcode, unsigned int PC);

void disasmSetSymbols(SymbolMap *syms);
void disasmAddBranchSymbols(unsigned int opcode, unsigned int *PC, SymbolMap &syms);
SymbolType disasmResolveSymbol(unsigned int PC, char *name, int namelen);
SymbolEntry* disasmFindSymbol(unsigned int PC);
int disasmIsBranch(unsigned int opcode, unsigned int PC, unsigned int *dwTarget);
void disasmSetXmlOutput();
int disasmAddStringRef(unsigned int opcode, unsigned int base, unsigned int size, unsigned int PC, ImmMap &imms, SymbolMap &syms, int data_addr, u32 data_base, u32 data_base_size);
void resetMovwMovt();

void loadDisasm(const uint8_t *code, size_t code_size, uint64_t address);

#endif
