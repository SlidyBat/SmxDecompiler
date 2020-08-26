#include "smx-file.h"
#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	const SmxFunction& func = smx.function( 0x13 );
	printf( "Function %s\n", func.name );
	puts( SmxDisassembler::DisassembleFunction( func ).c_str() );

	CfgBuilder builder( smx );
	ControlFlowGraph cfg = builder.Build( func.pcode_start );
	//for( size_t i = 0; i < cfg.num_blocks(); i++ )
	//{
	//	const BasicBlock& bb = cfg.block( i );
	//	std::string disasm = SmxDisassembler::DisassembleBlock( bb );
	//	printf( "===== BB%i =====\n", bb.id() );
	//	for( size_t in = 0; in < bb.num_in_edges(); in++ )
	//	{
	//		printf( "> Incoming edge: BB%i\n", bb.in_edge( in )->id() );
	//	}
	//	for( size_t out = 0; out < bb.num_out_edges(); out++ )
	//	{
	//		printf( "> Outgoing edge: BB%i\n", bb.out_edge( out )->id() );
	//	}
	//	printf( "%s\n", disasm.c_str() );
	//}

	PcodeLifter lifter( smx );
	ILControlFlowGraph ilcfg = lifter.Lift( cfg );
	ILDisassembler disassembler;
	for( size_t i = 0; i < ilcfg.num_blocks(); i++ )
	{
		const ILBlock& bb = ilcfg.block( i );
		std::string disasm = disassembler.DisassembleBlock( bb );
		printf( "===== BB%zi =====\n", bb.id() );
		for( size_t in = 0; in < bb.num_in_edges(); in++ )
		{
			printf( "> Incoming edge: BB%zi\n", bb.in_edge( in )->id() );
		}
		for( size_t out = 0; out < bb.num_out_edges(); out++ )
		{
			printf( "> Outgoing edge: BB%zi\n", bb.out_edge( out )->id() );
		}
		printf( "%s\n", disasm.c_str() );
	}

	return 0;
}