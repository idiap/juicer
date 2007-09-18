/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_GENERAL_INC
#define WFST_GENERAL_INC

#include "general.h"

/*
   Author :    Darren Moore (moore@idiap.ch)
   Date :      20 October 2004
   $Id: WFSTGeneral.h,v 1.4.4.1 2006/06/21 12:34:12 juicer Exp $
*/

/*
   Author:	Octavian Cheng (ocheng@idiap.ch)
   Date:	6 June 2006
*/

namespace Juicer {


#define  WFST_EPSILON         0
#define  WFST_EPSILON_STR     "<eps>"
#define  WFST_PHI_STR         "#phi"

// Changes Octavian
#define NONPUSHING_OUTLABEL			-1
#define UNDECIDED_OUTLABEL 			-2


inline void writeFSMTransition( FILE *fsmFD , int fromSt , int toSt , 
                                int inSym , int outSym , real weight=0.0 )
{
   if ( weight == 0.0 )
   {
      fprintf( fsmFD , "%d %d %d %d\n" , fromSt , toSt , inSym , outSym ) ;
   }
   else
   {
      fprintf( fsmFD , "%d %d %d %d %.3f\n", fromSt, toSt, inSym, outSym, weight ) ;
   }
};
   

inline void writeFSMFinalState( FILE *fsmFD , int finalSt , real weight=0.0 )
{
   if ( weight == 0.0 )
      fprintf( fsmFD , "%d\n" , finalSt ) ;
   else
      fprintf( fsmFD , "%d %f\n" , finalSt , weight ) ;
};


inline void writeFSMSymbol( FILE *symFD , const char *str , int id )
{
   fprintf( symFD , "%-25s %d\n" , str , id ) ;
};


}

#endif
