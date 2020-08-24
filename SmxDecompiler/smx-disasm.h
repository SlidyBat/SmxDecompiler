#pragma once

#include "smx-file.h"
#include "smx-opcodes.h"
#include <string>

enum class SmxParam
{
	CONSTANT,
	STACK,
	JUMP,
	FUNCTION,
	NATIVE,
	ADDRESS
};

struct SmxInstr
{
	const char* mnem;
	int num_params;
	SmxParam params[5];
};

class SmxDisassembler
{
public:
	static const SmxInstr& InstrInfo( SmxOpcode op );
	static const SmxInstr& InstrInfo( cell_t op );
	static std::string DisassembleInstr( const cell_t* instr );
	static std::string DisassembleFunction( const SmxFunction& func );
};