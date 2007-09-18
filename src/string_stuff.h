/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef STRING_STUFF_INC
#define STRING_STUFF_INC

#include "general.h"

namespace Juicer {


/*
   Author:		Darren Moore (moore@idiap.ch)
	Date:			2002
	$Id: string_stuff.h,v 1.3 2005/08/26 01:16:34 moore Exp $
*/


char *myfgets( FILE *fd ) ;
void strtoupper( char *str ) ;


inline void byteRev32( void *val )
{
	unsigned int v = *((unsigned int *)val) ;
	v = ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8) | 
		((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24) ;
	*((unsigned int *)val) = v ;
};

inline void byteRev16( void *val )
{
	unsigned short v = *((unsigned short *)val) ;
	v = ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8) ;
	*((unsigned short *)val) = v ;
};


}

#endif
