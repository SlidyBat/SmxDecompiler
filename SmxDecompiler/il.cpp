#include "il.h"

class Inverter : public ILVisitor
{
public:
	ILNode* Invert( ILNode* node )
	{
		ILNode* save = nullptr;
		std::swap( save, result_ );

		node->Accept( this );
		if( !result_ )
			result_ = new ILUnary( node, ILUnary::NOT );

		std::swap( save, result_ );
		return save;
	}

	virtual void VisitBinary( ILBinary* binary ) override
	{
		switch( binary->op() )
		{
			case ILBinary::EQ:
				result_ = new ILBinary( binary->left(), ILBinary::NEQ, binary->right() );
				return;
			case ILBinary::NEQ:
				result_ = new ILBinary( binary->left(), ILBinary::EQ, binary->right() );
				return;
			case ILBinary::SGRTR:
				result_ = new ILBinary( binary->left(), ILBinary::SLEQ, binary->right() );
				return;
			case ILBinary::SGEQ:
				result_ = new ILBinary( binary->left(), ILBinary::SLESS, binary->right() );
				return;
			case ILBinary::SLESS:
				result_ = new ILBinary( binary->left(), ILBinary::SGEQ, binary->right() );
				return;
			case ILBinary::SLEQ:
				result_ = new ILBinary( binary->left(), ILBinary::SGRTR, binary->right() );
				return;
			case ILBinary::AND:
				result_ = new ILBinary( Invert( binary->left() ), ILBinary::OR, Invert( binary->right() ) );
				return;
			case ILBinary::OR:
				result_ = new ILBinary( Invert( binary->left() ), ILBinary::AND, Invert( binary->right() ) );
				return;
		}
	}
	virtual void VisitUnary( ILUnary* unary ) override
	{
		switch( unary->op() )
		{
			case ILUnary::NOT:
				result_ = unary->val();
				return;
		}
	}
private:
	ILNode* result_ = nullptr;
};

void ILJumpCond::Invert()
{
	std::swap( true_branch_, false_branch_ );

	Inverter inv;
	condition_ = inv.Invert( condition_ );
}