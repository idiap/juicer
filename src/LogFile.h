/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef LOG_FILE_INC
#define LOG_FILE_INC

#include "general.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		15 Dec 2004
	$Id: LogFile.h,v 1.3 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {


class LogFile
{
public:
   LogFile() ;
   virtual ~LogFile() ;

   static void open( const char *logFName_ ) ;
   static void close() ;
   static void puts( const char *str ) ;
   static void date( const char *prefixStr ) ;
   static void hostname( const char *prefixStr ) ;
   static void printf( const char *msg , ... ) ;
   static FILE *getFD() { return outFD ; } ;

private:
   static char        *logFName ;
   static FILE        *outFD ;
};


}

#endif
