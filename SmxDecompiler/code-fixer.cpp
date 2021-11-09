#include "code-fixer.h"

#include "il.h"

// Multidim arrays are accessed a bit oddly
// The compiler will generate "indirection vectors" for the first dimension
// of the array, which contains an offset to the actual data. This pass will
// try to fix that up so that they're accessed as normally expected.
//
// Before:
//  `(arr[x] + &arr[x])[y] = z`
// After:
//  `arr[x][y] = z`
//
class FixMultidimArrays : public RecursiveILVisitor
{
public:
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		RecursiveILVisitor::VisitArrayElementVar( node );

		ILNode* base;
		ILNode* index;
		if( !GetBaseAndIndex( node->base(), &base, &index ) )
			return;

		if( auto* load = dynamic_cast<ILLoad*>( base ) )
			base = load->var();
		if( auto* load = dynamic_cast<ILLoad*>( index ) )
			index = load->var();

		auto* iv_val = dynamic_cast<ILArrayElementVar*>( base );
		if( !iv_val )
			return;

		ILNode* arr = index;
		GetBaseAndIndex( arr, &arr, nullptr );

		if( !AreEquivalentAddresses( iv_val->base(), arr ) )
			return;

		node->base()->ReplaceUsesWith( iv_val );
	}
private:
	bool GetBaseAndIndex( ILNode* node, ILNode** base, ILNode** index )
	{
		if( auto* binary = dynamic_cast<ILBinary*>( node ) )
		{
			if( binary->op() != ILBinary::ADD )
				return false;
			if( base )
				*base = binary->left();
			if( index )
				*index = binary->right();
			return true;
		}
		else if( auto* arr = dynamic_cast<ILArrayElementVar*>( node ) )
		{
			if( base )
				*base = arr->base();
			if( index )
				*index = arr->index();
			return true;
		}

		return false;
	}
	bool AreEquivalentAddresses( ILNode* a, ILNode* b )
	{
		cell_t addr_a, addr_b;
		if( GetEffectiveAddress( a, &addr_a ) && GetEffectiveAddress( b, &addr_b ) )
		{
			return addr_a == addr_b;
		}
		return false;
	}
	bool GetEffectiveAddress( ILNode* node, cell_t* addr )
	{
		if( auto* global = dynamic_cast<ILGlobalVar*>( node ) )
		{
			*addr = global->addr();
			return true;
		}
		if( auto* constant = dynamic_cast<ILConst*>( node ) )
		{
			*addr = constant->value();
			return true;
		}
		if( auto* local = dynamic_cast<ILLocalVar*>( node ) )
		{
			return local->stack_offset();
		}
		if( auto* heap = dynamic_cast<ILHeapVar*>( node ) )
		{
			return heap->addr();
		}
		if( auto* tmp = dynamic_cast<ILTempVar*>( node ) )
		{
			return tmp->index();
		}

		assert( !"Unhandled variable type" );
		return false;
	}
};

// Sometimes globals are referenced by their constant address.
// To the lifter, this looks like just a regular constant, but
// it should be interpreted as a variable.
// 
// Before:
//  `2332[i] = 0`
// After:
//  `g_var[i] = 0`
// 
class FixConstGlobals : public RecursiveILVisitor
{
public:
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		RecursiveILVisitor::VisitArrayElementVar( node );

		if( auto* constant = dynamic_cast<ILConst*>(node->base()) )
		{
			auto* var = new ILGlobalVar( constant->value() );
			constant->ReplaceUsesWith( var );
		}
	}
};

