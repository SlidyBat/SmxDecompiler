#pragma once

#include "il-cfg.h"
#include "statement.h"

class Structurizer
{
public:
	Structurizer( ILControlFlowGraph* cfg );

	Statement* Transform();
private:
	void FindBlocksInInterval( ILBlock* interval, size_t level, std::vector<ILBlock*>& blocks );
	void FindBlocksInLoop( ILBlock* head, ILBlock* latch, const std::vector<ILBlock*>& blocks );
	void MarkLoops();
	void MarkIfs();
	ILBlock*& LoopHead( ILBlock* bb ) { return loop_heads_[bb->id()]; }
	ILBlock*& LoopLatch( ILBlock* bb ) { return loop_latch_[bb->id()]; }
	ILBlock*& IfFollow( ILBlock* bb ) { return if_follow_[bb->id()]; }

	Statement*& StatementForBlock( ILBlock* bb ) { return statements_[bb->id()]; }

	Statement* CreateStatement( ILBlock* bb, ILBlock* loop_latch );
	Statement* CreateLoopStatement( ILBlock* bb, ILBlock* loop_latch );
	Statement* CreateNonLoopStatement( ILBlock* bb, ILBlock* loop_latch );

	void PushScope( ILBlock* follow ) { follow_stack_.push_back( follow ); }
	void PopScope() { follow_stack_.pop_back(); }
	bool IsPartOfOuterScope( ILBlock* block ) const { return std::find( follow_stack_.begin(), follow_stack_.end(), block ) != follow_stack_.end(); }

	ILControlFlowGraph* cfg() { return derived_[0]; }
private:
	std::vector<ILControlFlowGraph*> derived_;
	std::vector<ILBlock*> loop_heads_;
	std::vector<ILBlock*> loop_latch_;
	std::vector<ILBlock*> if_follow_;
	std::vector<ILBlock*> follow_stack_;
	std::vector<Statement*> statements_;
};