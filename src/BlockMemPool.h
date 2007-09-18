/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef BLOCK_MEM_POOL
#define BLOCK_MEM_POOL

#include "general.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		15/06/2005
	$Id: BlockMemPool.h,v 1.3.4.1 2006/06/21 12:34:12 juicer Exp $
*/

/*
 	Author: Octavian Cheng (ocheng@idiap.ch)
	Date:		6 June 2006
*/

namespace Juicer {

/**
 * Block memory pool
 */
class BlockMemPool
{
public:
    BlockMemPool( int elemByteLen_ , int reallocAmount_ ) ;
    virtual ~BlockMemPool() ;

    void *getElem() ;
    void returnElem( void *elem ) ;

    // Changes Octavian
    bool isAllFreed()
    {
        return (nFree == nTotal);
    }

private:
    int elemByteLen ;
    int reallocAmount ;
    int	reallocByteLen ;
    int	nTotal ;
    int	nFree ;
    int	nUsed ;
    int	nAllocs ;
    int getCnt ;
    int retCnt ;

    char** allocs ;
    char** freeElems ;
};


}

#endif


