#pragma once

#include <vector>
#include "il.h"

enum class StatementType
{
	BASIC,
	IF,
	ENDLESS,
	WHILE,
	DO_WHILE,
	SWITCH,
	CONTINUE,
	BREAK,
	GOTO
};

class BasicStatement;
class IfStatement;
class DoWhileStatement;
class EndlessStatement;
class WhileStatement;
class SwitchStatement;
class ContinueStatement;
class BreakStatement;
class GotoStatement;

class StatementVisitor
{
public:
	virtual void VisitBasicStatement( BasicStatement* stmt ) = 0;
	virtual void VisitIfStatement( IfStatement* stmt ) = 0;
	virtual void VisitDoWhileStatement( DoWhileStatement* stmt ) = 0;
	virtual void VisitEndlessStatement( EndlessStatement* stmt ) = 0;
	virtual void VisitWhileStatement( WhileStatement* stmt ) = 0;
	virtual void VisitSwitchStatement( SwitchStatement* stmt ) = 0;
	virtual void VisitContinueStatement( ContinueStatement* stmt ) = 0;
	virtual void VisitBreakStatement( BreakStatement* stmt ) = 0;
	virtual void VisitGotoStatement( GotoStatement* stmt ) = 0;
};

class Statement
{
public:
	Statement( StatementType type, Statement* next ) :
		type_( type ),
		next_( next )
	{}
	virtual ~Statement() = default;

	virtual void Accept( StatementVisitor* visitor ) = 0;

	void CreateLabel( cell_t pc ) { label_ = "label_" + std::to_string( pc ); }
	const char* label() const { return label_.empty() ? nullptr : label_.c_str(); }

	StatementType type() const { return type_; }
	Statement* next() { return next_; }
private:
	StatementType type_;
	Statement* next_ = nullptr;
	std::string label_;
};

class BasicStatement : public Statement
{
public:
	BasicStatement( std::vector<ILNode*> nodes, Statement* next ) :
		Statement( StatementType::BASIC, next ),
		nodes_( std::move( nodes ) )
	{}
	BasicStatement( ILBlock* block, Statement* next ) :
		Statement( StatementType::BASIC, next )
	{
		size_t end = block->num_nodes();

		// If this is a jump then we don't want to include it, but if it is a fallthrough then keep it
		if( block->num_out_edges() > 1 ||
			dynamic_cast<ILJump*>(block->Last()) ||
			dynamic_cast<ILSwitch*>(block->Last()))
		{
			end -= 1;
		}

		for( size_t i = 0; i < end; i++ )
			nodes_.push_back( block->node( i ) );
	}

	size_t num_nodes() const { return nodes_.size(); }
	ILNode* node( size_t index ) { return nodes_[index]; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitBasicStatement( this ); }
private:
	std::vector<ILNode*> nodes_;
};

class IfStatement : public Statement
{
public:
	IfStatement( ILNode* condition, Statement* then_branch, Statement* else_branch, Statement* next ) :
		Statement( StatementType::IF, next ),
		condition_( condition ),
		then_branch_( then_branch ),
		else_branch_( else_branch )
	{}

	ILNode* condition() { return condition_; }
	Statement* then_branch() { return then_branch_; }
	Statement* else_branch() { return else_branch_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitIfStatement( this ); }
private:
	ILNode* condition_;
	Statement* then_branch_;
	Statement* else_branch_;
};

class EndlessStatement : public Statement
{
public:
	EndlessStatement( Statement* body, Statement* next ) :
		Statement( StatementType::ENDLESS, next ),
		body_( body )
	{}

	Statement* body() { return body_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitEndlessStatement( this ); }
private:
	Statement* body_;
};

class DoWhileStatement : public Statement
{
public:
	DoWhileStatement( ILNode* condition, Statement* body, Statement* next ) :
		Statement( StatementType::WHILE, next ),
		condition_( condition ),
		body_( body )
	{}

	ILNode* condition() { return condition_; }
	Statement* body() { return body_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitDoWhileStatement( this ); }
private:
	ILNode* condition_;
	Statement* body_;
};

class WhileStatement : public Statement
{
public:
	WhileStatement( ILNode* condition, Statement* body, Statement* next ) :
		Statement( StatementType::WHILE, next ),
		condition_( condition ),
		body_( body )
	{}

	ILNode* condition() { return condition_; }
	Statement* body() { return body_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitWhileStatement( this ); }
private:
	ILNode* condition_;
	Statement* body_;
};

struct CaseStatement
{
	cell_t value;
	Statement* body;
};

class SwitchStatement : public Statement
{
public:
	SwitchStatement( ILNode* value, Statement* default_case, std::vector<CaseStatement> cases, Statement* next ) :
		Statement( StatementType::WHILE, next ),
		value_( value ),
		default_case_( default_case ),
		cases_( std::move( cases ) )
	{}

	ILNode* value() { return value_; }
	Statement* default_case() { return default_case_; }
	size_t num_cases() const { return cases_.size(); }
	const CaseStatement& case_entry( size_t index ) const { return cases_[index]; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitSwitchStatement( this ); }
private:
	ILNode* value_;
	Statement* default_case_;
	std::vector<CaseStatement> cases_;
};

class ContinueStatement : public Statement
{
public:
	ContinueStatement() :
		Statement( StatementType::CONTINUE, nullptr )
	{}

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitContinueStatement( this ); }
};

class BreakStatement : public Statement
{
public:
	BreakStatement() :
		Statement( StatementType::BREAK, nullptr )
	{}

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitBreakStatement( this ); }
};

class GotoStatement : public Statement
{
public:
	GotoStatement( Statement* target ) :
		Statement( StatementType::GOTO, nullptr ),
		target_( target )
	{
		if( !target )
			assert( false );
	}

	Statement* target() { return target_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitGotoStatement( this ); }
private:
	Statement* target_;
};