#include "structurizer.h"

#include "il.h"

Structurizer::Structurizer( ILControlFlowGraph* cfg )
{
	derived_.push_back( cfg );
	ILControlFlowGraph* curr = derived_.back();
	ILControlFlowGraph* next = curr->Next();
	while( curr->num_blocks() != next->num_blocks() )
	{
		derived_.push_back( next );
		curr = derived_.back();
		next = curr->Next();
	}

	loop_heads_.resize( cfg->num_blocks(), nullptr );
	loop_latch_.resize( cfg->num_blocks(), nullptr );
	statements_.resize( cfg->num_blocks(), nullptr );
}

Statement* Structurizer::Transform()
{
	MarkLoops();

	return CreateStatement( &cfg()->block( 0 ) );
}

void Structurizer::FindBlocksInInterval( ILBlock* interval, size_t level, std::vector<ILBlock*>& blocks )
{
	if( level > 0 )
	{
		for( size_t i = 0; i < interval->num_nodes(); i++ )
		{
			ILInterval* I = static_cast<ILInterval*>( interval->node( i ) );
			FindBlocksInInterval( I->block(), level - 1, blocks );
		}
	}
	else
	{
		blocks.push_back( interval );
	}
}

void Structurizer::FindBlocksInLoop( ILBlock* head, ILBlock* latch, const std::vector<ILBlock*>& interval )
{
	LoopHead( head ) = head;
	for( size_t i = head->id() + 1; i < latch->id(); i++ )
	{
		ILBlock* bb = &derived_[0]->block( i );
		ILBlock* immed_dominator = bb->immed_dominator();
		if( LoopHead( immed_dominator ) == head &&
			LoopHead( bb ) == nullptr &&
			std::find( interval.begin(), interval.end(), bb ) != interval.end() )
		{
			LoopHead( bb ) = head;
		}
	}
	LoopHead( latch ) = head;

	LoopLatch( head ) = latch;
}

void Structurizer::MarkLoops()
{
	std::vector<ILBlock*> blocks;

	for( size_t level = 1; level < derived_.size(); level++ )
	{
		ILControlFlowGraph* G = derived_[level];

		for( size_t Ii = 0; Ii < G->num_blocks(); Ii++ )
		{
			ILBlock* latch = nullptr;
			blocks.clear();

			// Get interval basic blocks
			FindBlocksInInterval( &G->block( Ii ), level, blocks );

			ILBlock* head = blocks[0];

			// Find greatest back edge in current interval
			for( size_t i = 0; i < head->num_in_edges(); i++ )
			{
				ILBlock* pred = &head->in_edge( i );

				// Must be a back edge
				if( pred->id() <= head->id() )
					continue;
				
				// Must be in current interval
				if( std::find( blocks.begin(), blocks.end(), pred ) == blocks.end() )
					continue;

				if( !latch || ( pred->id() > latch->id() ) )
				{
					latch = pred;
				}
			}

			if( latch && LoopHead( latch ) == nullptr )
			{
				FindBlocksInLoop( head, latch, blocks );
			}
		}
	}
}

Statement*& Structurizer::StatementForBlock( ILBlock* bb )
{
	return statements_[bb->id()];
}

Statement* Structurizer::CreateStatement( ILBlock* bb )
{
	if( LoopHead( bb ) == bb )
	{
		return CreateLoopStatement( bb );
	}
	return CreateNonLoopStatement( bb );
}

Statement* Structurizer::CreateLoopStatement( ILBlock* bb )
{
	ILBlock* head = bb;
	ILBlock* latch = LoopLatch( head );

	if( head->num_out_edges() == 2 && latch->num_out_edges() == 1 )
	{
		// While statement
		ILBlock* succ1 = &head->out_edge( 0 );
		ILBlock* succ2 = &head->out_edge( 1 );
		
		// Determine which is the body and which is the follow
		ILBlock* body;
		ILBlock* follow;
		if( LoopHead( succ1 ) == head )
		{
			body = succ1;
			follow = succ2;
		}
		else
		{
			body = succ2;
			follow = succ1;
		}

		ILJumpCond* jmp = static_cast<ILJumpCond*>( head->Last() );
		if( jmp->true_branch() != body )
			jmp->Invert();

		std::vector<Statement*> statements;
		statements.push_back( new WhileStatement( jmp->condition(), CreateStatement( body ) ) );
		statements.push_back( CreateStatement( follow ) );
		return new SequenceStatement( std::move( statements ) );
	}

	return nullptr;
}

Statement* Structurizer::CreateNonLoopStatement( ILBlock* bb )
{
	if( bb->num_out_edges() > 0 )
	{
		if( auto* switch_node = dynamic_cast<ILSwitch*>(bb->Last()) )
		{
			ILNode* value = switch_node->value();
			Statement* default_case = CreateStatement( switch_node->default_case() );
			std::vector<CaseStatement> cases;
			cases.reserve( switch_node->num_cases() );
			for( size_t i = 0; i < switch_node->num_cases(); i++ )
			{
				CaseStatement case_entry;
				case_entry.body = CreateStatement( switch_node->case_entry( i ).address );
				case_entry.value = switch_node->case_entry( i ).value;
			}

			return new SwitchStatement( value, default_case, std::move( cases ) );
		}
		else if( bb->num_out_edges() == 1 )
		{
			// If this is a back edge then 
			if( bb->IsBackEdge( 0 ) )
			{
				return new BasicStatement( bb );
			}
			else
			{
				std::vector<Statement*> statements;
				statements.push_back( new BasicStatement( bb ) );
				statements.push_back( CreateStatement( &bb->out_edge( 0 ) ) );
				return new SequenceStatement( std::move( statements ) );
			}
		}
		else
		{
			assert( bb->num_out_edges() == 2 );
			auto* jmp = static_cast<ILJumpCond*>( bb->Last() );
			jmp->Invert();
			return new IfStatement( jmp->condition(),
				CreateStatement( jmp->true_branch() ),
				CreateStatement( jmp->false_branch() ) );

		}
	}
	else
	{
		return new BasicStatement( bb );
	}
}

