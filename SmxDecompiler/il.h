#pragma once

#include "il-cfg.h"
#include "smx-file.h"

#include <vector>
#include <string>
#include <cassert>

class ILBlock;
class ILConst;
class ILUnary;
class ILBinary;
class ILLocalVar;
class ILGlobalVar;
class ILHeapVar;
class ILArrayElementVar;
class ILFieldVar;
class ILTempVar;
class ILLoad;
class ILStore;
class ILJump;
class ILJumpCond;
class ILSwitch;
class ILCall;
class ILNative;
class ILReturn;
class ILPhi;
class ILInterval;

class ILVisitor
{
public:
	virtual ~ILVisitor() = default;

	virtual void VisitConst( ILConst* node ) {}
	virtual void VisitUnary( ILUnary* node ) {}
	virtual void VisitBinary( ILBinary* node ) {}
	virtual void VisitLocalVar( ILLocalVar* node ) {}
	virtual void VisitGlobalVar( ILGlobalVar* node ) {}
	virtual void VisitHeapVar( ILHeapVar* node ) {}
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) {}
	virtual void VisitFieldVar( ILFieldVar* node ) {}
	virtual void VisitTempVar( ILTempVar* node ) {}
	virtual void VisitLoad( ILLoad* node ) {}
	virtual void VisitStore( ILStore* node ) {}
	virtual void VisitJump( ILJump* node ) {}
	virtual void VisitJumpCond( ILJumpCond* node ) {}
	virtual void VisitSwitch( ILSwitch* node ) {}
	virtual void VisitCall( ILCall* node ) {}
	virtual void VisitNative( ILNative* node ) {}
	virtual void VisitReturn( ILReturn* node ) {}
	virtual void VisitPhi( ILPhi* node ) {}
	virtual void VisitInterval( ILInterval* node ) {}
};

class ILNode
{
public:
	virtual ~ILNode() = default;

	void AddUse( ILNode* user ) { uses_.push_back( user ); }
	void ReplaceUsesWith( ILNode* replacement )
	{
		for( ILNode* use : uses_ )
			use->ReplaceParam( this, replacement );
		uses_.clear();
	}
	size_t num_uses() const { return uses_.size(); }
	ILNode* use( size_t index ) { return uses_[index]; }
	void RemoveUse( size_t index ) { uses_.erase( uses_.begin() + index ); }
	void RemoveUse( ILNode* user )
	{
		auto it = std::find( uses_.begin(), uses_.end(), user );
		if( it != uses_.end() )
			uses_.erase( it );
	}
	
	const SmxVariableType* type() const { return type_; }
	void SetType( const SmxVariableType* type ) { type_ = type; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) {}

	virtual void Accept( ILVisitor* visitor ) = 0;
private:
	std::vector<ILNode*> uses_;
	const SmxVariableType* type_ = nullptr;
};

class ILConst : public ILNode
{
public:
	ILConst( cell_t val )
		:
		val_( val )
	{}

	cell_t value() const { return val_; }
	float value_as_float() const
	{
		union
		{
			cell_t c;
			float f;
		} x;

		x.c = val_;
		return x.f;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitConst( this ); }
private:
	cell_t val_;
};

class ILUnary : public ILNode
{
public:
	enum UnaryOp
	{
		NOT,
		NEG,
		INVERT,

		FABS,
		FLOAT,
		FLOATNOT,
		RND_TO_NEAREST,
		RND_TO_CEIL,
		RND_TO_ZERO,
		RND_TO_FLOOR,

		INC,
		DEC
	};

	ILUnary( ILNode* val, UnaryOp op )
		:
		val_( val ),
		op_( op )
	{
		val->AddUse( this );
	}

	ILNode* val() { return val_; }
	UnaryOp op() { return op_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( val_ == target );
		replacement->AddUse( this );
		val_ = replacement;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitUnary( this ); }
private:
	ILNode* val_;
	UnaryOp op_;
};

class ILBinary : public ILNode
{
public:
	enum BinaryOp
	{
		ADD,
		SUB,
		DIV,
		MUL,
		MOD,
		SHL,
		SHR,
		SSHR,
		BITAND,
		BITOR,
		XOR,

		EQ,
		NEQ,
		SGRTR,
		SGEQ,
		SLESS,
		SLEQ,
		AND,
		OR,

		FLOATADD,
		FLOATSUB,
		FLOATMUL,
		FLOATDIV,

		FLOATCMP,
		FLOATGT,
		FLOATGE,
		FLOATLE,
		FLOATLT,
		FLOATEQ,
		FLOATNE
	};

	ILBinary( ILNode* left, BinaryOp op, ILNode* right )
		:
		left_( left ),
		op_( op ),
		right_( right )
	{
		left->AddUse( this );
		right->AddUse( this );
	}

	BinaryOp op() const { return op_; }
	ILNode* left() const { return left_; }
	ILNode* right() const { return right_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( left_ == target || right_ == target );
		replacement->AddUse( this );
		if( left_ == target )
		{
			left_ = replacement;
		}
		else
		{
			right_ = replacement;
		}
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitBinary( this ); }
private:
	BinaryOp op_;
	ILNode* left_;
	ILNode* right_;
};