// When arrays/enum-structs are loaded from / stored into at offset 0, there is no offset operation.
// This class fixes up those cases to add in the offset into first element. It also fixes
// up cases where addition is used to index into an array.
// 
// Before:
//  `arr = x`
//  `x = arr`
//  `PrintToServer("%s", arr + i)`
// After:
//  `arr[0] = x`
//  `x = arr[0]`
//  `PrintToServer("%s", arr[i])`
//
class FixArrays : public RecursiveILVisitor
{
public:
	virtual void VisitLoad( ILLoad* node ) override
	{
		RecursiveILVisitor::VisitLoad( node );

		const SmxVariableType* type = node->var()->type();
		
		// Not much we can do without type information at this stage, just bail
		if( !type )
			return;

		// Not an array/enum-struct or already correct operation, nothing to fix
		if( !IsArrayOrEnumStructType( type ) || IsArrayOrEnumStructVar( node->var() ) )
			return;

		ILVar* var = node->var();
		if( var->smx_var() && var->smx_var()->vclass == SmxVariableClass::ARG )
		{
			node->ReplaceUsesWith( var );
			return;
		}

		ILVar* new_var = nullptr;
		if( type->dimcount > 0 )
		{
			// Array case
			new_var = new ILArrayElementVar( node->var(), new ILConst( 0 ) );
		}
		else
		{
			// Enum struct case
			SmxEnumStruct* enum_struct = type->enum_struct;
			new_var = new ILFieldVar( node->var(), 0, enum_struct->FindFieldAtOffset( 0 ) );
		}

		node->ReplaceParam( node->var(), new_var );
	}
	virtual void VisitStore( ILStore* node ) override
	{
		RecursiveILVisitor::VisitStore( node );

		const SmxVariableType* type = node->var()->type();

		// Not much we can do without type information at this stage, just bail
		if( !type )
			return;

		// Not an array/enum-struct or already correct operation, nothing to fix
		if( !IsArrayOrEnumStructType( type ) || IsArrayOrEnumStructVar( node->var() ) )
			return;

		ILVar* new_var = nullptr;
		if( type->dimcount > 0 )
		{
			// Array case
			new_var = new ILArrayElementVar( node->var(), new ILConst( 0 ) );
		}
		else
		{
			// Enum struct case
			SmxEnumStruct* enum_struct = type->enum_struct;
			new_var = new ILFieldVar( node->var(), 0, enum_struct->FindFieldAtOffset( 0 ) );
		}

		node->ReplaceParam( node->var(), new_var );
	}
	virtual void VisitBinary( ILBinary* node ) override
	{
		RecursiveILVisitor::VisitBinary( node );

		if( node->op() == ILBinary::ADD )
		{
			ILVar* base = nullptr;
			ILNode* index = nullptr;
			if( ILVar* var = dynamic_cast<ILVar*>(node->left()) )
			{
				if( var->type() && var->type()->dimcount > 0 )
				{
					base = var;
					index = node->right();
				}
			}
			if( ILVar* var = dynamic_cast<ILVar*>(node->right()) )
			{
				if( var->type() && var->type()->dimcount > 0 )
				{
					base = var;
					index = node->left();
				}
			}

			if( !index || !base )
				return;

			node->ReplaceUsesWith( new ILArrayElementVar( base, index ) );
		}
	}
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		const SmxVariableType* base_type = node->base()->type();
		const SmxVariableType* index_type = node->index()->type();

		if( !base_type || base_type->dimcount > 0 )
			return;
		if( !index_type || index_type->dimcount == 0 )
			return;

		auto* swapped = new ILArrayElementVar( node->index(), node->base() );
		node->ReplaceUsesWith( swapped );

		RecursiveILVisitor::VisitArrayElementVar( swapped );
	}
private:
	bool IsArrayOrEnumStructType( const SmxVariableType* type )
	{
		return ( type->dimcount > 0 ) || ( type->tag == SmxVariableType::ENUM_STRUCT );
	}
	bool IsArrayOrEnumStructVar( ILVar* var )
	{
		return dynamic_cast<ILArrayElementVar*>( var ) || dynamic_cast<ILFieldVar*>( var );
	}
};

// Some float operations are implemented via natives rather than actual instructions.
// This replaces those natives with actual instructions.
// 
// Before:
//  `c = FloatAdd(a, b)`
// After:
//  `c = a + b`
//
class ReplaceFloatNatives : public RecursiveILVisitor
{
public:
	ReplaceFloatNatives( SmxFile& smx ) : smx_( &smx ) {}

