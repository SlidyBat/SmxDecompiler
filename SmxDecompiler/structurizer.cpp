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
	if_follow_.resize( cfg->num_blocks(), nullptr );
	statements_.resize( cfg->num_blocks(), nullptr );
}

Statement* Structurizer::Transform()
{
	MarkLoops();
	MarkIfs();

	cfg()->NewEpoch();
	return CreateStatement( &cfg()->block( 0 ), nullptr );
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

void Structurizer::MarkIfs()
{
	for( int i = (int)cfg()->num_blocks() - 1; i >= 0; i-- )
	{
		ILBlock* bb = &cfg()->block( i );
		if( bb->num_out_edges() != 2 )
			continue;

		if( auto* switch_node = dynamic_cast<ILSwitch*>(bb->Last()) )
			continue;

		if( bb->immed_post_dominator() == bb )
		{
			IfFollow( bb ) = nullptr;
		}
		else
		{
			IfFollow( bb ) = bb->immed_post_dominator();
		}
	}
}

Statement* Structurizer::CreateStatement( ILBlock* bb, ILBlock* loop_latch )
{
	if( !bb )
		return nullptr;

	const Scope* outer_scope = FindInOuterScope( bb );
	if( outer_scope )
	{
		if( outer_scope->type == ScopeType::BASIC )
			return nullptr;

		if( !FindSameTypeScopeAfter( outer_scope ) )
		{
			if( outer_scope->type == ScopeType::BREAK )
			{
				return new BreakStatement;
			}
			else if( outer_scope->type == ScopeType::CONTINUE )
			{
				return new ContinueStatement;
			}

			assert( !"Unhandled scope type" );
			return new GotoStatement( StatementForBlock( bb ) );
		}
	}

	Statement* stmt;
	if( bb->IsVisited() )
	{
		stmt = StatementForBlock( bb );
		assert( stmt );
		if( !stmt->label() )
			stmt->CreateLabel( bb->pc() );
		return new GotoStatement( stmt );
	}

	if( bb == loop_latch )
	{
		return new BasicStatement( bb, nullptr );
	}

	if( LoopHead( bb ) == bb )
	{
		stmt = CreateLoopStatement( bb, loop_latch );
	}
	else
	{
		stmt = CreateNonLoopStatement( bb, loop_latch );
	}

	StatementForBlock( bb ) = stmt;
	bb->SetVisited();

	return stmt;
}

Statement* Structurizer::CreateLoopStatement( ILBlock* bb, ILBlock* loop_latch )
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

		PushScope( head, ScopeType::CONTINUE );
		PushScope( follow, ScopeType::BREAK);

		Statement* body_stmt = nullptr;
		if( head != latch )
			body_stmt = CreateStatement( body, latch );
		
		PopScope();
		PopScope();

		Statement* next_stmt = CreateStatement( follow, loop_latch );
		return new WhileStatement( jmp->condition(), body_stmt, next_stmt );
	}

	return nullptr;
}

Statement* Structurizer::CreateNonLoopStatement( ILBlock* bb, ILBlock* loop_latch )
{
	if( auto* switch_node = dynamic_cast<ILSwitch*>(bb->Last()) )
	{
		ILBlock* follow = bb->immed_post_dominator();

		// This is intentionally not BREAK, sp has no fall-through on cases so the break is implicit
		PushScope( follow, ScopeType::BASIC );

		Statement* default_case = nullptr;
		if( switch_node->default_case() != follow )
			default_case = CreateStatement( switch_node->default_case(), loop_latch );

		std::vector<CaseStatement> cases;
		cases.reserve( switch_node->num_cases() );
		for( size_t i = 0; i < switch_node->num_cases(); i++ )
		{
			CaseStatement case_entry;
			case_entry.body = CreateStatement( switch_node->case_entry( i ).address, loop_latch );
			case_entry.value = switch_node->case_entry( i ).value;
			cases.push_back( case_entry );
		}

		PopScope();

		Statement* next_stmt = CreateStatement( follow, loop_latch );
		Statement* switch_stmt = new SwitchStatement( switch_node->value(), default_case, std::move( cases ), next_stmt );

		// There can be some code before the Switch in the head, if there is then add that here too
		if( bb->num_nodes() > 1 )
			switch_stmt = new BasicStatement( bb, switch_stmt );

		return switch_stmt;
	}
	else if( bb->num_out_edges() == 2 )
	{
		ILBlock* follow = IfFollow( bb );

		auto* jmp = static_cast<ILJumpCond*>(bb->Last());
		jmp->Invert();

		if( !follow )
		{
			// The branches never converge, at least one of the branches terminates

			// First check if one of the other branches converges, if so, then that will be the follow
			if( jmp->true_branch()->immed_post_dominator() != jmp->true_branch() )
			{
				follow = jmp->true_branch()->immed_post_dominator();
			}
			else if( jmp->false_branch()->immed_post_dominator() != jmp->false_branch() )
			{
				follow = jmp->false_branch()->immed_post_dominator();
			}

			// Both branches terminate, this must be an if-then without an else
			// Mark the else branch as the follow
			if( !follow )
			{
				follow = jmp->false_branch();
			}
		}

		assert( follow );

		PushScope( follow, ScopeType::BASIC );

		Statement* then_branch = nullptr;
		if( jmp->true_branch() != follow )
			then_branch = CreateStatement( jmp->true_branch(), loop_latch );
		Statement* else_branch = nullptr;
		if( jmp->false_branch() != follow )
			else_branch = CreateStatement( jmp->false_branch(), loop_latch );

		PopScope();

		Statement* next_stmt = CreateStatement( follow, loop_latch );
		Statement* if_stmt = new IfStatement( jmp->condition(), then_branch, else_branch, next_stmt );

		// There can be some code before the JumpCond in the head, if there is then add that here too
		if( bb->num_nodes() > 1 )
			if_stmt = new BasicStatement( bb, if_stmt );

		return if_stmt;
	}
	else if( bb->num_out_edges() == 1 )
	{
		ILBlock* succ = &bb->out_edge( 0 );
		const Scope* scope = FindInOuterScope( succ );

		Statement* next_stmt = nullptr;
		if( !scope || scope->type != ScopeType::BASIC )
			next_stmt = CreateStatement( succ, loop_latch );

		return new BasicStatement( bb, next_stmt );
	}
	else
	{
		assert( bb->num_out_edges() == 0 );
		return new BasicStatement( bb, nullptr );
	}
}

const Structurizer::Scope* Structurizer::FindInOuterScope( ILBlock* block ) const
{
	for( const Scope& scope : scope_stack_ )
	{
		if( scope.follow == block )
			return &scope;
	}
	return nullptr;
}

bool Structurizer::FindSameTypeScopeAfter( const Scope* scope ) const
{
	size_t index = scope - &scope_stack_[0];
	for( size_t i = index + 1; i < scope_stack_.size(); i++ )
	{
		if( scope_stack_[i].type == scope->type )
			return true;
	}

	return false;
}

