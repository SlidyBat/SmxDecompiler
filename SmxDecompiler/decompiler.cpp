#include "decompiler.h"

#include <iostream>

#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"
#include "typer.h"
#include "code-fixer.h"
#include "structurizer.h"
#include "code-writer.h"

Decompiler::Decompiler( SmxFile& smx, const DecompilerOptions& options ) :
	smx_( &smx ),
	options_( options )
{}

void Decompiler::Print()
{
	if( options_.print_globals )
	{
		for( size_t i = 0; i < smx_->num_globals(); i++ )
		{
			SmxVariable& var = smx_->global( i );
			CodeWriter writer( *smx_, nullptr );
			std::cout << writer.BuildVarDecl( var.name, &var.type ) << ";\n";
		}
		std::cout << std::endl;
	}

	for( size_t i = 0; i < smx_->num_functions(); i++ )
	{
		SmxFunction& func = smx_->function( i );
		if( options_.function && func.name && strcmp( func.name, options_.function ) != 0 )
			continue;

		if( options_.print_assembly )
		{
			SmxDisassembler disasm( *smx_ );
			std::cout << disasm.DisassembleFunction( func ).c_str() << std::endl;
		}

		CfgBuilder builder( *smx_ );
		ControlFlowGraph cfg = builder.Build( smx_->code( func.pcode_start ) );

		DiscoverFunctions( cfg );

		PcodeLifter lifter( *smx_ );
		ILControlFlowGraph* ilcfg = lifter.Lift( cfg );

		if( options_.print_il )
		{
			ILDisassembler ildisasm( *smx_ );
			std::cout << ildisasm.DisassembleCFG( *ilcfg );
		}

		Typer typer( *smx_ );
		typer.PopulateTypes( *ilcfg );

		CodeFixer fixer( *smx_ );
		for( int i = 0; i < 3; i++ )
		{
			typer.PopulateTypes( *ilcfg );
			fixer.ApplyFixes( *ilcfg );
			typer.PropagateTypes( *ilcfg );
		}

		Structurizer structurizer( ilcfg );
		Statement* func_stmt = structurizer.Transform();

		CodeWriter writer( *smx_, &func );
		std::string code = writer.Build( func_stmt );
		std::cout << code << std::endl;
	}
}

void Decompiler::DiscoverFunctions( ControlFlowGraph& cfg )
{
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		BasicBlock& bb = cfg.block( i );
		const cell_t* instr = bb.start();
		while( instr < bb.end() )
		{
			auto op = (SmxOpcode)instr[0];
			const cell_t* params = instr + 1;
			const auto& info = SmxInstrInfo::Get( op );
			const cell_t* next_instr = instr + info.num_params + 1;

			if( op == SMX_OP_CALL )
			{
				cell_t func = params[0];
				if( !smx_->FindFunctionAt( func ) )
					smx_->AddFunction( func );
			}

			instr = next_instr;
		}
	}
}
