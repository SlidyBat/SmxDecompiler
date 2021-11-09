#include <iostream>
#include <filesystem>
#include "optparse.h"
#include "smx-file.h"
#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"
#include "typer.h"
#include "code-fixer.h"
#include "structurizer.h"
#include "code-writer.h"

void DiscoverFunctions( SmxFile& smx, ControlFlowGraph& cfg )
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
				if( !smx.FindFunctionAt( func ) )
					smx.AddFunction( func );
			}

			instr = next_instr;
		}
	}
}

int main( int argc, const char* argv[] )
{
	OptParse args;
	args.AddArgOption( "function", 'f' )
		.AddFlagOption( "no-globals", 'g' )
		.AddFlagOption( "assembly", 'a' )
		.AddFlagOption( "il", 'i' );
	args.Process( argc, argv );

	if( args.GetArgC() < 1 )
	{
		std::cout << "Usage: "
			<< argv[0]
			<< " [--function/-f <function>] [--no-globals/-g] [--assembly/-a] [--il/-i] <filename>\n";
		return 1;
	}

	if( !std::filesystem::exists( args.GetArg( 0 ) ) )
	{
		std::cout << "Could not open file " << args.GetArg( 0 ) << std::endl;
		return 1;
	}

	SmxFile smx( args.GetArg( 0 ).c_str() );
	
	if( !args["no-globals"] )
	{
		for( size_t i = 0; i < smx.num_globals(); i++ )
		{
			SmxVariable& var = smx.global( i );
			CodeWriter writer( smx, nullptr );
			std::cout << writer.BuildVarDecl( var.name, &var.type ) << ";\n";
		}
		std::cout << std::endl;
	}

	for( size_t i = 0; i < smx.num_functions(); i++ )
	{
		SmxFunction& func = smx.function( i );
		if( args["function"] && func.name && strcmp( func.name, args["function"] ) != 0 )
			continue;

		if( args["assembly"] )
		{
			SmxDisassembler disasm( smx );
			std::cout << disasm.DisassembleFunction( func ).c_str() << std::endl;
		}

		CfgBuilder builder( smx );
		ControlFlowGraph cfg = builder.Build( smx.code( func.pcode_start ) );

		DiscoverFunctions( smx, cfg );

		PcodeLifter lifter( smx );
		ILControlFlowGraph* ilcfg = lifter.Lift( cfg );

		if( args["il"] )
		{
			ILDisassembler ildisasm( smx );
			std::cout << ildisasm.DisassembleCFG( *ilcfg );
		}

		Typer typer( smx );
		typer.PopulateTypes( *ilcfg );

		CodeFixer fixer( smx );
		for( int i = 0; i < 3; i++ )
		{
			typer.PopulateTypes( *ilcfg );
			fixer.ApplyFixes( *ilcfg );
			typer.PropagateTypes( *ilcfg );
		}

		Structurizer structurizer( ilcfg );
		Statement* func_stmt = structurizer.Transform();

		CodeWriter writer( smx, &func );
		std::string code = writer.Build( func_stmt );
		std::cout << code << std::endl;
	}

	return 0;
}