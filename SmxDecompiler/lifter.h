#pragma once

#include "smx-file.h"
#include "il-cfg.h"
#include "cfg.h"

class ILLocalVar;

class PcodeLifter
{
public:
	PcodeLifter( const SmxFile& smx ) : smx_( &smx ) {}

	ILControlFlowGraph* Lift( const ControlFlowGraph& cfg );
private:
	void LiftBlock( BasicBlock& bb, ILBlock& ilbb );
	void CleanCalls( ILBlock& ilbb );
	void PruneVarsInBlock( ILBlock& ilbb );
	void MovePhis( ILBlock& ilbb );
	void CompoundConditions() const;
	void CompoundXorY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const;
	void CompoundXandY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const;
	void CompoundNotXorY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const;
	void CompoundNotXandY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const;


	ILLocalVar* Push( ILNode* value );
	ILLocalVar* Pop();
	ILNode* PopValue();
	ILLocalVar* GetFrameVar( int offset );
	ILNode* GetFrameVal( int offset );
	void SetFrameVal( int offset, ILNode* val );
	class ILTempVar* MakeTemp( ILNode* value );
	class ILVar* GetVar( ILNode* node ) const;
private:
	const SmxFile* smx_;
	ILControlFlowGraph* ilcfg_;

	struct AbstractExprStack
	{
		std::vector<ILLocalVar*> stack;
		ILNode* pri;
		ILNode* alt;
	};

	std::vector<AbstractExprStack> block_stacks_;
	size_t num_temps_ = 0;
	AbstractExprStack* expr_stack_ = nullptr;
	cell_t heap_addr_ = 0;
};