#include "code-writer.h"

CodeWriter::CodeWriter( SmxFile& smx, const char* function ) :
	smx_( &smx )
{
	func_ = smx_->FindFunctionByName( function );
}

std::string CodeWriter::Build( Statement* stmt )
{
	if( func_->is_public )
		code_ << "public ";
	code_ << BuildFuncDecl( func_->name, &func_->signature ) << '\n';
	code_ << "{\n";
	Indent();
	Visit( stmt );
	Dedent();
	code_ << "}\n";
	return code_.str();
}

void CodeWriter::VisitBasicStatement( BasicStatement * stmt )
{
	for( size_t node = 0; node < stmt->num_nodes(); node++ )
	{
		code_ << Tabs() << Build( stmt->node( node ) ) << ";\n";
	}
}

void CodeWriter::VisitIfStatement( IfStatement* stmt )
{
	bool old = in_else_if_;
	if( in_else_if_ )
		code_ << Tabs() << "else if (" << Build( stmt->condition() ) << ")\n";
	else
		code_ << Tabs() << "if (" << Build( stmt->condition() ) << ")\n";
	code_ << Tabs() << "{\n";
	Indent();
	in_else_if_ = false;
	Visit( stmt->then_branch() );
	Dedent();
	code_ << Tabs() << "}\n";
	if( stmt->else_branch() )
	{
		auto* else_if = dynamic_cast<IfStatement*>(stmt->else_branch());
		if( else_if &&
			(else_if->next() == nullptr || dynamic_cast<IfStatement*>(else_if->next())) )
		{
			in_else_if_ = true;
			Visit( else_if );
		}
		else
		{
			in_else_if_ = false;
			code_ << Tabs() << "else\n";
			code_ << Tabs() << "{\n";
			Indent();
			Visit( stmt->else_branch() );
			Dedent();
			code_ << Tabs() << "}\n";
		}
	}
	in_else_if_ = old;
}

void CodeWriter::VisitWhileStatement( WhileStatement* stmt )
{
	code_ << Tabs() << "while (" << Build( stmt->condition() ) << ")\n";
	code_ << Tabs() << "{\n";
	Indent();
	Visit( stmt->body() );
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
		code_ << Tabs() << "{\n";
		Indent();
		Visit( stmt->case_entry( i ).body );
		Dedent();
		code_ << Tabs() << "}\n";
	}

	if( stmt->default_case() )
	{
		code_ << Tabs() << "default:\n";
		code_ << Tabs() << "{\n";
		Indent();
		Visit( stmt->default_case() );
		Dedent();
		code_ << Tabs() << "}\n";
	}
	Dedent();
	code_ << Tabs() << "}\n";
}

void CodeWriter::VisitContinueStatement( ContinueStatement* stmt )
{
	code_ << Tabs() << "continue;\n";
}

void CodeWriter::VisitBreakStatement( BreakStatement* stmt )
{
	code_ << Tabs() << "break;\n";
}

void CodeWriter::VisitGotoStatement( GotoStatement* stmt )
{
	code_ << Tabs() << "goto " << stmt->target()->label() << ";\n";
}

void CodeWriter::VisitConst( ILConst* node )
{
	cell_t value = node->value();
	code_ << BuildTypedValue( &value, node->type() );
}

void CodeWriter::VisitUnary( ILUnary* node )
{
	switch( node->op() )
	{
	case ILUnary::FLOATNOT:
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

		case ILBinary::FLOATADD: code_ << Build( node->left() ) << " + " << Build( node->right() ); break;
		case ILBinary::FLOATSUB: code_ << Build( node->left() ) << " - " << Build( node->right() ); break;
		case ILBinary::FLOATMUL: code_ << Build( node->left() ) << " * " << Build( node->right() ); break;
		case ILBinary::FLOATDIV: code_ << Build( node->left() ) << " / " << Build( node->right() ); break;

		case ILBinary::FLOATCMP: code_ << Build( node->left() ) << " fcmp " << Build( node->right() ); break;
		case ILBinary::FLOATGT:  code_ << Build( node->left() ) << " > " << Build( node->right() ); break;
		case ILBinary::FLOATGE:  code_ << Build( node->left() ) << " >= " << Build( node->right() ); break;
		case ILBinary::FLOATLE:  code_ << Build( node->left() ) << " <= " << Build( node->right() ); break;
		case ILBinary::FLOATLT:  code_ << Build( node->left() ) << " < " << Build( node->right() ); break;
		case ILBinary::FLOATEQ:  code_ << Build( node->left() ) << " == " << Build( node->right() ); break;
		case ILBinary::FLOATNE:  code_ << Build( node->left() ) << " != " << Build( node->right() ); break;
	}
}

void CodeWriter::VisitLocalVar( ILLocalVar* node )
{
	std::string var_name;
	if( node->smx_var() )
	{
		var_name = node->smx_var()->name;
	}
	else
	{
		if( node->stack_offset() < 0 )
		{
			var_name = "local_" + std::to_string( -node->stack_offset() );
		}
		else
		{
			var_name = "arg" + std::to_string( (node->stack_offset() / 4) - 3 + 1 );
		}
	}

	if( level_ == 1 )
	{
		// This is a top level node, so it's a variable declaration
		code_ << BuildVarDecl( var_name, node->type() );

		if( node->value() )
			code_ << " = " << Build( node->value() );
	}
	else
	{
		// Variable is being referenced somewhere after already being declared
		// Just output the name
		code_ << var_name;
	}
}

void CodeWriter::VisitGlobalVar( ILGlobalVar* node )
{
	if( node->smx_var() )
	{
		code_ << node->smx_var()->name;
	}
	else
	{
		code_ << "global_" << node->addr();
	}
}

