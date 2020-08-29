#include "il-cfg.h"

#include "il.h"
#include <cassert>

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

void ILControlFlowGraph::ComputeDominance()
{
	blocks_[0].SetImmediateDominator( &blocks_[0] );

	bool changed = true;
	while( changed )
	{
		changed = false;
		for( size_t i = 1; i < blocks_.size(); i++ )
		{
			ILBlock& b = blocks_[i];
			assert( b.num_in_edges() );

			ILBlock* new_idom = b.in_edge( 0 );
			for( size_t in = 1; in < b.num_in_edges(); in++ )
			{
				ILBlock* p = b.in_edge( in );
				if( p->idom() != nullptr )
				{
					new_idom = Intersect( p, new_idom );
				}
			}

			if( b.idom() != new_idom )
			{
				b.SetImmediateDominator( new_idom );
				changed = true;
			}
		}
	}
}

ILBlock* ILControlFlowGraph::Intersect( ILBlock* b1, ILBlock* b2 )
{
	ILBlock* finger1 = b1;
	ILBlock* finger2 = b2;
	while( finger1 != finger2 )
	{
		while( finger1->id() > finger2->id() )
		{
			finger1 = finger1->idom();
		}
		while( finger2->id() > finger1->id() )
		{
			finger2 = finger2->idom();
		}
	}
	return finger1;
}

void ILControlFlowGraph::NewEpoch()
{
	epoch_++;
}

void ILBlock::Prepend( ILNode* node )
{
	if( !nodes_.empty() &&
		(dynamic_cast<ILJump*>(nodes_.back()) || dynamic_cast<ILJumpCond*>(nodes_.back()) || dynamic_cast<ILRet*>(nodes_.back())) )
	{
		nodes_.insert( nodes_.end() - 1, node );
	}
	else
	{
		nodes_.push_back( node );
	}
}

void ILBlock::AddTarget( ILBlock* bb )
{
	bb->in_edges_.push_back( this );
	out_edges_.push_back( bb );
}

bool ILBlock::Dominates( ILBlock* block ) const
{
	for( ILBlock* p = block->idom(); ; p = p->idom() )
	{
		if( p == this )
		{
			return true;
		}
		if( p == p->idom() )
		{
			break;
		}
	}
	return false;
}

bool ILBlock::IsBackEdge( size_t out_edge ) const
{
	return out_edges_[out_edge]->id_ < id_;
}

bool ILBlock::IsLoopHeader() const
{
	for( ILBlock* p : in_edges_ )
	{
		if( p->id_ >= id_ )
		{
			return true;
		}
	}
	return false;
}

bool ILBlock::IsVisited() const
{
	return epoch_ == cfg_->epoch();
}

void ILBlock::SetVisited()
{
	epoch_ = cfg_->epoch();
}