class ILVar : public ILNode
{
public:
	SmxVariable* smx_var() const { return var_; }
	void SetSmxVar( SmxVariable* var ) { var_ = var; }
private:
	SmxVariable* var_ = nullptr;
};

class ILLocalVar : public ILVar
{
public:
	ILLocalVar( int stack_offset, ILNode* value )
		:
		stack_offset_( stack_offset ),
		value_( value )
	{
		if( value )
			value->AddUse( this );
	}

	void SetValue( ILNode* val ) { value_ = val; value_->AddUse( this ); }
	ILNode* value() const { return value_; }
	int stack_offset() const { return stack_offset_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( value_ == target );
		if( replacement )
			replacement->AddUse( this );
		if( value_ )
			value_->RemoveUse( this );
		value_ = replacement;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitLocalVar( this ); }
private:
	int stack_offset_;
	ILNode* value_;
};

class ILGlobalVar : public ILVar
{
public:
	ILGlobalVar( cell_t addr )
		:
		addr_( addr )
	{}

	cell_t addr() const { return addr_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitGlobalVar( this ); }
private:
	cell_t addr_;
};

class ILHeapVar : public ILVar
{
public:
	ILHeapVar( cell_t addr, cell_t size )
		:
		addr_( addr ),
		size_( size )
	{}

	cell_t addr() const { return addr_; }
	cell_t size() const { return size_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitHeapVar( this ); }
private:
	cell_t addr_;
	cell_t size_;
};

class ILArrayElementVar : public ILVar
{
public:
	ILArrayElementVar( ILNode* base, ILNode* index )
		:
		base_( base ),
		index_( index )
	{
		base->AddUse( this );
		index->AddUse( this );
	}

	ILNode* base() { return base_; }
	ILNode* index() { return index_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( base_ == target || index_ == target );
		replacement->AddUse( this );
		if( base_ == target )
		{
			base_->RemoveUse( this );
			base_ = replacement;
		}
		else
		{
			index_->RemoveUse( this );
			index_ = replacement;
		}
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitArrayElementVar( this ); }
private:
	ILNode* base_;
	ILNode* index_;
};

class ILFieldVar : public ILVar
{
public:
	ILFieldVar( ILVar* base, size_t offset, SmxESField* field )
		:
		base_( base ),
		offset_( offset ),
		field_( field )
	{}

	ILVar* base() { return base_; }
	size_t offset() { return offset_; }
	SmxESField* field() { return field_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitFieldVar( this ); }
private:
	ILVar* base_;
	size_t offset_;
	SmxESField* field_;
};

class ILTempVar : public ILVar
{
public:
	ILTempVar( size_t index, ILNode* value )
		:
		index_( index ),
		value_( value )
	{}

	size_t index() const { return index_; }
	void SetValue( ILNode* value ) { value_ = value; }
	ILNode* value() { return value_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitTempVar( this ); }
private:
	size_t index_;
	ILNode* value_;
};

class ILLoad : public ILNode
{
public:
	ILLoad( ILVar* var, size_t width = 4 )
		:
		var_( var ),
		width_( width )
	{
		assert( width == 1 || width == 2 || width == 4 );
		assert( var_ );
		var->AddUse( this );
	}

	ILVar* var() { return var_; }
	size_t width() const { return width_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( var_ == target && dynamic_cast<ILVar*>( target ) );
		replacement->AddUse( this );
		var_->RemoveUse( this );
		var_ = dynamic_cast<ILVar*>( replacement );
		assert( var_ );
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitLoad( this ); }
private:
	size_t width_;
	ILVar* var_;
};

class ILStore : public ILNode
{
public:
	ILStore( ILVar* var, ILNode* val, size_t width = 4 )
		:
		var_( var ),
		val_( val ),
		width_( width )
	{
		assert( width == 1 || width == 2 || width == 4 );
		var->AddUse( this );
		val->AddUse( this );
	}

	size_t width() const { return width_; }
	ILVar* var() { return var_; }
	ILNode* val() { return val_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( var_ == target || val_ == target );
		replacement->AddUse( this );
		if( var_ == target )
		{
			var_->RemoveUse( this );
			var_ = dynamic_cast<ILVar*>(replacement);
			assert( var_ );
		}
		else
		{
			val_->RemoveUse( this );
			val_ = replacement;
		}
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitStore( this ); }
private:
	size_t width_;
	ILVar* var_;
	ILNode* val_;
};

class ILJump : public ILNode
{
public:
	ILJump( ILBlock* target )
		:
		target_( target )
	{}

	ILBlock* target() { return target_; }

	void ReplaceTarget( ILBlock* from, ILBlock* to )
	{
		if( target_ == from )
			target_ = to;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitJump( this ); }
private:
	ILBlock* target_;
};

class ILJumpCond : public ILNode
{
public:
	ILJumpCond( ILNode* condition, ILBlock* true_branch, ILBlock* false_branch )
		:
		condition_( condition ),
		true_branch_( true_branch ),
		false_branch_( false_branch )
	{
		condition_->AddUse( this );
	}

