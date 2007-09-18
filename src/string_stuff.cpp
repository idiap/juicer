/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <string.h>
#include <ctype.h>
#include "string_stuff.h"

using namespace Torch;

namespace Juicer {


char *myfgets( FILE *fd )
{
	int step_size=1000 , max_size ;
	char *ptr , *buf ;

#ifdef DEBUG
	if ( fd==NULL )
		error("myfgets - invalid input parameter\n") ;
#endif

	// initially allocate 1000 bytes
	max_size = step_size ;
	buf = (char *)malloc( max_size * sizeof(char) ) ;
	ptr = buf ;

	if ( fgets( buf , step_size , fd ) == NULL )
	{
		free( buf ) ;
		return NULL ;
	}

	while ( ((int)strlen( ptr ) >= (step_size-1)) && (ptr[step_size-2] != '\n') )
	{
		// The buffer was not big enough to read the whole line.
		// We now have max_size-1 chars + the NULL char
		// Reallocate and keep reading (overwrite the NULL).
		max_size += (step_size-1) ;
		buf = (char *)realloc( buf , max_size*sizeof(char) ) ;
		ptr = buf + max_size - step_size ;
		if ( fgets( ptr , step_size , fd ) == NULL )
			error("myfgets - unexpected EOF\n") ;
	}

	return buf ;
}


void strtoupper( char *str )
{
	if ( str == NULL )
		return ;

	for ( int i=0 ; i<(int)strlen(str) ; i++ )
		str[i] = toupper( str[i] ) ;
}


}
