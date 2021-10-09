#pragma once

#include <sstream>
#include "structurizer.h"

class CodeWriter : public StatementVisitor, public ILVisitor
{
public:
	CodeWriter( SmxFile& smx, const char* function );

	std::string Build( Statement* stmt );

	virtual void VisitBasicStatement( BasicStatement* stmt ) override;
	virtual void VisitSequenceStatement( SequenceStatement* stmt ) override;
	virtual void VisitIfStatement( IfStatement* stmt ) override;
	virtual void VisitWhileStatement( WhileStatement* stmt ) override;
	virtual void VisitSwitchStatement( SwitchStatement* stmt ) override;

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
	virtual void VisitCall( ILCall* node ) override;
	virtual void VisitNative( ILNative* node ) override;
	virtual void VisitReturn( ILReturn* node ) override;
	virtual void VisitPhi( ILPhi* node ) override;
	virtual void VisitInterval( ILInterval* node ) override;
private:
	std::string Build( ILBlock* block );
	std::string Build( ILNode* node );

	std::string Tabs();
	void Indent();
	void Dedent();
private:
	SmxFile* smx_;
	SmxFunction* func_;
	std::stringstream code_;
	int indent_ = 0;
	int level_ = 0;
};