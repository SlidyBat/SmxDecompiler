#include "cfg-builder.h"

#include "smx-disasm.h"
#include <algorithm>
#include <cassert>

CfgBuilder::CfgBuilder( const SmxFile& smx ) : smx_( &smx )
{}

ControlFlowGraph CfgBuilder::Build( const cell_t* entry )
{
	MarkLeaders( entry );

	for( const cell_t* leader : leaders_ )
	{
		const cell_t* last_instr = leader;
		const cell_t* next_leader = NextInstruction( leader );
		while( next_leader < code_end_ && !IsLeader( next_leader ) )
		{
			last_instr = next_leader;
			next_leader = NextInstruction( last_instr );
		}

		BasicBlock* curr_block = cfg_.FindBlockAt( leader );
		curr_block->SetEnd( next_leader );

		auto op = (SmxOpcode)last_instr[0];

		switch( op )
		{
			case SMX_OP_JUMP:
			{
				cell_t* target = smx_->code( last_instr[1] );
				curr_block->AddTarget( cfg_.FindBlockAt( target ) );
				break;
			}
			case SMX_OP_JEQ:
			case SMX_OP_JNEQ:
			case SMX_OP_JZER:
			case SMX_OP_JNZ:
			case SMX_OP_JSGRTR:
			case SMX_OP_JSGEQ:
			case SMX_OP_JSLESS:
			case SMX_OP_JSLEQ:
			{
				cell_t* target = smx_->code( last_instr[1] );
				curr_block->AddTarget( cfg_.FindBlockAt( target ) );
				curr_block->AddTarget( cfg_.FindBlockAt( next_leader ) );
				break;
			}
			case SMX_OP_SWITCH:
			{
				cell_t* casetbl = smx_->code( last_instr[1] );
				cell_t ncases = casetbl[1];
				cell_t* def = smx_->code( casetbl[2] );
				curr_block->AddTarget( cfg_.FindBlockAt( def ) );
				for( cell_t i = 0; i < ncases; i++ )
				{
					cell_t* target = smx_->code( casetbl[2 + i * 2 + 1] );
					curr_block->AddTarget( cfg_.FindBlockAt( target ) );
				}
				break;
			}
			case SMX_OP_HALT:
			case SMX_OP_RETN:
			{
				// No edges to add
				break;
			}
			default:
			{
				// Fall-through to next block
				if( BasicBlock* bb = cfg_.FindBlockAt( next_leader ) )
				{
					curr_block->AddTarget( bb );
				}
				break;
			}
		}
	}

	cfg_.ComputeOrdering();

	return std::move( cfg_ );
}

const cell_t* CfgBuilder::NextInstruction( const cell_t* instr ) const
{
	return instr + SmxInstrInfo::Get( instr[0] ).num_params + 1;
}

void CfgBuilder::MarkLeaders( const cell_t* entry )
{
	leaders_.clear();

	// For now assume end is at end of section
	code_end_ = smx_->code( smx_->code_size() );

	// Keep track of how many args this function references
	int last_arg_offset = 0;

	// Entry point is always a leader
	AddLeader( entry );
	assert( entry[0] == SMX_OP_PROC );
	const cell_t* instr = NextInstruction( entry );
	while( instr < code_end_ )
	{
		auto op = (SmxOpcode)instr[0];
		const cell_t* params = instr + 1;
		auto& info = SmxInstrInfo::Get( op );

		// Check if any args are referenced
		for( int param = 0; param < info.num_params; param++ )
		{
			if( info.params[param] == SmxParam::STACK )
			{
				cell_t offset = params[param];
				last_arg_offset = std::max( last_arg_offset, offset );
			}
		}

		switch( op )
		{
			case SMX_OP_JUMP:
			{
				cell_t* target = smx_->code( instr[1] );
				AddLeader( target );
				break;
			}
			case SMX_OP_JEQ:
			case SMX_OP_JNEQ:
			case SMX_OP_JZER:
			case SMX_OP_JNZ:
			case SMX_OP_JSGRTR:
			case SMX_OP_JSGEQ:
			case SMX_OP_JSLESS:
			case SMX_OP_JSLEQ:
			{
				cell_t* target = smx_->code( instr[1] );
				AddLeader( target );
				AddLeader( NextInstruction( instr ) );
				break;
			}
			case SMX_OP_SWITCH:
			{
				cell_t* casetbl = smx_->code( instr[1] );
				cell_t ncases = casetbl[1];
				cell_t* def = smx_->code( casetbl[2] );
				AddLeader( def );
				for( cell_t i = 0; i < ncases; i++ )
				{
					cell_t* target = smx_->code( casetbl[2 + i * 2 + 1] );
					AddLeader( target );
				}
				break;
			}
			case SMX_OP_ENDPROC:
			case SMX_OP_PROC:
			{
				// Found end of function, update it
				code_end_ = instr;
				break;
			}
		}

		instr = NextInstruction( instr );
	}

	if( last_arg_offset >= 12 )
	{
		cfg_.SetNumArgs( ( last_arg_offset - 12 ) / 4 + 1 );
	}
}

void CfgBuilder::AddLeader( const cell_t* addr )
{
	if( std::find( leaders_.begin(), leaders_.end(), addr ) != leaders_.end() )
	{
		return;
	}

	leaders_.push_back( addr );
	cfg_.NewBlock( addr );
}

bool CfgBuilder::IsLeader( const cell_t* addr ) const
{
	return std::find( leaders_.begin(), leaders_.end(), addr ) != leaders_.end();
}
