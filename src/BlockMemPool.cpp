/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "BlockMemPool.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		15/06/2005
	$Id: BlockMemPool.cpp,v 1.2 2005/08/26 01:16:34 moore Exp $
*/

using namespace Torch;

namespace Juicer {

BlockMemPool::BlockMemPool( int elemByteLen_ , int reallocAmount_ )
{
    if ( sizeof(char) != 1 )
        error("BlockMemPool::BlockMemPool - sizeof(char) assumption wrong") ;

    elemByteLen = elemByteLen_ ;
    reallocAmount = reallocAmount_ ;
    reallocByteLen = elemByteLen * reallocAmount ;
   
	nTotal = 0 ;
	nFree = 0 ;
	nUsed = 0 ;
	nAllocs = 0 ;
    getCnt = 0 ;
    retCnt = 0 ;
	allocs = NULL ;
	freeElems = NULL ;
}


BlockMemPool::~BlockMemPool()
{
    purge_memory();
}

void BlockMemPool::purge_memory() {
	if ( allocs != NULL )
	{
		for ( int i=0 ; i<nAllocs ; i++ )
			delete [] allocs[i] ;
		::free( allocs ) ;
	}

	if ( freeElems != NULL )
		::free( freeElems ) ;

	nTotal = 0 ;
	nFree = 0 ;
	nUsed = 0 ;
	nAllocs = 0 ;
    getCnt = 0 ;
    retCnt = 0 ;
	allocs = NULL ;
	freeElems = NULL ;
}

void *BlockMemPool::getElem()
{
	if ( nFree == 0 )
	{
		freeElems = (char **)realloc(
            freeElems , (nTotal+reallocAmount)*sizeof(char *)
        );
		allocs = (char **)realloc( allocs , (nAllocs+1) * sizeof(char *) ) ;
		allocs[nAllocs] = new char[reallocByteLen] ;
		for ( int i=0 ; i<reallocAmount ; i++ )
			freeElems[i] = allocs[nAllocs] + elemByteLen * i ;
		
		nTotal += reallocAmount ;
		nFree += reallocAmount ;
		nAllocs++ ;
        //printf("Block realloc %d %d\n", elemByteLen, reallocAmount);
	}

	nFree-- ;
	nUsed++ ;
    getCnt++ ;

	return freeElems[nFree] ;
}


void BlockMemPool::returnElem( void *elem )
{
	freeElems[nFree] = (char *)elem ;

	nFree++ ;
	nUsed-- ;
    retCnt++ ;
}

}
