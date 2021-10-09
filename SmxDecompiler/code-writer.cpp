#include "code-writer.h"

CodeWriter::CodeWriter( SmxFile& smx, const char* function ) :
	smx_( &smx )
{
	func_ = smx_->FindFunctionByName( function );
}

std::string CodeWriter::Build( Statement* stmt )
{
	code_ << "public int " << func_->name << "()\n";
	code_ << "{\n";
	Indent();
	stmt->Accept( this );
	Dedent();
	code_ << "}\n";
	return code_.str();
}

void CodeWriter::VisitBasicStatement( BasicStatement * stmt )
{
	Build( stmt->block() );
}

void CodeWriter::VisitSequenceStatement( SequenceStatement* stmt )
{
	for( size_t i = 0; i < stmt->num_statements(); i++ )
	{
		stmt->statement( i )->Accept( this );
	}
}

void CodeWriter::VisitIfStatement( IfStatement* stmt )
{
	code_ << Tabs() << "if (" << Build( stmt->condition() ) << ")\n";
	code_ << Tabs() << "{\n";
	Indent();
	stmt->then_branch()->Accept( this );
	Dedent();
	code_ << Tabs() << "}\n";
	if( stmt->else_branch() )
	{
		code_ << Tabs() << "else\n";
		code_ << Tabs() << "{\n";
		Indent();
		stmt->else_branch()->Accept( this );
		Dedent();
		code_ << Tabs() << "}\n";
	}
}

void CodeWriter::VisitWhileStatement( WhileStatement* stmt )
{
	code_ << Tabs() << "while (" << Build( stmt->condition() ) << ")\n";
	code_ << Tabs() << "{\n";
	Indent();
	stmt->body()->Accept( this );
	Dedent();
	code_ << Tabs() << "}\n";
}

void CodeWriter::VisitSwitchStatement( SwitchStatement* stmt )
{
	code_ << Tabs() << "switch (" << Build( stmt->value() ) << ")\n";
	code_ << Tabs() << "{\n";
	Indent();
	for( size_t i = 0; i < stmt->num_cases(); i++ )
	{
		code_ << Tabs() << "case " << stmt->case_entry( i ).value << ":\n";
		Indent();
		stmt->case_entry( i ).body->Accept( this );
		Dedent();
	}

	if( stmt->default_case() )
	{
		code_ << Tabs() << "default:\n";
		Indent();
		stmt->default_case()->Accept( this );
		Dedent();
	}
	Dedent();
	code_ << Tabs() << "}\n";
}

void CodeWriter::VisitConst( ILConst* node )
{
	code_ << node->value();
}

void CodeWriter::VisitUnary( ILUnary* node )
{
	switch( node->op() )
	{
	case ILUnary::NOT:
		code_ << "!" << Build( node->val() );
		break;
	case ILUnary::NEG:
		code_ << "-" << Build( node->val() );
		break;
	case ILUnary::INVERT:
		code_ << "~" << Build( node->val() );
		break;

	case ILUnary::FABS:
	case ILUnary::FLOAT:
	case ILUnary::FLOATNOT:
	case ILUnary::RND_TO_NEAREST:
	case ILUnary::RND_TO_CEIL:
	case ILUnary::RND_TO_ZERO:
	case ILUnary::RND_TO_FLOOR:
		code_ << "<err>";
		break;

	case ILUnary::INC:
		code_ << "++" << Build( node->val() );
		break;
	case ILUnary::DEC:
		code_ << "--" << Build( node->val() );
		break;
	}
}

