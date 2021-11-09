#include "il-cfg.h"

#include "il.h"
#include <cassert>

void ILControlFlowGraph::AddBlock( size_t id, cell_t pc )
{
	blocks_.emplace_back( *this, pc );
	blocks_.back().id_ = id;

	stable_blocks_.clear();
	for( ILBlock& bb : blocks_ )
		stable_blocks_.emplace_back( &bb );
}

ILBlock* ILControlFlowGraph::FindBlockAt( cell_t pc )
{
	for( ILBlock* bb : stable_blocks_ )
	{
		if( bb->pc() == pc )
		{
			return bb;
		}
	}
	return nullptr;
}

void ILControlFlowGraph::Remove( ILBlock& bb )
{
	auto it = std::find( stable_blocks_.begin(), stable_blocks_.end(), &bb );
	if( it != stable_blocks_.end() )
	{
		stable_blocks_.erase( it );
		for( size_t i = 0; i < stable_blocks_.size(); i++ )
		{
			stable_blocks_[i]->id_ = i;
		}
	}

	Verify();
}

void ILControlFlowGraph::RemoveMultiple( ILBlock** blocks, size_t num_blocks )
{
	Verify();

	for( size_t block = 0; block < num_blocks; block++ )
	{
		auto it = std::find( stable_blocks_.begin(), stable_blocks_.end(), blocks[block] );
		if( it != stable_blocks_.end() )
		{
			stable_blocks_.erase( it );
		}
	}

	for( size_t i = 0; i < stable_blocks_.size(); i++ )
	{
		stable_blocks_[i]->id_ = i;
	}

	Verify();
}

void ILControlFlowGraph::ComputeDominance()
{
	for( ILBlock* b : stable_blocks_ )
	{
		b->SetImmediateDominator( nullptr );
		b->SetImmediatePostDominator( nullptr );
	}

	// Compute immediate dominators
	block( 0 ).SetImmediateDominator( &block( 0 ) );

	bool changed = true;
	while( changed )
	{
		changed = false;
		for( size_t i = 1; i < num_blocks(); i++ )
		{
			ILBlock& b = block( i );
			assert( b.num_in_edges() );

			ILBlock* new_idom = &b.in_edge( 0 );
			for( size_t in = 1; in < b.num_in_edges(); in++ )
			{
				ILBlock& p = b.in_edge( in );
				if( p.immed_dominator() != nullptr )
				{
					new_idom = Intersect( p, *new_idom );
				}
			}

			if( b.immed_dominator() != new_idom )
			{
				b.SetImmediateDominator( new_idom );
				changed = true;
			}
		}
	}

	// Compute immediate post-dominators
	int last = (int)num_blocks() - 1;
	block( last ).SetImmediatePostDominator( &block( last ) );

	changed = true;
	while( changed )
	{
		changed = false;
		for( int i = last - 1; i >= 0; i-- )
		{
			ILBlock& b = block( i );
			if( !b.num_out_edges() )
			{
				b.SetImmediatePostDominator( &b );
				continue;
			}

			ILBlock* new_idom = &b.out_edge( b.num_out_edges() - 1 );
			for( int out = (int)b.num_out_edges() - 2; out >= 0; out-- )
			{
				ILBlock& p = b.out_edge( out );
				if( p.immed_post_dominator() != nullptr )
				{
					new_idom = IntersectPost( p, *new_idom );
					if( !new_idom )
						break;
				}
			}

			if( new_idom == nullptr )
				new_idom = &b;

			if( b.immed_post_dominator() != new_idom )
			{
				b.SetImmediatePostDominator( new_idom );
				changed = true;
			}
		}
	}

	Verify();
}

ILControlFlowGraph* ILControlFlowGraph::Next()
{
	std::vector<std::vector<ILBlock*>> intervals;

	NewEpoch();

	intervals.push_back( IntervalForHeader( block( 0 ) ) );

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
			finger1 = finger1->immed_dominator();
		}
		while( finger2->id() > finger1->id() )
		{
			finger2 = finger2->immed_dominator();
		}
	}
	return finger1;
}

