#include "smx-file.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	for( size_t i = 0; i < smx.num_natives(); i++ )
	{
		auto& ntv = smx.native( i );
		printf( "Native: %s\n", ntv.name );
	}

	return 0;
}