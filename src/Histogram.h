/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef HISTOGRAM_INC
#define HISTOGRAM_INC

#include "general.h"
#include "log_add.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: Histogram.h,v 1.3 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {


typedef struct
{
	int 	min ;
	int 	max ;
	int 	cnt ;
	real	best ;
	real	worst ;
} HistogramBin ;


class Histogram
{
public:
	// Public member variables
	int				count ;
	real				bestScore ;
	real				worstScore ;

	// Constructors / destructor
	Histogram( int binWidth_ , real minScore_ , real maxScore_ ) ;
	virtual ~Histogram() ;	

	// Public methods
	void addScore( real score , real oldScore=LOG_ZERO ) ;
	void reset() ;
	real calcThresh( int maxN ) ;
	void outputText() ;
	
private:
	// Private member variables
	int				nBins ;
	int				minScore ;
	int				maxScore ;
	int				binWidth ;
	
	HistogramBin	*bins ;	
	
	// Private methods
	void resetBin( int ind ) ;
} ;


}

#endif
