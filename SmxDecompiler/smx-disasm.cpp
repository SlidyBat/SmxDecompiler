#include "smx-disasm.h"

#include <sstream>

std::string SmxDisassembler::DisassembleInstr( const cell_t* instr )
{
	const auto& info = SmxInstrInfo::Get( instr[0] );
	std::ostringstream ss;
	ss << std::hex << instr << '\t' << info.mnem;
	if( info.num_params > 0 )
	{
		ss << " ";
		for( int param = 1; param <= info.num_params; param++ )
		{
			ss << std::hex << instr[param];
			if( param != info.num_params )
			{
				ss << ", ";
			}
		}
	}
	return ss.str();
}

std::string SmxDisassembler::DisassembleFunction( const SmxFunction& func )
{
	std::ostringstream ss;
	const cell_t* instr = func.pcode_start;
	while( instr < func.pcode_end )
	{
		const auto& info = SmxInstrInfo::Get( instr[0] );
		ss << DisassembleInstr( instr ) << "\n";
		instr += 1 + info.num_params;
	}
	return ss.str();
}

std::string SmxDisassembler::DisassembleBlock( const BasicBlock& bb )
{
	std::ostringstream ss;
	const cell_t* instr = bb.start();
	while( instr < bb.end() )
	{
		const auto& info = SmxInstrInfo::Get( instr[0] );
		ss << DisassembleInstr( instr ) << "\n";
		instr += 1 + info.num_params;
	}
	return ss.str();
}
