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

	loop_heads_.resize( cfg->max_id() + 1, nullptr );
	loop_latch_.resize( cfg->max_id() + 1, nullptr );
	if_follow_.resize( cfg->max_id() + 1, nullptr );
	statements_.resize( cfg->max_id() + 1, nullptr );
}

Statement* Structurizer::Transform()
{
	MarkLoops();
	MarkIfs();

	cfg()->NewEpoch();
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

	bool all_edges_loop = true;
	for( size_t i = 0; i < head->num_out_edges(); i++ )
	{
		if( LoopHead( &head->out_edge( i ) ) != head )
		{
			all_edges_loop = false;
			break;
		}
	}

	if( all_edges_loop )
	{
		// Either in infinite loop, or do/while loop
		// In a do/while loop the latch is a jmp which has a conditional predecessor (which is the real latch)
		// In a infinite loop the latch has no condition
		if( latch->num_in_edges() == 1 &&
			latch->in_edge( 0 ).num_out_edges() == 2 )
		{
			// This is a do/while loop, correct the latch
			latch = &latch->in_edge( 0 );
		}
	}

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

		if( bb->immed_post_dominator() != bb )
		{
			IfFollow( bb ) = bb->immed_post_dominator();
		}
		else
		{
			for( int j = i + 1; j < cfg()->num_blocks(); j++ )
			{
				ILBlock* potential_follow = &cfg()->block( j );
				if( potential_follow->immed_dominator() == bb &&
					potential_follow != &bb->out_edge( 0 ) &&
					potential_follow != &bb->out_edge( 1 ) )
				{
					IfFollow( bb ) = potential_follow;
					break;
				}
			}
		}
	}
}

Statement* Structurizer::CreateStatement( ILBlock* bb )
{
	if( !bb )
		return nullptr;

	const Scope* outer_scope = FindInOuterScope( bb );
	if( outer_scope &&
		outer_scope->type != ScopeType::BASIC &&
		outer_scope->type != ScopeType::LATCH &&
		CanEmitBreakOrContinue( outer_scope ) )
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

	Statement* stmt;
	if( bb->IsVisited() )
	{
		stmt = StatementForBlock( bb );
		assert( stmt );
		if( !stmt->label() )
			stmt->CreateLabel( bb->pc() );
		return new GotoStatement( stmt );
	}

	if( outer_scope )
	{
		if( outer_scope->type == ScopeType::LATCH )
			return new BasicStatement( bb, nullptr );
	
		return nullptr;
	}

	if( LoopHead( bb ) == bb )
	{
		stmt = CreateLoopStatement( bb );
	}
	else
	{
		stmt = CreateNonLoopStatement( bb );
	}

	StatementForBlock( bb ) = stmt;
	bb->SetVisited();

	return stmt;
}

Statement* Structurizer::CreateLoopStatement( ILBlock* bb )
{
	ILBlock* head = bb;
	ILBlock* latch = LoopLatch( head );

	if( latch->num_out_edges() == 2 )
	{
		// Condition is at the end of the loop
		return CreateDoWhileStatement( head, latch );
	}
	if( head->num_out_edges() == 2 && latch->num_out_edges() == 1 )
	{
		// Condition at the start and no condition at the end, this is probably a while loop
		// 
		// However, if the condition at the start doesn't break out of the loop, then it is
		// an endless loop
		ILBlock* exit1 = LoopHead( &head->out_edge( 0 ) );
		ILBlock* exit2 = LoopHead( &head->out_edge( 1 ) );
		if( exit1 == exit2 )
		{
			// Both exits are still part of the same loop, this must be endless loop
			return CreateEndlessStatement( head, latch );
		}

		return CreateWhileStatement( head, latch );
	}

	return nullptr;
}

Statement* Structurizer::CreateNonLoopStatement( ILBlock* bb )
{
	if( auto* switch_node = dynamic_cast<ILSwitch*>(bb->Last()) )
	{
		return CreateSwitchStatement( bb, switch_node );
	}
	else if( bb->num_out_edges() == 2 )
	{
		return CreateIfStatement( bb );
	}
	else if( bb->num_out_edges() == 1 )
	{
		ILBlock* succ = &bb->out_edge( 0 );
		const Scope* scope = FindInOuterScope( succ );

		Statement* next_stmt = nullptr;
		if( !scope || scope->type != ScopeType::BASIC )
			next_stmt = CreateStatement( succ );

		return new BasicStatement( bb, next_stmt );
	}
	else
	{
		assert( bb->num_out_edges() == 0 );
		return new BasicStatement( bb, nullptr );
	}
}

Statement* Structurizer::CreateDoWhileStatement( ILBlock* head, ILBlock* latch )
{
	ILBlock* succ1 = &latch->out_edge( 0 );
	ILBlock* succ2 = &latch->out_edge( 1 );

	// Determine which is the body and which is the follow
	ILBlock* body;
	ILBlock* follow;
	if( latch->IsBackEdge( 0 ) )
	{
		body = succ1;
		follow = succ2;
	}
	else
	{
		body = succ2;
		follow = succ1;
	}

	ILJumpCond* jmp = static_cast<ILJumpCond*>( latch->Last() );
	if( jmp->true_branch() != body )
		jmp->Invert();

	PushScope( follow, ScopeType::BREAK );
	PushScope( latch, ScopeType::CONTINUE );

	Statement* body_stmt = nullptr;
	if( head != latch )
		body_stmt = CreateNonLoopStatement( head );

	PopScope();
	PopScope();

	Statement* next_stmt = CreateStatement( follow );
	return new DoWhileStatement( jmp->condition(), body_stmt, next_stmt );
}

