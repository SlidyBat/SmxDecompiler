#pragma once

#include "il-cfg.h"
#include "statement.h"

class Structurizer
{
public:
	Structurizer( ILControlFlowGraph* cfg );

	Statement* Transform();
private:
	enum class ScopeType
	{
		BASIC,
		BREAK,
		CONTINUE,
		LATCH
	};
	struct Scope
	{
		Scope( ILBlock* follow, ScopeType type ) :
			follow( follow ),
			type( type )
		{}

		ILBlock* follow;
		ScopeType type;
	};

	void FindBlocksInInterval( ILBlock* interval, size_t level, std::vector<ILBlock*>& blocks );
	void FindBlocksInLoop( ILBlock* head, ILBlock* latch, const std::vector<ILBlock*>& blocks );
	void MarkLoops();
	void MarkIfs();
	ILBlock*& LoopHead( ILBlock* bb ) { return loop_heads_[bb->id()]; }
	ILBlock*& LoopLatch( ILBlock* bb ) { return loop_latch_[bb->id()]; }
	ILBlock*& IfFollow( ILBlock* bb ) { return if_follow_[bb->id()]; }

	Statement*& StatementForBlock( ILBlock* bb ) { return statements_[bb->id()]; }

	Statement* CreateStatement( ILBlock* bb );
	Statement* CreateLoopStatement( ILBlock* bb );
	Statement* CreateNonLoopStatement( ILBlock* bb );
	Statement* CreateDoWhileStatement( ILBlock* head, ILBlock* latch );
	Statement* CreateEndlessStatement( ILBlock* head, ILBlock* latch );
	Statement* CreateWhileStatement( ILBlock* head, ILBlock* latch );
	Statement* CreateIfStatement( ILBlock* bb );
	Statement* CreateSwitchStatement( ILBlock* bb, ILSwitch* switch_node );

	void PushScope( ILBlock* follow, ScopeType type ) { scope_stack_.emplace_back( follow, type ); }
	void PopScope() { scope_stack_.pop_back(); }
	const Scope* FindInOuterScope( ILBlock* block ) const;
	bool CanEmitBreakOrContinue( const Scope* scope ) const;

	ILControlFlowGraph* cfg() { return derived_[0]; }
private:
	std::vector<ILControlFlowGraph*> derived_;
	std::vector<ILBlock*> loop_heads_;
	std::vector<ILBlock*> loop_latch_;
	std::vector<ILBlock*> if_follow_;
	std::vector<Scope> scope_stack_;
	std::vector<Statement*> statements_;
};