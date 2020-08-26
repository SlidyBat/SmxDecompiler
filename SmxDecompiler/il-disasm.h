#pragma once

#include "il.h"
#include <sstream>

class ILDisassembler : ILVisitor
{
public:
	std::string DisassembleNode( ILNode* node );
	std::string DisassembleBlock( const ILBlock& block );
private:
	virtual void VisitConst( ILConst* node ) override;
	virtual void VisitUnary( ILUnary* node ) override;
	virtual void VisitBinary( ILBinary* node ) override;
	virtual void VisitLocalVar( ILLocalVar* node ) override;
	virtual void VisitGlobalVar( ILGlobalVar* node ) override;
	virtual void VisitHeapVar( ILHeapVar* node ) override;
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override;
	virtual void VisitLoad( ILLoad* node ) override;
	virtual void VisitStore( ILStore* node ) override;
	virtual void VisitJump( ILJump* node ) override;
	virtual void VisitJumpCond( ILJumpCond* node ) override;
	virtual void VisitCall( ILCall* node ) override;
	virtual void VisitNative( ILNative* node ) override;
	virtual void VisitRet( ILRet* node ) override;

	std::string Visit( ILNode* node );
private:
	std::stringstream disasm_;
};