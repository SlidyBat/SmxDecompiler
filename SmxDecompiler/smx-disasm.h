#pragma once

#include "smx-file.h"
#include "smx-opcodes.h"
#include "cfg.h"
#include <string>

class SmxDisassembler
{
public:
	static std::string DisassembleInstr( const cell_t* instr );
	static std::string DisassembleFunction( const SmxFunction& func );
	static std::string DisassembleBlock( const BasicBlock& bb );
};