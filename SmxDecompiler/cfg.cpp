#include "cfg.h"

#include "smx-disasm.h"
#include <algorithm>

BasicBlock::BasicBlock( const cell_t* start )
	:
	start_( start ),
	end_( nullptr )
{
	static int bb_id = 0;
	id_ = bb_id++;
}

void BasicBlock::AddTarget( BasicBlock* bb )
{
	bb->in_edges_.push_back( this );
	out_edges_.push_back( bb );
}

void BasicBlock::SetEnd( const cell_t* addr )
{
	end_ = addr;
}

BasicBlock* ControlFlowGraph::NewBlock( const cell_t* start )
{
	blocks_.emplace_back( start );
	return &blocks_.back();
}

BasicBlock* ControlFlowGraph::FindBlockAt( const cell_t* addr )
{
	for( BasicBlock& bb : blocks_ )
	{
		if( bb.start() == addr )
		{
			return &bb;
		}
	}
	return nullptr;
}