Statement* Structurizer::CreateEndlessStatement( ILBlock* head, ILBlock* latch )
{
	ILBlock* follow = head->immed_post_dominator();

	PushScope( head, ScopeType::CONTINUE );
	PushScope( follow, ScopeType::BREAK );
	PushScope( latch, ScopeType::LATCH );

	Statement* body_stmt = nullptr;
	if( head != latch )
		body_stmt = CreateNonLoopStatement( head );

	PopScope();
	PopScope();
	PopScope();

	Statement* next_stmt = CreateStatement( follow );
	return new EndlessStatement( body_stmt, next_stmt );
}

Statement* Structurizer::CreateWhileStatement( ILBlock* head, ILBlock* latch )
{
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

	ILJumpCond* jmp = static_cast<ILJumpCond*>(head->Last());
	if( jmp->true_branch() != body )
		jmp->Invert();

	PushScope( head, ScopeType::CONTINUE );
	PushScope( follow, ScopeType::BREAK );
	PushScope( latch, ScopeType::LATCH );

	Statement* body_stmt = nullptr;
	if( head != latch )
		body_stmt = CreateStatement( body );

	PopScope();
	PopScope();
	PopScope();

	Statement* next_stmt = CreateStatement( follow );
	return new WhileStatement( jmp->condition(), body_stmt, next_stmt );
}

Statement* Structurizer::CreateIfStatement( ILBlock* bb )
{
	ILBlock* follow = IfFollow( bb );

	auto* jmp = static_cast<ILJumpCond*>(bb->Last());
	jmp->Invert();

	//if( !follow )
	//{
	//	follow = jmp->false_branch();
	//}

	if( follow == jmp->true_branch() )
		jmp->Invert();

	// Heuristic: If the then branch terminates, then get rid of the else branch
	// This won't catch cases all the cases where else branch can be removed, is there way to make this more generic?
	if( jmp->true_branch()->num_out_edges() == 0 )
	{
		follow = jmp->false_branch();
	}

	PushScope( follow, ScopeType::BASIC );

	Statement* then_branch = nullptr;
	if( jmp->true_branch() != follow )
		then_branch = CreateStatement( jmp->true_branch() );
	Statement* else_branch = nullptr;
	if( jmp->false_branch() != follow )
		else_branch = CreateStatement( jmp->false_branch() );

	PopScope();

	Statement* next_stmt = CreateStatement( follow );
	Statement* if_stmt = new IfStatement( jmp->condition(), then_branch, else_branch, next_stmt );

	// There can be some code before the JumpCond in the head, if there is then add that here too
	if( bb->num_nodes() > 1 )
		if_stmt = new BasicStatement( bb, if_stmt );

	return if_stmt;
}

Statement* Structurizer::CreateSwitchStatement( ILBlock* bb, ILSwitch* switch_node )
{
	ILBlock* follow = bb->immed_post_dominator();
	if( follow == bb )
		follow = nullptr;

	// This is intentionally not BREAK, sp has no fall-through on cases so the break is implicit
	PushScope( follow, ScopeType::BASIC );

	Statement* default_case = nullptr;
	if( switch_node->default_case() != follow )
		default_case = CreateStatement( switch_node->default_case() );

	std::vector<CaseStatement> cases;
	cases.reserve( switch_node->num_cases() );
	for( size_t i = 0; i < switch_node->num_cases(); i++ )
	{
		CaseStatement case_entry;
		case_entry.body = CreateStatement( switch_node->case_entry( i ).address );
		case_entry.value = switch_node->case_entry( i ).value;
		cases.push_back( case_entry );
	}

	PopScope();

	Statement* next_stmt = CreateStatement( follow );
	Statement* switch_stmt = new SwitchStatement( switch_node->value(), default_case, std::move( cases ), next_stmt );

	// There can be some code before the Switch in the head, if there is then add that here too
	if( bb->num_nodes() > 1 )
		switch_stmt = new BasicStatement( bb, switch_stmt );

	return switch_stmt;
}

const Structurizer::Scope* Structurizer::FindInOuterScope( ILBlock* block ) const
{
	for( int i = (int)scope_stack_.size() - 1; i >= 0; i-- )
	{
		if( scope_stack_[i].follow == block )
			return &scope_stack_[i];
	}
	return nullptr;
}

bool Structurizer::CanEmitBreakOrContinue( const Scope* scope ) const
{
	// If this scope is already the outermost one, then there is no need to emit a break/continue
	bool should_emit = false;
	bool is_outermost_loop = true;

	size_t index = scope - &scope_stack_[0];
	for( size_t i = index + 1; i < scope_stack_.size(); i++ )
	{
		if( scope_stack_[i].type == scope->type )
			is_outermost_loop = false;
		else if( scope_stack_[i].type == ScopeType::BASIC )
			should_emit = true;
	}

	// Need to generate a goto in this case, which we don't do at the moment
	assert( is_outermost_loop );

	return should_emit && is_outermost_loop;
}

