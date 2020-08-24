#include "smx-file.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	for( size_t i = 0; i < smx.num_type_sets(); i++ )
	{
		auto& ts = smx.type_set( i );
		printf( "Typeset: %s\n", ts.name );
	}
	for( size_t i = 0; i < smx.num_type_defs(); i++ )
	{
		auto& td = smx.type_def( i );
		printf( "Typedef: %s\n", td.name );
	}

	return 0;
}