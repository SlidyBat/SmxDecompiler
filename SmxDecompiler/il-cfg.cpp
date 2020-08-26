#include "il-cfg.h"

void ILControlFlowGraph::AddBlock( size_t id, cell_t pc )
{
	blocks_.emplace_back( *this, pc );
	blocks_.back().id_ = id;
}

ILBlock* ILControlFlowGraph::FindBlockAt( cell_t pc )
{
	for( ILBlock& bb : blocks_ )
	{
		if( bb.pc() == pc )
		{
			return &bb;
		}
	}
	return nullptr;
}

void ILControlFlowGraph::NewEpoch()
{
	epoch_++;
}

void ILBlock::AddTarget( ILBlock* bb )
{
	bb->in_edges_.push_back( this );
	out_edges_.push_back( bb );
}

bool ILBlock::IsVisited() const
{
	return epoch_ == cfg_->epoch();
}

void ILBlock::SetVisited()
{
	epoch_ = cfg_->epoch();
}