	virtual void VisitNative( ILNative* node ) override
	{
		// Handle the args of this call, they can contain native calls that we want to replace too
		RecursiveILVisitor::VisitNative( node );

		SmxNative* native = smx_->FindNativeByIndex( node->native_index() );
		assert( native );

		if( strcmp( native->name, "FloatMul" ) == 0 || strcmp( native->name, "__FLOAT_MUL__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATMUL, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatDiv" ) == 0 || strcmp( native->name, "__FLOAT_DIV__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATDIV, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatAdd" ) == 0 || strcmp( native->name, "__FLOAT_ADD__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATADD, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatSub" ) == 0 || strcmp( native->name, "__FLOAT_SUB__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATSUB, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_NOT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILUnary( node->arg( 0 ), ILUnary::FLOATNOT ) );
		}
		else if( strcmp( native->name, "__FLOAT_GT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATGT, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_GE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATGE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_LT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATLT, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_LE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATLE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_NE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATNE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_EQ__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATEQ, node->arg( 1 ) ) );
		}
	}
private:
	SmxFile* smx_;
};

// If the function has a void return type, then return statements shouldn't have value
// 
// Before:
//  `return x`
// After:
//  `return`
//
class RemoveVoidRets : public ILVisitor
{
public:
	void VisitReturn( ILReturn* node )
	{
		if( node->value() )
			node->ReplaceParam( node->value(), nullptr );
	}
};

// Bool operations are always represented with comparisons to zero, which should be implicit anyways
//
// Before:
//  `if ((x == 2) != 0) {}`
//  `if ((x > 0) == 0) {}`
// After:
//  `if (x == 2) {}`
//  `if (!(x > 0)) {}`
//
class UseBoolOps : public RecursiveILVisitor
{
public:
	void VisitBinary( ILBinary* node )
	{
		RecursiveILVisitor::VisitBinary( node );

		if( node->op() != ILBinary::EQ && node->op() != ILBinary::NEQ )
			return;

		if( !(node->left()->type() && node->left()->type()->tag == SmxVariableType::BOOL) )
			return;

		if( auto* constant = dynamic_cast<ILConst*>(node->right()) )
		{
			if( constant->value() != 0 )
				return;

			if( node->op() == ILBinary::EQ )
			{
				node->ReplaceUsesWith( new ILUnary( node->left(), ILUnary::NOT ) );
			}
			else
			{
				node->ReplaceUsesWith( node->left() );
			}
		}
	}
};

void CodeFixer::ApplyFixes( ILControlFlowGraph& cfg ) const
{
	FixArrays arrays;
	VisitAllNodes( cfg, arrays );
	
	FixMultidimArrays multidim_arrays;
	VisitAllNodes( cfg, multidim_arrays );

	FixConstGlobals fix_const_globals;
	VisitAllNodes( cfg, fix_const_globals );

	ReplaceFloatNatives replace_float_natives( *smx_ );
	VisitAllNodes( cfg, replace_float_natives );

	SmxFunction* func = smx_->FindFunctionAt( cfg.Entry().pc() );
	if( func->signature.ret && func->signature.ret->tag == SmxVariableType::VOID )
	{
		RemoveVoidRets remove_void_rets;
		VisitAllNodes( cfg, remove_void_rets );
	}

	UseBoolOps use_bool_ops;
	VisitAllNodes( cfg, use_bool_ops );

	for( int i = (int)cfg.num_blocks() - 1; i >= 0; i-- )
	{
		CleanStores( cfg.block( i ) );
		CleanIncAndDec( cfg.block( i ) );
		RemoveTmpLocalVars( cfg.block( i ) );
	}

	for( int i = (int)cfg.num_blocks() - 1; i >= 0; i-- )
	{
		FixShortCircuitConditions( cfg, cfg.block( i ) );
	}
	cfg.ComputeDominance();

	for( int i = (int)cfg.num_blocks() - 1; i >= 0; i-- )
	{
		FixArrayAndESDecl( cfg.block( i ) );
	}
}

void CodeFixer::VisitAllNodes( ILControlFlowGraph& cfg, ILVisitor& visitor ) const
{
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		ILBlock& bb = cfg.block( i );
		for( size_t node = 0; node < bb.num_nodes(); node++ )
		{
			bb.node( node )->Accept( &visitor );
		}
	}
}

void CodeFixer::CleanStores( ILBlock& bb ) const
{
	// There is a common pattern of declaring a local variable then storing a value into it
	// e.g.
	// ```
	// float x;
	// x = a + b;
	// ```
	//
	// We can clean this up by removing the store and placing its value into the declaration
	// e.g.
	// ```
	// float x = a + b;
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 1; i-- )
	{
		if( auto* store = dynamic_cast<ILStore*>(bb.node( i )) )
		{
			if( auto* store_var = dynamic_cast<ILLocalVar*>(store->var()) )
			{
				if( auto* decl_var = dynamic_cast<ILLocalVar*>(bb.node( i - 1 )) )
				{
					if( store_var == decl_var && decl_var->value() == nullptr )
					{
						decl_var->SetValue( store->val() );
						bb.Remove( i );
					}
				}
			}
		}
	}
}

