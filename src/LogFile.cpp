/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "LogFile.h"
#include "time.h"
#include "unistd.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		15 Dec 2004
	$Id: LogFile.cpp,v 1.7 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch;

namespace Juicer {


FILE *LogFile::outFD = NULL ;
char *LogFile::logFName = NULL ;


LogFile::LogFile()
{
}


LogFile::~LogFile()
{
}


void LogFile::open( const char *logFName_ )
{
   if ( outFD != NULL )
      error("LogFile::open - outFD != NULL. File already open.") ;

   if ( (logFName_ == NULL) || (logFName_[0] == '\0') )
      return ;

   if ( strcmp( logFName_ , "stdout" ) == 0 )
      outFD = stdout ;
   else if ( strcmp( logFName_ , "stderr" ) == 0 )
      outFD = stderr ;
   else
   {
      logFName = new char[strlen(logFName_)+1] ;
      strcpy( logFName , logFName_ ) ;
   
      if ( (outFD = fopen( logFName , "wb" )) == NULL )
         error("LogFile::init - error opening log file: %s" , logFName) ;
   }
}


void LogFile::close()
{
   if ( (outFD != NULL) && (outFD != stdout) && (outFD != stderr) )
      fclose( outFD ) ;
   outFD = NULL ;
   delete [] logFName ;
   logFName = NULL ;
}


void LogFile::puts( const char *str )
{
   if ( outFD == NULL )
      return ;

   if ( (str == NULL) || (str[0] == '\0') )
      error("LogFile::puts - str undefined") ;
   
   fputs( str , outFD ) ;
   fflush( outFD ) ;
}


void LogFile::date( const char *prefixStr )
{
   if ( outFD == NULL )
      return ;
   
   if ( prefixStr != NULL )
   {
      fprintf( outFD , "%s " , prefixStr ) ;
   }
   
   time_t t = time(NULL) ;
   fputs( ctime( &t ) , outFD ) ;
   fflush( outFD ) ;
}


void LogFile::hostname( const char *prefixStr )
{
   if ( outFD == NULL )
      return ;
   
   if ( prefixStr != NULL )
   {
      fprintf( outFD , "%s " , prefixStr ) ;
   }
   
   char str[100] ;
   if ( gethostname( str , 100 ) < 0 )
   {
      fprintf( outFD , "UNKNOWN HOST\n" ) ;
   }
   else
   {
      fprintf( outFD , "%s\n" , str ) ;
   }
   fflush( outFD ) ;
}


void LogFile::printf( const char *msg , ... )
{
   if ( outFD == NULL )
      return ;
      
   va_list args ;
   va_start( args , msg ) ;
   vfprintf( outFD , msg , args ) ;
   va_end(args);
   fflush( outFD ) ;
}


}
