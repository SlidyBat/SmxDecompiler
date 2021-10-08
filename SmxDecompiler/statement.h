#pragma once

#include <vector>
#include "il.h"

enum class StatementType
{
	BASIC,
	SEQUENCE,
	IF,
	WHILE,
	DO_WHILE
};

class BasicStatement;
class SequenceStatement;
class IfStatement;
class WhileStatement;

class StatementVisitor
{
public:
	virtual void VisitBasicStatement( BasicStatement* stmt ) = 0;
	virtual void VisitSequenceStatement( SequenceStatement* stmt ) = 0;
	virtual void VisitIfStatement( IfStatement* stmt ) = 0;
	virtual void VisitWhileStatement( WhileStatement* stmt ) = 0;
};

class Statement
{
public:
	Statement( StatementType type ) : type_( type ) {}
	virtual ~Statement() = default;

	virtual void Accept( StatementVisitor* visitor ) = 0;

	StatementType type() const { return type_; }
private:
	StatementType type_;
};

class BasicStatement : public Statement
{
public:
	BasicStatement( ILBlock* block ) :
		Statement( StatementType::BASIC ),
		block_( block )
	{}

	ILBlock* block() { return block_; }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitBasicStatement( this ); }
private:
	ILBlock* block_;
};

class SequenceStatement : public Statement
{
public:
	SequenceStatement( std::vector<Statement*> statements ) :
		Statement( StatementType::SEQUENCE ),
		statements_( std::move( statements ) )
	{}

	Statement* statement( size_t index ) { return statements_[index]; }
	size_t num_statements() const { return statements_.size(); }

	virtual void Accept( StatementVisitor* visitor ) override { visitor->VisitSequenceStatement( this ); }
private:
	std::vector<Statement*> statements_;
};

class IfStatement : public Statement
{
public:
	IfStatement( ILNode* condition, Statement* then_branch, Statement* else_branch ) :
		Statement( StatementType::IF ),
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

class WhileStatement : public Statement
{
public:
	WhileStatement( ILNode* condition, Statement* body ) :
		Statement( StatementType::WHILE ),
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