void CodeWriter::VisitHeapVar( ILHeapVar* node )
{
	code_ << "heap_" << node->addr();
	if( level_ == 1 )
		code_ << " = alloc(" << node->size() << ")";
}

void CodeWriter::VisitArrayElementVar( ILArrayElementVar* node )
{
	code_ << Build( node->base() ) << "[" << Build( node->index() ) << "]";
}

void CodeWriter::VisitTempVar( ILTempVar* node )
{
	std::string var_name = "tmp_" + std::to_string( node->index() );
	if( level_ == 1 )
	{
		code_ << BuildVarDecl( var_name, node->type() );
		if( node->value() )
			code_ << " = " << Build( node->value() );
	}
	else
	{
		code_ << var_name;
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
	SmxNative* native = smx_->FindNativeByIndex( node->native_index() );
	if( native )
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

void CodeWriter::Visit( Statement* stmt )
{
	do
	{
		if( stmt->label() )
		{
			Dedent();
			code_ << Tabs() << stmt->label() << ":\n";
			Indent();
		}

		stmt->Accept( this );
		stmt = stmt->next();
	} while( stmt != nullptr );
}

std::string CodeWriter::Build( ILNode* node )
{
	level_++;
	node->Accept( this );
	level_--;
	return "";
}

std::string CodeWriter::BuildVarDecl( const std::string& var_name, const SmxVariableType* type )
{
	std::ostringstream decl_str;

	if( !type )
	{
		// No type info, just assume int
		decl_str << "int " << var_name;
		return decl_str.str();
	}

	if( type->flags & SmxVariableType::IS_CONST )
	{
		decl_str << "const ";
	}

	switch( type->tag )
	{
		case SmxVariableType::UNKNOWN:
			decl_str << "<unknown>";
			break;
		case SmxVariableType::VOID:
			decl_str << "void";
			break;
		case SmxVariableType::BOOL:
			decl_str << "bool";
			break;
		case SmxVariableType::INT:
			decl_str << "int";
			break;
		case SmxVariableType::FLOAT:
			decl_str << "float";
			break;
		case SmxVariableType::CHAR:
			decl_str << "char";
			break;
		case SmxVariableType::ANY:
			decl_str << "any";
			break;
		case SmxVariableType::ENUM:
			decl_str << type->enumeration->name;
			break;
		case SmxVariableType::TYPEDEF:
			decl_str << type->type_def->name;
			break;
		case SmxVariableType::TYPESET:
			decl_str << type->type_set->name;
			break;
		case SmxVariableType::CLASSDEF:
			decl_str << type->classdef->name;
			break;
		case SmxVariableType::ENUM_STRUCT:
			decl_str << type->enum_struct->name;
			break;

		default:
			assert( !"Unhandled type" );
			decl_str << "int";
			break;
	}

	if( type->flags & SmxVariableType::BY_REF )
	{
		decl_str << '&';
	}

	decl_str << ' ' << var_name;

	for( int i = 0; i < type->dimcount; i++ )
	{
		decl_str << "[";
		if( type->dims[i] )
			decl_str << type->dims[i];
		decl_str << "]";
	}

	return decl_str.str();
}

std::string CodeWriter::BuildFuncDecl( const std::string& func_name, const SmxFunctionSignature* sig )
{
	std::ostringstream decl_str;

	if( !sig )
	{
		decl_str << "int " << func_name << "()";
		return decl_str.str();
	}

	if( !sig->ret )
	{
		decl_str << "int ";
	}
	else
	{
		decl_str << BuildVarDecl( "", sig->ret );
	}

	decl_str << func_name << '(';
	for( size_t i = 0; i < sig->nargs; i++ )
	{
		if( i != 0 )
			decl_str << ", ";
		if( sig->args[i].name )
		{
			decl_str << BuildVarDecl( sig->args[i].name, &sig->args[i].type );
		}
		else
		{
			decl_str << BuildVarDecl( "arg" + std::to_string( i + 1 ), &sig->args[i].type );
		}
	}
	decl_str << ')';

	return decl_str.str();
}

std::string CodeWriter::BuildTypedValue( cell_t* val, const SmxVariableType* type )
{
	if( !type )
		return std::to_string( *val );

	switch( type->tag )
	{
		case SmxVariableType::UNKNOWN:
		case SmxVariableType::INT:
		case SmxVariableType::ANY:
			assert( type->dimcount == 0 );
			return std::to_string( *val );

		case SmxVariableType::BOOL:
			assert( type->dimcount == 0 );
			return (*val) ? "true" : "false";

		case SmxVariableType::FLOAT:
		{
			assert( type->dimcount == 0 );

			union
			{
				cell_t c;
				float f;
			} x;

			x.c = *val;

			char buf[32];
			snprintf( buf, sizeof( buf ), "%g", x.f );
			return buf;
		}

		case SmxVariableType::CHAR:
		{
			if( type->dimcount == 0 )
			{
				return std::to_string( *val );
			}

			return std::string("\"") + (char*)smx_->data(*val) + "\"";
		}

		case SmxVariableType::ENUM:
		{
			// TODO: Figure out what the value corresponds to in the enum?
			assert( type->dimcount == 0 );
			return std::to_string( *val );
		}

		case SmxVariableType::TYPEDEF:
		case SmxVariableType::TYPESET:
		{
			assert( type->dimcount == 0 );
			if( *val == 0 )
				return "INVALID_FUNCTION";
			SmxFunction* func = smx_->FindFunctionById( *val );
			if( !func )
				return "<error>";
			return func->name;
		}

		default:
			assert( 0 );
			return std::to_string( *val );
	}
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