ILBlock* ILControlFlowGraph::IntersectPost( ILBlock& b1, ILBlock& b2 )
{
	ILBlock* finger1 = &b1;
	ILBlock* finger2 = &b2;
	while( finger1 != finger2 )
	{
		while( finger1->id() < finger2->id() )
		{
			if( finger1 == finger1->immed_post_dominator() )
				return nullptr;
			finger1 = finger1->immed_post_dominator();
			if( !finger1 )
				return finger2;
		}
		while( finger2->id() < finger1->id() )
		{
			if( finger2 == finger2->immed_post_dominator() )
				return nullptr;
			finger2 = finger2->immed_post_dominator();
			if( !finger2 )
				return finger1;
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

void ILControlFlowGraph::Verify()
{
#ifdef _DEBUG
	// Make sure there are no dangling edges from removed blocks
	for( ILBlock* b : stable_blocks_ )
	{
		assert( b == &Entry() || b->num_in_edges() );
		for( size_t i = 0; i < b->num_in_edges(); i++ )
		{
			assert( std::find( stable_blocks_.begin(), stable_blocks_.end(), &b->in_edge( i ) ) != stable_blocks_.end() );
		}
		for( size_t i = 0; i < b->num_out_edges(); i++ )
		{
			assert( std::find( stable_blocks_.begin(), stable_blocks_.end(), &b->out_edge( i ) ) != stable_blocks_.end() );
		}
	}
#endif
}

void ILControlFlowGraph::NewEpoch()
{
	epoch_++;
}

void ILBlock::Remove( ILNode* node )
{
	auto it = std::find( nodes_.begin(), nodes_.end(), node );
	if( it != nodes_.end() )
	{
		nodes_.erase( it );
	}
}

void ILBlock::ReplaceOutEdge( ILBlock& from_block, ILBlock& to_block )
{
	for( size_t i = 0; i < out_edges_.size(); i++ )
	{
		if( out_edges_[i] == &from_block )
		{
			out_edges_[i] = &to_block;
			break;
		}
	}

	if( auto* jmp = dynamic_cast<ILJump*>( Last() ) )
	{
		jmp->ReplaceTarget( &from_block, &to_block );
	}
	else if( auto* jmp_cond = dynamic_cast<ILJumpCond*>( Last() ) )
	{
		jmp_cond->ReplaceTarget( &from_block, &to_block );
	}
}

void ILBlock::ReplaceInEdge( ILBlock& from_block, ILBlock& to_block )
{
	for( size_t i = 0; i < in_edges_.size(); i++ )
	{
		if( in_edges_[i] == &from_block )
		{
			in_edges_[i] = &to_block;
			return;
		}
	}
}

void ILBlock::RemoveOutEdge( ILBlock& block )
{
	auto it = std::find( out_edges_.begin(), out_edges_.end(), &block );
	if( it != out_edges_.end() )
		out_edges_.erase( it );
}

void ILBlock::RemoveInEdge( ILBlock& block )
{
	auto it = std::find( in_edges_.begin(), in_edges_.end(), &block );
	if( it != in_edges_.end() )
		in_edges_.erase( it );
}

void ILBlock::AddOutEdge( ILBlock& block )
{
	out_edges_.push_back( &block );
}

void ILBlock::AddInEdge( ILBlock& block )
{
	in_edges_.push_back( &block );
}

void ILBlock::AddToStart( ILNode* node )
{
	nodes_.insert( nodes_.begin(), node );
}

void ILBlock::AddToEnd( ILNode* node )
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
	for( ILBlock* p = block->immed_dominator(); ; p = p->immed_dominator() )
	{
		if( p == this )
		{
			return true;
		}
		if( p == p->immed_dominator() )
		{
			break;
		}
	}
	return false;
}

size_t ILBlock::NumDominators() const
{
	size_t num = 0;
	for( ILBlock* p = immed_dominator(); ; p = p->immed_dominator() )
	{
		if( p == p->immed_dominator() )
		{
			return num;
		}
		num++;
	}

	return 0;
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
