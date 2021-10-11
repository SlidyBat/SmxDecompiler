#include "cfg.h"

#include "smx-disasm.h"
#include <algorithm>
#include <cassert>

BasicBlock::BasicBlock( const ControlFlowGraph& cfg, const cell_t* start )
	:
	start_( start ),
	end_( nullptr ),
	cfg_( &cfg )
{}

void BasicBlock::AddTarget( BasicBlock* bb )
{
	bb->in_edges_.push_back( this );
	out_edges_.push_back( bb );
}

void BasicBlock::SetEnd( const cell_t* addr )
{
	end_ = addr;
}

bool BasicBlock::Contains( const cell_t* addr ) const
{
	if( end_ == nullptr )
	{
		// We don't know the range of this block yet, just check if it matches the start
		return addr == start_;
	}
	return addr >= start_ && addr < end_;
}

bool BasicBlock::IsBackEdge( size_t index ) const
{
	return out_edges_[index]->id_ < id_;
}

bool BasicBlock::IsVisited() const
{
	return epoch_ == cfg_->epoch();
}

void BasicBlock::SetVisited()
{
	epoch_ = cfg_->epoch();
}

BasicBlock* ControlFlowGraph::NewBlock( const cell_t* start )
{
	blocks_.emplace_back( *this, start );
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

void ControlFlowGraph::Remove( size_t block_index )
{
	blocks_.erase( blocks_.begin() + block_index );
}

void ControlFlowGraph::ComputeOrdering()
{
	// Prune blocks with no input edges (other than the entry node)
	// This happens with casetbl instruction, which is never meant to actually be executed
	ordered_blocks_.reserve( blocks_.size() );
	for( BasicBlock& bb : blocks_ )
	{
		if( &bb != &EntryBlock() && bb.num_in_edges() == 0 )
		{
			continue;
		}
		
		ordered_blocks_.push_back( &bb );
	}

	NewEpoch();
	VisitPostOrderAndSetId( EntryBlock(), 1 );
	std::sort( ordered_blocks_.begin(), ordered_blocks_.end(), []( const BasicBlock* a, const BasicBlock* b ) {
		return a->id() < b->id();
	} );
}

size_t ControlFlowGraph::VisitPostOrderAndSetId( BasicBlock& bb, size_t po_number )
{
	bb.SetVisited();
	for( size_t out = 0; out < bb.num_out_edges(); out++ )
	{
		BasicBlock& successor = *bb.out_edge( out );
		if( successor.IsVisited() )
		{
			continue;
		}
		po_number = VisitPostOrderAndSetId( successor, po_number );
	}
	
	bb.id_ = num_blocks() - po_number; // Set ID to RPO index
	return po_number + 1;
}
