#include "smx-disasm.h"

#include <sstream>

SmxDisassembler::SmxDisassembler( const SmxFile& smx )
	:
	smx_( &smx )
{}

std::string SmxDisassembler::DisassembleInstr( const cell_t* instr )
{
	const auto& info = SmxInstrInfo::Get( instr[0] );
	std::ostringstream ss;
	cell_t addr = (cell_t)( (intptr_t)instr - (intptr_t)smx_->code() );
	ss << std::hex << addr << '\t' << info.mnem;
	if( info.num_params > 0 )
	{
		ss << " ";
		for( int param = 1; param <= info.num_params; param++ )
		{
			ss << instr[param];
			if( param != info.num_params )
			{
				ss << ", ";
			}
		}
	}

	if( instr[0] == SMX_OP_CASETBL )
	{
		cell_t num_cases = instr[1];
		instr += 3;
		for( cell_t i = 0; i < num_cases; i++ )
		{
			ss << '\n';

			cell_t addr = (cell_t)((intptr_t)instr - (intptr_t)smx_->code());
			ss << std::hex << addr << '\t';
			ss << "case " << instr[0] << ", " << instr[1];
			instr += 2;
		}
	}

	return ss.str();
}

std::string SmxDisassembler::DisassembleFunction( const SmxFunction& func )
{
	std::ostringstream ss;
	const cell_t* instr = smx_->code( func.pcode_start );
	while( instr < smx_->code( func.pcode_end ) )
	{
		const auto& info = SmxInstrInfo::Get( instr[0] );
		ss << DisassembleInstr( instr ) << "\n";
		if( instr[0] == SMX_OP_CASETBL )
			instr += instr[1] * 2;
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
		if( instr[0] == SMX_OP_CASETBL )
			instr += instr[1] * 2;
		instr += 1 + info.num_params;
	}
	return ss.str();
}