void CodeFixer::CleanIncAndDec( ILBlock& bb ) const
{
	// INC/DEC instructions should not be a part of a store since the
	// store is implicit, however in the pcode the store is explicit
	//
	// This results in code that looks like this:
	// `i = ++i`
	// 
	// When we actually want:
	// `++i`
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* store = dynamic_cast<ILStore*>(bb.node( i )) )
		{
			if( auto* unary = dynamic_cast<ILUnary*>(store->val()) )
			{
				if( unary->op() == ILUnary::INC || unary->op() == ILUnary::DEC )
				{
					bb.Replace( i, unary );
				}
			}
		}
	}
}

void CodeFixer::RemoveTmpLocalVars( ILBlock& bb ) const
{
	// Sometimes local vars are used to store result from other var.
	// This pass will optimize out those local vars and use the result immediately.
	// Before:
	//  `int local_3212 = x;`
	//  `PrintToServer("%i", local_3212);`
	// After:
	//  `PrintToServer("%i", x);`
	// 
	// This could result in optimizing out code that was part of original source, but
	// that shouldn't affect readability of the decompilation. As a way to try avoiding
	// this case, only variables that don't have any debug info associated with them are
	// removed.
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* local_var = dynamic_cast<ILLocalVar*>(bb.node( i )) )
		{
			if( local_var->smx_var() )
				continue;

			if( !local_var->value() || local_var->num_uses() > 1 )
				continue;

			local_var->ReplaceUsesWith( local_var->value() );
			bb.Remove( i );
		}
		else if( auto* tmp_var = dynamic_cast<ILTempVar*>(bb.node( i )) )
		{
			if( tmp_var->smx_var() )
				continue;

			if( !tmp_var->value() || tmp_var->num_uses() > 1 )
				continue;

			tmp_var->ReplaceUsesWith( tmp_var->value() );
			bb.Remove( i );
		}
	}
}

void CodeFixer::FixArrayAndESDecl( ILBlock& bb ) const
{
	// Often times the lifter will give us a local var with an attached value instead of a store.
	// Most of the time this is good since it combines the declaration/initialization, but for
	// enum structs or arrays this is undesirable. We want it to be a separate store on the actual
	// element being accessed.
	//
	// Before:
	//  ```
	//  MyEnumStruct x = 5;
	// 	int y[10] = 10;
	// 	```
	// After:
	// 	```
	//  MyEnumStruct x;
	// 	x.field_0 = 5;
	// 	int y[10];
	// 	y[0] = 10;
	// 	```
	//
	for( size_t i = 0; i < bb.num_nodes(); i++ )
	{
		if( auto* local_var = dynamic_cast<ILLocalVar*>( bb.node( i ) ) )
		{
			if( !local_var->value() )
				continue;

			if( !local_var->type() )
				continue;

			if( !( local_var->type()->dimcount > 0 || local_var->type()->tag == SmxVariableType::ENUM_STRUCT ) )
				continue;

			ILVar* new_var = nullptr;
			if( local_var->type()->dimcount > 0 )
			{
				// Array case
				new_var = new ILArrayElementVar( local_var, new ILConst( 0 ) );
			}
			else
			{
				// Enum struct case
				SmxEnumStruct* enum_struct = local_var->type()->enum_struct;
				new_var = new ILFieldVar( local_var, 0, enum_struct->FindFieldAtOffset( 0 ) );
			}

			ILNode* value = local_var->value();
			local_var->ReplaceParam( value, nullptr );
			bb.Insert( i + 1, new ILStore( new_var, value ) );
		}
	}
}

