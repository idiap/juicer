/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "Histogram.h"
#include "log_add.h"
#include <math.h>

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: Histogram.cpp,v 1.4 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch;

namespace Juicer {


Histogram::Histogram( int binWidth_ , real minScore_ , real maxScore_ )
{
	if ( minScore_ >= maxScore_ )
		error("Histogram::Histogram - minScore_ >= maxScore_") ;

	// Don't need to be super precise with the cutoffs
	minScore = (int)(minScore_ - 1.0) ;
	maxScore = (int)(maxScore_ + 1.0) ;
	if ( (binWidth = binWidth_) <= 0 )
		error("Histogram::Histogram - binWidth_ <= 0") ;

	if ( ((maxScore - minScore + 1) % binWidth) != 0 )
		maxScore = ((maxScore-minScore+1)/binWidth + 1) * binWidth + minScore - 1 ;

	nBins = (maxScore - minScore + 1) / binWidth ;
	bins = new HistogramBin[nBins] ;
	
	count = 0 ;
	bestScore = LOG_ZERO ;
	worstScore = -LOG_ZERO ;

	// Create the bins then reset and configure min and max for each bin.
	for ( int i=0 ; i<nBins ; i++ )
	{
		resetBin(i) ;
		bins[i].min = (binWidth * i + minScore) ;
		bins[i].max = bins[i].min + binWidth - 1 ;
	}

	// Set the max field of the last bin to be exactly the maxScore of the histogram
	if ( bins[nBins-1].max != maxScore )
		error("Histogram::Histogram - unexpected maxScore") ;
}


Histogram::~Histogram()
{
	delete [] bins ;
}


void Histogram::addScore( real score , real oldScore )
{
	// 1. Figure out which bin this score belongs in.
	// 2. Increment count for that bin.
	// 3. See if the new score is the best/worst so far for that bin.
	// 4. If oldScore is defined, decrement the corresponding bin count.
	
	int bin , sc ;

	if ( score < 0.0 )
		sc = (int)(score - 0.5) ;
	else
		sc = (int)(score + 0.5) ;
	
	if ( sc > maxScore )
		error("Histogram::addScore - score > maxScore") ;
	if ( sc < minScore )
		return ;

	bin = (sc - minScore) / binWidth ;
#ifdef DEBUG
	if ( (sc < bins[bin].min) || (sc > bins[bin].max) )
		error("Histogram::addScore - score not in bin range. score=%f bin=%d min=%f max=%f minScore=%f maxScore=%f" , score , bin , bins[bin].min , bins[bin].max , minScore , maxScore ) ;
#endif

	// Increment bin count and check for best/worst conditions
	bins[bin].cnt++ ;
	if ( score > bins[bin].best )
		bins[bin].best = score ;
	if ( score < bins[bin].worst )
		bins[bin].worst = score ;

	// Increment global count and check for global best/worst conditions
	count++ ;
	if ( score > bestScore )
		bestScore = score ;
	if ( score < worstScore )
		worstScore = score ;

	if ( oldScore <= LOG_ZERO )
		return ;

	if ( oldScore < 0.0 )
		sc = (int)(oldScore - 0.5) ;
	else
		sc = (int)(oldScore + 0.5) ;

	if ( sc >= minScore )
	{
		if ( sc > maxScore )
			error("Histogram::addScore - oldScore > maxScore") ;
		
		bin = (sc - minScore) / binWidth ;
		bins[bin].cnt-- ;
		count-- ;
	}	
}		


void Histogram::reset()
{
	count = 0 ;
	bestScore = LOG_ZERO ;
	worstScore = -LOG_ZERO ;

	for ( int i=0 ; i<nBins ; i++ )
		resetBin( i ) ;
}

	
real Histogram::calcThresh( int maxN )
{
	// We want to figure out the minimum number of bins we need to include
	//   in order to keep the best 'maxN' scores.
	// We then return the 'min' field of the last bin 
	//   (which will be used as a pruning thresh by the decoder).
	
	int total = 0 ;

	// If the global count is >= maxN then return the min field of the bottom bin.
	if ( count <= maxN )
		return (real)( (real)( bins[0].min ) - 0.5 ) ;

	for ( int i=(nBins-1) ; i>=0 ; i-- )
	{
		total += bins[i].cnt ;
		if ( total >= maxN )
			return (real)( (real)( bins[i].min ) - 0.5 ) ;
	}

	// If we make it here then there is a problem.
	error("Histogram::calcThresh - there is a problem") ;
	return (real)( bins[0].min ) ;
}


void Histogram::resetBin( int ind )
{
#ifdef DEBUG
	if ( (ind < 0) || (ind >= nBins) )
		error("Histogram::resetBin - bin index out of range") ;
#endif

	bins[ind].cnt = 0 ;
	bins[ind].best = LOG_ZERO ;
	bins[ind].worst = -LOG_ZERO ;
}


void Histogram::outputText()
{
	printf( "*** HISTOGRAM nBins=%d minScore=%d maxScore=%d ***\n" ,
			  nBins , minScore , maxScore ) ;
	for ( int i=0 ; i<nBins ; i++ )
		printf( "Bin %d: min=%d max=%d cnt=%d best=%.4f worst=%.4f\n" , 
				  i , bins[i].min , bins[i].max , bins[i].cnt , bins[i].best , bins[i].worst ) ;
	printf( "*** END HISTOGRAM OUTPUT ***\n" ) ;
}


} // namespace Juicer