	ILNode* condition() { return condition_; }
	ILBlock* true_branch() { return true_branch_; }
	ILBlock* false_branch() { return false_branch_; }

	void Invert();

	void ReplaceTarget( ILBlock* from, ILBlock* to )
	{
		if( true_branch_ == from )
			true_branch_ = to;
		else if( false_branch_ == from )
			false_branch_ = to;
	}
	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( condition_ == target );
		replacement->AddUse( this );
		condition_->RemoveUse( this );
		condition_ = replacement;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitJumpCond( this ); }
private:
	ILNode* condition_;
	ILBlock* true_branch_;
	ILBlock* false_branch_;
};

struct CaseTableEntry
{
	cell_t value;
	ILBlock* address;
};

class ILSwitch : public ILNode
{
public:
	ILSwitch( ILNode* value, ILBlock* default_case, std::vector<CaseTableEntry> cases )
		:
		value_( value ),
		default_case_( default_case ),
		cases_( std::move( cases ) )
	{
		value->AddUse( this );
	}

	ILNode* value() { return value_; }
	ILBlock* default_case() { return default_case_; }
	CaseTableEntry& case_entry( size_t index ) { return cases_[index]; }
	size_t num_cases() const { return cases_.size(); }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( value_ == target );
		replacement->AddUse( this );
		value_->RemoveUse( this );
		value_ = replacement;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitSwitch( this ); }
private:
	ILNode* value_;
	ILBlock* default_case_;
	std::vector<CaseTableEntry> cases_;
};

class ILCallable : public ILNode
{
public:
	void AddArg( ILNode* arg ) { args_.push_back( arg ); arg->AddUse( this ); }

	size_t num_args() const { return args_.size(); }
	ILNode* arg( size_t index ) { return args_[index]; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		replacement->AddUse( this );
		for( size_t i = 0; i < args_.size(); i++ )
		{
			if( args_[i] == target )
			{
				args_[i]->RemoveUse( this );
				args_[i] = replacement;
				break;
			}
		}
	}
private:
	std::vector<ILNode*> args_;
};

class ILCall : public ILCallable
{
public:
	ILCall( cell_t addr ) : addr_( addr ) {}

	cell_t addr() const { return addr_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitCall( this ); }
private:
	cell_t addr_;
};

class ILNative : public ILCallable
{
public:
	ILNative( cell_t native_index ) : native_index_( native_index ) {}

	cell_t native_index() const { return native_index_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitNative( this ); }
private:
	cell_t native_index_;
};

class ILReturn : public ILNode
{
public:
	ILReturn( ILNode* value ) : value_( value ) { value->AddUse( this ); }

	ILNode* value() { return value_; }

	virtual void ReplaceParam( ILNode* target, ILNode* replacement ) override
	{
		assert( value_ == target );
		if( replacement )
			replacement->AddUse( this );
		if( value_ )
			value_->RemoveUse( this );
		value_ = replacement;
	}

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitReturn( this ); }
private:
	ILNode* value_;
};

class ILPhi : public ILNode
{
public:
	void AddInput( ILNode* input ) { inputs_.push_back( input ); }
	size_t num_inputs() const { return inputs_.size(); }
	ILNode* input( size_t index ) { return inputs_[index]; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitPhi( this ); }
private:
	std::vector<ILNode*> inputs_;
};

class ILInterval : public ILNode
{
public:
	ILInterval( ILBlock* block ) : inner_( block ) {}
	
	ILBlock* block() { return inner_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitInterval( this ); }
private:
	ILBlock* inner_;
};

class RecursiveILVisitor : public ILVisitor
{
protected:
	virtual void VisitUnary( ILUnary* node ) override
	{
		node->val()->Accept( this );
	}
	virtual void VisitBinary( ILBinary* node ) override
	{
		node->left()->Accept( this );
		node->right()->Accept( this );
	}
	virtual void VisitLocalVar( ILLocalVar* node )
	{
		if( node->value() )
			node->value()->Accept( this );
	}
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		node->base()->Accept( this );
		node->index()->Accept( this );
	}
	virtual void VisitLoad( ILLoad* node ) override
	{
		node->var()->Accept( this );
	}
	virtual void VisitStore( ILStore* node ) override
	{
		node->var()->Accept( this );
		node->val()->Accept( this );
	}
	virtual void VisitJumpCond( ILJumpCond* node ) override
	{
		node->condition()->Accept( this );
	}
	virtual void VisitSwitch( ILSwitch* node ) override
	{
		node->value()->Accept( this );
	}
	virtual void VisitCall( ILCall* node ) override
	{
		for( size_t i = 0; i < node->num_args(); i++ )
			node->arg( i )->Accept( this );
	}
	virtual void VisitNative( ILNative* node ) override
	{
		for( size_t i = 0; i < node->num_args(); i++ )
			node->arg( i )->Accept( this );
	}
	virtual void VisitReturn( ILReturn* node ) override
	{
		if( node->value() )
			node->value()->Accept( this );
	}
};