void CodeFixer::FixShortCircuitConditions( ILControlFlowGraph& cfg, ILBlock& bb ) const
{
	// Short circuit conditions (&&/||) generate code that assigns to some tmp var, then
	// checks the tmp var to actually run the user code. This pass removes the tmp var
	// and uses the condition directly.
	// 
	// Before:
	//  ```
	//  if (a && b && c)
	//  {
	//    tmp = 1
	//  }
	//  else
	//  {
	//    tmp = 0
	//  }
	//  if (tmp)
	//  {
	//    // user code
	//  }
	//  ```
	// After:
	//  ```
	//  if (a && b && c)
	//  {
	//    // user code
	//  }
	//  ```
	//
	auto* node = dynamic_cast<ILJumpCond*>(bb.Last());
	if( !node )
		return;

	ILBlock* then_branch = node->true_branch();
	ILBlock* else_branch = node->false_branch();
	if( then_branch->num_nodes() != 1 ||
		else_branch->num_nodes() != 2 ||
		then_branch->num_in_edges() != 1 ||
		else_branch->num_in_edges() != 1 )
		return;

	auto* jmp = dynamic_cast<ILJump*>(else_branch->Last());
	if( !jmp )
		return;

	auto* then_store = dynamic_cast<ILStore*>(then_branch->node( 0 ));
	auto* else_store = dynamic_cast<ILStore*>(else_branch->node( 0 ));
	if( !then_store || !else_store || then_store->var() != else_store->var() )
		return;

	auto* tmp = then_store->var();

	auto* then_const = dynamic_cast<ILConst*>(then_store->val());
	auto* else_const = dynamic_cast<ILConst*>(else_store->val());
	if( !then_const || !else_const )
		return;

	cell_t then_val = then_const->value();
	cell_t else_val = else_const->value();
	if( then_val != !else_val )
		return;

	if( then_val == 0 )
	{
		node->Invert();
	}

	// This is the "real" block that uses the tmp var result
	auto* real_cond_block = jmp->target();

	// Remove entire if-statement, just use bool result inline
	real_cond_block->RemoveInEdge( bb );
	real_cond_block->RemoveInEdge( *then_branch );
	real_cond_block->RemoveInEdge( *else_branch );
	for( size_t i = 0; i < bb.num_in_edges(); i++ )
	{
		bb.in_edge( i ).ReplaceOutEdge( bb, *real_cond_block );
		real_cond_block->AddInEdge( bb.in_edge( i ) );
	}

	for( int i = (int)bb.num_nodes() - 2; i >= 0; i-- )
	{
		real_cond_block->AddToStart( bb.node( i ) );
	}

	ILBlock* remove[] = { &bb, then_branch, else_branch };
	cfg.RemoveMultiple( remove, 3 );

	// Remove unnecessary references to tmp
	real_cond_block->Remove( tmp );
	for( int i = (int)tmp->num_uses() - 1; i >= 0; i-- )
	{
		if( auto* store = dynamic_cast<ILStore*>( tmp->use( i ) ) )
		{
			tmp->RemoveUse( i );
			store->ReplaceUsesWith( node->condition() );
		}
		else if( auto* load = dynamic_cast<ILLoad*>( tmp->use( i ) ) )
		{
			tmp->RemoveUse( i );
			load->ReplaceUsesWith( node->condition() );
		}
	}

	tmp->ReplaceUsesWith( node->condition() );
}
