#pragma once

#include "il-cfg.h"

#include <vector>
#include <string>
#include <cassert>

class ILConst;
class ILUnary;
class ILBinary;
class ILLocalVar;
class ILGlobalVar;
class ILHeapVar;
class ILArrayElementVar;
class ILLoad;
class ILStore;
class ILJump;
class ILJumpCond;
class ILCall;
class ILNative;
class ILRet;

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
	virtual void VisitLoad( ILLoad* node ) {}
	virtual void VisitStore( ILStore* node ) {}
	virtual void VisitJump( ILJump* node ) {}
	virtual void VisitJumpCond( ILJumpCond* node ) {}
	virtual void VisitCall( ILCall* node ) {}
	virtual void VisitNative( ILNative* node ) {}
	virtual void VisitRet( ILRet* node ) {}
};

class ILNode
{
public:
	virtual ~ILNode() = default;
	virtual void Accept( ILVisitor* visitor ) = 0;
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
	{}

	ILNode* val() { return val_; }
	UnaryOp op() { return op_; }

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
		AND,
		OR,
		XOR,

		EQ,
		NEQ,
		SGRTR,
		SGEQ,
		SLESS,
		SLEQ,

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
	{}

	BinaryOp op() const { return op_; }
	ILNode* left() const { return left_; }
	ILNode* right() const { return right_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitBinary( this ); }
private:
	BinaryOp op_;
	ILNode* left_;
	ILNode* right_;
};

class ILVar : public ILNode
{};

class ILLocalVar : public ILVar
{
public:
	ILLocalVar( int stack_offset )
		:
		stack_offset_( stack_offset )
	{}

	int stack_offset() const { return stack_offset_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitLocalVar( this ); }
private:
	int stack_offset_;
};

class ILGlobalVar : public ILVar
{
public:
	ILGlobalVar( int addr )
		:
		addr_( addr )
	{}

	int addr() const { return addr_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitGlobalVar( this ); }
private:
	int addr_;
};

class ILHeapVar : public ILVar
{
public:
	ILHeapVar( int size )
		:
		size_( size )
	{}

	int size() const { return size_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitHeapVar( this ); }
private:
	int size_;
};

class ILArrayElementVar : public ILVar
{
public:
	ILArrayElementVar( ILVar* base, ILNode* index )
		:
		base_( base ),
		index_( index )
	{}

	ILVar* base() { return base_; }
	ILNode* index() { return index_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitArrayElementVar( this ); }
private:
	ILVar* base_;
	ILNode* index_;
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
	}

	ILVar* var() { return var_; }
	size_t width() const { return width_; }

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
	}

	size_t width() const { return width_; }
	ILVar* var() { return var_; }
	ILNode* val() { return val_; }

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
	{}

	ILNode* condition() { return condition_; }
	ILBlock* true_branch() { return true_branch_; }
	ILBlock* false_branch() { return false_branch_; }

	virtual void Accept( ILVisitor* visitor ) { visitor->VisitJumpCond( this ); }
private:
	ILNode* condition_;
	ILBlock* true_branch_;
	ILBlock* false_branch_;
};

class ILCallable : public ILNode
{
public:
	void AddArg( ILNode* arg ) { args_.push_back( arg ); }

	size_t num_args() const { return args_.size(); }
	ILNode* arg( size_t index ) { return args_[index]; }
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

class ILRet : public ILNode
{
public:
	virtual void Accept( ILVisitor* visitor ) { visitor->VisitRet( this ); }
};