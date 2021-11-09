#include <iostream>
#include <filesystem>
#include "optparse.h"
#include "smx-file.h"
#include "decompiler.h"

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
	
	DecompilerOptions options;
	options.print_globals = !args["no-globals"];
	options.print_il = args["il"];
	options.print_assembly = args["assembly"];
	options.function = args["function"];

	Decompiler decompiler( smx, options );
	decompiler.Print();

	return 0;
}