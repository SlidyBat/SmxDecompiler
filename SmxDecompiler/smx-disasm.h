#pragma once

#include "smx-file.h"
#include "smx-opcodes.h"
#include "cfg.h"
#include <string>

class SmxDisassembler
{
public:
	SmxDisassembler( const SmxFile& smx );

	std::string DisassembleInstr( const cell_t* instr );
	std::string DisassembleFunction( const SmxFunction& func );
	std::string DisassembleBlock( const BasicBlock& bb );
private:
	const SmxFile* smx_;
};