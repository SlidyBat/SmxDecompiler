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

			ILBlock* new_idom = &b.in_edge( 0 );
			for( size_t in = 1; in < b.num_in_edges(); in++ )
			{
				ILBlock& p = b.in_edge( in );
				if( p.idom() != nullptr )
				{
					new_idom = Intersect( p, *new_idom );
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

ILControlFlowGraph* ILControlFlowGraph::Next()
{
	std::vector<std::vector<ILBlock*>> intervals;

	NewEpoch();

	intervals.push_back( IntervalForHeader( blocks_[0] ) );

	bool changed = true;
	while( changed )
	{
		changed = false;

		for( size_t i = 1; i < num_blocks(); i++ )
		{
			ILBlock& m = block( i );
			if( m.IsVisited() )
			{
				continue;
			}

			for( size_t i = 0; i < m.num_in_edges(); i++ )
			{
				ILBlock& p = m.in_edge( i );
				if( p.IsVisited() )
				{
					intervals.push_back( IntervalForHeader( m ) );
					changed = true;
					break;
				}
			}
		}
	}

	ILControlFlowGraph* next = new ILControlFlowGraph;

	// Add intervals to new graph
	for( size_t i = 0; i < intervals.size(); i++ )
	{
		next->AddBlock( i, intervals[i][0]->pc() );
		ILBlock& interval_block = next->block( i );
		for( auto& block : intervals[i] )
		{
			interval_block.Add( new ILInterval( block ) );
		}
	}

	// Calculate the edges
	for( size_t i = 0; i < intervals.size(); i++ )
	{
		ILBlock* outer = &next->block( i );
		for( size_t j = 0; j < intervals[i].size(); j++ )
		{
			ILBlock* inner = intervals[i][j];
			for( size_t edge = 0; edge < inner->num_out_edges(); edge++ )
			{
				ILBlock* target = &inner->out_edge( edge );

				// Find the outer interval that corresponds to this edge
				size_t outer_target_index = FindOuterTarget( intervals, target );
				ILBlock* outer_target = &next->block( outer_target_index );
				if( outer_target != outer )
				{
					outer->AddTarget( *outer_target );
				}
			}
		}
	}

	return next;
}

ILBlock* ILControlFlowGraph::Intersect( ILBlock& b1, ILBlock& b2 )
{
	ILBlock* finger1 = &b1;
	ILBlock* finger2 = &b2;
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

std::vector<ILBlock*> ILControlFlowGraph::IntervalForHeader( ILBlock& header )
{
	std::vector<ILBlock*> I;
	I.push_back( &header );
	header.SetVisited();
	for( size_t i = 1; i < num_blocks(); i++ )
	{
		ILBlock& m = block( i );
		if( m.IsVisited() )
		{
			continue;
		}

		bool add_to_interval = true;
		for( size_t j = 0; j < m.num_in_edges(); j++ )
		{
			ILBlock* p = &m.in_edge( j );
			if( std::find( I.begin(), I.end(), p ) == I.end() )
			{
				add_to_interval = false;
				break;
			}
		}

		if( add_to_interval )
		{
			I.push_back( &m );
			m.SetVisited();
		}
	}

	return I;
}

size_t ILControlFlowGraph::FindOuterTarget( const std::vector<std::vector<ILBlock*>> intervals, ILBlock* target )
{
	for( size_t i = 0; i < intervals.size(); i++ )
	{
		if( std::find( intervals[i].begin(), intervals[i].end(), target ) != intervals[i].end() )
			return i;
	}

	return (size_t)-1;
}

void ILControlFlowGraph::NewEpoch()
{
	epoch_++;
}

void ILBlock::Prepend( ILNode* node )
{
	if( !nodes_.empty() &&
		(dynamic_cast<ILJump*>(nodes_.back()) || dynamic_cast<ILJumpCond*>(nodes_.back()) || dynamic_cast<ILReturn*>(nodes_.back())) )
	{
		nodes_.insert( nodes_.end() - 1, node );
	}
	else
	{
		nodes_.push_back( node );
	}
}

void ILBlock::AddTarget( ILBlock& bb )
{
	bb.in_edges_.push_back( this );
	out_edges_.push_back( &bb );
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
