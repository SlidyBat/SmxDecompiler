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
	ILBlock*& LoopHead( ILBlock* bb ) { return loop_heads_[bb->id()]; }
	ILBlock*& LoopLatch( ILBlock* bb ) { return loop_latch_[bb->id()]; }
	Statement*& StatementForBlock( ILBlock* bb );

	Statement* CreateStatement( ILBlock* bb );
	Statement* CreateLoopStatement( ILBlock* bb );
	Statement* CreateNonLoopStatement( ILBlock* bb );

	ILControlFlowGraph* cfg() { return derived_[0]; }
private:
	std::vector<ILControlFlowGraph*> derived_;
	std::vector<ILBlock*> loop_heads_;
	std::vector<ILBlock*> loop_latch_;
	std::vector<Statement*> statements_;
};