void CodeWriter::VisitBinary( ILBinary* node )
{
	switch( node->op() )
	{
		case ILBinary::ADD:      code_ << Build( node->left() ) << " + " << Build( node->right() ); break;
		case ILBinary::SUB:      code_ << Build( node->left() ) << " - " << Build( node->right() ); break;
		case ILBinary::DIV:      code_ << Build( node->left() ) << " / " << Build( node->right() ); break;
		case ILBinary::MUL:      code_ << Build( node->left() ) << " * " << Build( node->right() ); break;
		case ILBinary::MOD:      code_ << Build( node->left() ) << " % " << Build( node->right() ); break;
		case ILBinary::SHL:      code_ << Build( node->left() ) << " << " << Build( node->right() ); break;
		case ILBinary::SHR:      code_ << Build( node->left() ) << " >> " << Build( node->right() ); break;
		case ILBinary::SSHR:     code_ << Build( node->left() ) << " >> " << Build( node->right() ); break;
		case ILBinary::AND:      code_ << Build( node->left() ) << " & " << Build( node->right() ); break;
		case ILBinary::OR:       code_ << Build( node->left() ) << " | " << Build( node->right() ); break;
		case ILBinary::XOR:      code_ << Build( node->left() ) << " ^ " << Build( node->right() ); break;

		case ILBinary::EQ:       code_ << Build( node->left() ) << " == " << Build( node->right() ); break;
		case ILBinary::NEQ:      code_ << Build( node->left() ) << " != " << Build( node->right() ); break;
		case ILBinary::SGRTR:    code_ << Build( node->left() ) << " > " << Build( node->right() ); break;
		case ILBinary::SGEQ:     code_ << Build( node->left() ) << " >= " << Build( node->right() ); break;
		case ILBinary::SLESS:    code_ << Build( node->left() ) << " < " << Build( node->right() ); break;
		case ILBinary::SLEQ:     code_ << Build( node->left() ) << " <= " << Build( node->right() ); break;

		case ILBinary::FLOATADD: code_ << Build( node->left() ) << " f+ " << Build( node->right() ); break;
		case ILBinary::FLOATSUB: code_ << Build( node->left() ) << " f- " << Build( node->right() ); break;
		case ILBinary::FLOATMUL: code_ << Build( node->left() ) << " f* " << Build( node->right() ); break;
		case ILBinary::FLOATDIV: code_ << Build( node->left() ) << " f/ " << Build( node->right() ); break;

		case ILBinary::FLOATCMP: code_ << Build( node->left() ) << " fcmp " << Build( node->right() ); break;
		case ILBinary::FLOATGT:  code_ << Build( node->left() ) << " f> " << Build( node->right() ); break;
		case ILBinary::FLOATGE:  code_ << Build( node->left() ) << " f>= " << Build( node->right() ); break;
		case ILBinary::FLOATLE:  code_ << Build( node->left() ) << " f<= " << Build( node->right() ); break;
		case ILBinary::FLOATLT:  code_ << Build( node->left() ) << " f< " << Build( node->right() ); break;
		case ILBinary::FLOATEQ:  code_ << Build( node->left() ) << " f== " << Build( node->right() ); break;
		case ILBinary::FLOATNE:  code_ << Build( node->left() ) << " f!= " << Build( node->right() ); break;
	}
}

void CodeWriter::VisitLocalVar( ILLocalVar* node )
{
	bool found_name = false;
	if( func_ )
	{
		for( size_t i = 0; i < func_->num_locals; i++ )
		{
			if( func_->locals[i].address == node->stack_offset() )
			{
				if( level_ == 1 )
					code_ << Type( func_->locals[i].type ) << ' ';
				code_ << func_->locals[i].name;
				found_name = true;
				break;
			}
		}
	}

	if( !found_name )
	{
		if( level_ == 1 )
			code_ << Type( node ) << ' ';
		code_ << "local_" << -node->stack_offset();
	}

	if( node->value() && level_ == 1 )
	{
		code_ << " = " << Build( node->value() );
	}
}

void CodeWriter::VisitGlobalVar( ILGlobalVar* node )
{
	if( SmxVariable* var = smx_->FindGlobalAt( node->addr() ) )
	{
		code_ << var->name;
	}
	else
	{
		code_ << "global_" << node->addr();
	}
}

void CodeWriter::VisitHeapVar( ILHeapVar* node )
{
	code_ << "heap_" << node->size();
}

