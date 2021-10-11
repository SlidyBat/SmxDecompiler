#pragma once

#include "il.h"
#include <sstream>

class ILDisassembler : ILVisitor
{
public:
	ILDisassembler( SmxFile& smx );

	std::string DisassembleNode( ILNode* node );
	std::string DisassembleBlock( const ILBlock& block );
	std::string DisassembleCFG( const ILControlFlowGraph& cfg );
private:
	virtual void VisitConst( ILConst* node ) override;
	virtual void VisitUnary( ILUnary* node ) override;
	virtual void VisitBinary( ILBinary* node ) override;
	virtual void VisitLocalVar( ILLocalVar* node ) override;
	virtual void VisitGlobalVar( ILGlobalVar* node ) override;
	virtual void VisitHeapVar( ILHeapVar* node ) override;
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override;
	virtual void VisitTempVar( ILTempVar* node ) override;
	virtual void VisitLoad( ILLoad* node ) override;
	virtual void VisitStore( ILStore* node ) override;
	virtual void VisitJump( ILJump* node ) override;
	virtual void VisitJumpCond( ILJumpCond* node ) override;
	virtual void VisitSwitch( ILSwitch* node ) override;
	virtual void VisitCall( ILCall* node ) override;
	virtual void VisitNative( ILNative* node ) override;
	virtual void VisitReturn( ILReturn* node ) override;
	virtual void VisitPhi( ILPhi* node ) override;

	std::string Visit( ILNode* node );
	std::string VisitTopLevel( ILNode* node );
private:
	SmxFile* smx_;
	SmxFunction* func_;
	std::stringstream disasm_;
	bool top_level_;
};