void CodeWriter::VisitArrayElementVar( ILArrayElementVar* node )
{
	code_ << Build( node->base() ) << "[" << Build( node->index() ) << "]";
}

void CodeWriter::VisitTempVar( ILTempVar* node )
{
	code_ << Type( node ) << " tmp_" << node->index();
	if( node->value() && level_ == 1 )
	{
		code_ << " = " << Build( node->value() );
	}
}

void CodeWriter::VisitLoad( ILLoad* node )
{
	code_ << Build( node->var() );
}

void CodeWriter::VisitStore( ILStore* node )
{
	code_ << Build( node->var() ) << " = " << Build( node->val() );
}

void CodeWriter::VisitJump( ILJump* node )
{
}

void CodeWriter::VisitJumpCond( ILJumpCond* node )
{
}

void CodeWriter::VisitCall( ILCall* node )
{
	if( SmxFunction* func = smx_->FindFunctionAt( node->addr() ) )
	{
		code_ << func->name;
	}
	else
	{
		code_ << "func_" << node->addr();
	}

	code_ << "(";
	for( size_t i = 0; i < node->num_args(); i++ )
	{
		code_ << Build( node->arg( i ) );
		if( i != node->num_args() - 1 )
		{
			code_ << ", ";
		}
	}
	code_ << ")";
}

void CodeWriter::VisitNative( ILNative* node )
{
	if( SmxNative* native = smx_->FindNativeByIndex( node->native_index() ) )
	{
		code_ << native->name;
	}
	else
	{
		code_ << "native_" << node->native_index();
	}

	code_ << "(";
	for( size_t i = 0; i < node->num_args(); i++ )
	{
		code_ << Build( node->arg( i ) );
		if( i != node->num_args() - 1 )
		{
			code_ << ", ";
		}
	}
	code_ << ")";
}

void CodeWriter::VisitReturn( ILReturn* node )
{

	code_ << "return";
	if( node->value() )
		code_ << " " << Build(node->value());
}

void CodeWriter::VisitPhi( ILPhi* node )
{
	assert( false );
}

void CodeWriter::VisitInterval( ILInterval* node )
{
	assert( false );
}

std::string CodeWriter::Build( ILBlock* block )
{
	for( size_t node = 0; node < block->num_nodes(); node++ )
	{
		if( dynamic_cast<ILJump*>( block->node( node ) ) || dynamic_cast<ILJumpCond*>( block->node( node ) ) )
		{
			
		}
		else
		{
			code_ << Tabs() << Build( block->node( node ) ) << ";\n";
		}
	}
	return "";
}

std::string CodeWriter::Build( ILNode* node )
{
	level_++;
	node->Accept( this );
	level_--;
	return "";
}

std::string CodeWriter::Type( const SmxVariableType& type )
{
	std::stringstream type_str;
	if( type.is_const )
	{
		type_str << "const ";
	}

	switch( type.tag )
	{
		case SmxVariableType::BOOL:
			type_str << "bool";
			break;
		case SmxVariableType::INT:
			type_str << "int";
			break;
		case SmxVariableType::FLOAT:
			type_str << "float";
			break;
		case SmxVariableType::CHAR:
			type_str << "char";
			break;
		case SmxVariableType::ANY:
			type_str << "any";
			break;
	}

	for( int i = 0; i < type.dimcount; i++ )
	{
		type_str << "[";
		if( type.dims[i] )
			type_str << type.dims[i];
		type_str << "]";
	}

	return type_str.str();
}

const char* CodeWriter::Type( ILVar* var )
{
	switch( var->type() )
	{
		case ILType::UNKNOWN:
		case ILType::INT:
			return "int";
		case ILType::FLOAT:
			return "float";
		case ILType::STRING:
			return "char[]";
	}

	return "<err>";
}

std::string CodeWriter::Tabs()
{
	std::string tabs;
	for( int i = 0; i < indent_; i++ )
		tabs += "  ";
	return tabs;
}

void CodeWriter::Indent()
{
	indent_++;
}

void CodeWriter::Dedent()
{
	indent_--;
}

