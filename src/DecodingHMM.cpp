/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DecodingHMM.h"
#include "log_add.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecodingHMM.cpp,v 1.8 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch;

namespace Juicer {


DecodingHMM::DecodingHMM()
{
	n_states = 0 ;
	states = NULL ;

	init_state = -1 ;
	fin_state = -1 ;

	n_emit_states = 0 ;
	n_nonemit_states = 0 ;
	emit_states = NULL ;
	nonemit_states = NULL ;

	from_bin_file = false ;
	dataHeap = 0;

    teeProb = LOG_ZERO;
}


/**
 * We use this to create a simple left to right model (eg. for a
 * single phone HMM) There are (n_states_-2) entries in
 * 'emis_prob_indices' (ie. only for emitting states).
 */
DecodingHMM::DecodingHMM(
    int n_states_ , real **log_trans_probs_ , int *emis_prob_indices
)
{
	real *suc_log_trans , *pred_log_trans ;
	int *suc_states , n_suc_states=0 , *pred_states , n_pred_states=0 ;

	n_states = n_states_ ;
	states = NULL ;
	init_state = 0 ;
	fin_state = n_states-1 ;

	n_emit_states = 0 ;
	n_nonemit_states = 0 ;
	emit_states = NULL ;
	nonemit_states = NULL ;

	from_bin_file = false ;
	dataHeap = 0;

	suc_log_trans = new real[10000] ;
	pred_log_trans = new real[10000] ;
	suc_states = new int[10000] ;
	pred_states = new int[10000] ;

    teeProb = LOG_ZERO;

	// Allocate memory to hold the states
	states = (DecodingHMMState *)malloc( n_states * sizeof(DecodingHMMState) ) ;

	// Create each state in turn
	for ( int i=0 ; i<n_states ; i++ )
	{
		if ( (i == 0) || (i == (n_states-1)) )
			initState( states+i , DHS_NONEMITTING ) ;
		else
			initState( states+i , DHS_EMITTING , -1 , -1 , emis_prob_indices[i-1] ) ;
	}

	// Now go through the log_transitions array in the HMM instance
	//   and extract the non-zero transitions FROM and TO this state.
	for ( int from=0 ; from<n_states ; from++ )
	{
		n_suc_states = 0 ; n_pred_states = 0 ;
		for ( int to=0 ; to<n_states ; to++ )
		{
			if ( log_trans_probs_[from][to] > LOG_ZERO )
			{
				suc_log_trans[n_suc_states] = log_trans_probs_[from][to] ;
				suc_states[n_suc_states] = to ;
				n_suc_states++ ;
			}

			// 'from' now is really 'to' and vice-versa in order to extract
			//   predecessor states
			if ( log_trans_probs_[to][from] > LOG_ZERO )
			{
				pred_log_trans[n_pred_states] = log_trans_probs_[to][from] ;
				pred_states[n_pred_states] = to ;
				n_pred_states++ ;
			}

		}
		setupSucStates( states+from , n_suc_states , suc_states , suc_log_trans ) ;
		setupPredStates( states+from , n_pred_states , pred_states , pred_log_trans ) ;
	}

	delete [] suc_log_trans ;
	delete [] pred_log_trans ;
	delete [] suc_states ;
	delete [] pred_states ;
}

/**
 * Creates a DecodingHMM from a HMM and its associated TransMatrix
 * (defined in Models.h) Specifically for WFSTDecoder, WFSTModel usage
 * - so not all fields initialised
 */
DecodingHMM::DecodingHMM(
    HMM *hmm , TransMatrix *tm
)
{
   if ( hmm == NULL )
      error("DecodingHMM::DecodingHMM(2) - hmm is NULL") ;
      
   n_states = hmm->nStates ;
   states = (DecodingHMMState *)malloc( n_states * sizeof(DecodingHMMState) ) ;

   for ( int i=0 ; i<n_states ; i++ )
   {
      states[i].wrd_id = -1 ;
      states[i].phn_id = -1 ;
      states[i].n_preds = 0 ;
      states[i].preds = NULL ;
      states[i].pred_probs = NULL ;

      states[i].emis_prob_index = hmm->gmmInds[i] ;
      if ( i == 0 )
      {
         if ( states[i].emis_prob_index >= 0 )
            error("DecodingHMM::DecodingHMM(2) - state 0 had emis_prob_index >= 0") ;
         states[i].type = DHS_PHONE_START ;
      }
      else if ( i == (n_states-1) )
      {
         if ( states[i].emis_prob_index >= 0 )
            error("DecodingHMM::DecodingHMM(2) - final state had emis_prob_index >= 0") ;
         states[i].type = DHS_PHONE_END ;
      }
      else
         states[i].type = DHS_EMITTING ;

      setupSucStates( states + i , tm->nSucs[i] , tm->sucs[i] , tm->logProbs[i] ) ; 
   }
   
   init_state = 0 ;
   fin_state = n_states - 1 ;
   from_bin_file = false ;
   dataHeap = NULL ;
   
	n_emit_states = 0 ;
	n_nonemit_states = 0 ;
	emit_states = NULL ;
	nonemit_states = NULL ;
   
   calcEmitAndNonEmitStates() ;

   // Check if this model is a tee model
   teeProb = LOG_ZERO;
   for (int i=0; i<tm->nSucs[0]; i++)
   {
       if (tm->sucs[0][i] == fin_state)
       {
           teeProb = tm->logProbs[0][i];
           //printf("Found tee prob %f\n", teeProb);
       }
   }
}


DecodingHMM::~DecodingHMM()
{
	if ( states != NULL )
	{
		if (dataHeap)
		{
			delete [] dataHeap;
			delete [] states ;
		}
		else
		{
			for ( int i=0 ; i<n_states ; i++ )
			{
				if ( states[i].n_sucs > 0 )
				{
					free( states[i].sucs ) ;
					free( states[i].suc_probs ) ;
				}
				if ( states[i].n_preds > 0 )
				{
					free( states[i].preds ) ;
					free( states[i].pred_probs ) ;
				}
			}
			free( states ) ;
		}
	}

	delete [] emit_states ;
	delete [] nonemit_states ;
}

/**
 * Adds a transition from from_st to to_st with probability log_prob_.
 * Assumes a transition does not already exist (does not check).
 */
void DecodingHMM::linkStates( int from_ind , int to_ind , real log_prob_ )
{
#ifdef DEBUG
	if ( (from_ind < 0) || (from_ind >= n_states) )
		error("DecodingHMM::linkStates - from_ind out of range %d\n",from_ind) ;
	if ( (to_ind < 0) || (to_ind >= n_states) )
		error("DecodingHMM::linkStates - to_ind out of range %d\n",to_ind) ;
	if ( from_bin_file == true )
		error("DecodingHMM::linkStates - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	int tmp ;
	DecodingHMMState *from_st , *to_st ;

	from_st = states + from_ind ;
	to_st = states + to_ind ;

	tmp = ++from_st->n_sucs ;
	from_st->sucs = (int *)realloc( from_st->sucs , tmp*sizeof(int) ) ;
	from_st->suc_probs = (real *)realloc( from_st->suc_probs , tmp*sizeof(real) ) ;
	from_st->sucs[tmp-1] = to_ind ;
	from_st->suc_probs[tmp-1] = log_prob_ ;

	tmp = ++to_st->n_preds ;
	to_st->preds = (int *)realloc( to_st->preds , tmp*sizeof(int) ) ;
	to_st->pred_probs = (real *)realloc( to_st->pred_probs , tmp*sizeof(real) ) ;
	to_st->preds[tmp-1] = from_ind ;
	to_st->pred_probs[tmp-1] = log_prob_ ;
}


void DecodingHMM::skipState( int skip_state )
{
#ifdef DEBUG
	if ( (skip_state < 0) || (skip_state >= n_states) )
		error("DecodingHMM::skipState - skip_state out of range\n") ;
	if ( (states[skip_state].type == DHS_EMITTING) || (states[skip_state].type == DHS_DISABLED) )
		error("DecodingHMM::skipState - cannot skip an emitting or disabled state\n") ;
	if ( from_bin_file == true )
		error("DecodingHMM::skipState - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	DecodingHMMState *st , *pred_st , *suc_st ;
	int i , j , k ;

	st = states + skip_state ;

	// 3.1 add the entire successor list of the skip state to each of its predecessors.
	for ( i=0 ; i<st->n_preds ; i++ )
	{
		pred_st = states + (st->preds[i]) ;
		// Remove the skip state from the successor list of the pred state
		for ( j=0 , k=0 ; j<pred_st->n_sucs ; j++ )
		{
			if ( pred_st->sucs[j] != skip_state )
			{
				pred_st->sucs[k] = pred_st->sucs[j] ;
				pred_st->suc_probs[k] = pred_st->suc_probs[j] ;
				k++ ;
			}
		}

		pred_st->n_sucs += (st->n_sucs - 1) ;
		pred_st->sucs = (int *)realloc( pred_st->sucs , pred_st->n_sucs*sizeof(int) ) ;
		pred_st->suc_probs = (real *)realloc( pred_st->suc_probs , pred_st->n_sucs*sizeof(real) ) ;
		for ( j=0 ; j<st->n_sucs ; j++ )
		{
			pred_st->sucs[k] = st->sucs[j] ;
			pred_st->suc_probs[k] = st->pred_probs[i] + st->suc_probs[j] ;
			k++ ;
		}
	}

	// 3.2 add the entire predecessor list of the skip state to each of its successors.
	for ( i=0 ; i<st->n_sucs ; i++ )
	{
		suc_st = states + (st->sucs[i]) ;
		// Remove the skip state from the predecessor list of the suc state
		for ( j=0 , k=0 ; j<suc_st->n_preds ; j++ )
		{
			if ( suc_st->preds[j] != skip_state )
			{
				suc_st->preds[k] = suc_st->preds[j] ;
				suc_st->pred_probs[k] = suc_st->pred_probs[j] ;
				k++ ;
			}
		}

		suc_st->n_preds += (st->n_preds - 1) ;
		suc_st->preds = (int *)realloc( suc_st->preds , suc_st->n_preds*sizeof(int) ) ;
		suc_st->pred_probs = (real *)realloc( suc_st->pred_probs , suc_st->n_preds*sizeof(real) ) ;
		for ( j=0 ; j<st->n_preds ; j++ )
		{
			suc_st->preds[k] = st->preds[j] ;
			suc_st->pred_probs[k] = st->suc_probs[i] + st->pred_probs[j] ;
			k++ ;
		}
	}

	// Mark 'skip_state' as disabled (ie. doesn't do anything anymore)
	disableState( skip_state ) ;
}


/**
 * Merges st2 into st1 and disables st2 
 *   (ie. changes type to DHS_DISABLED, deallocates suc and pred lists).
 * 1. The st2 successor list is added to the st1 successor list.
 * 2. All successors of st2 are updated to have st1 as a predecessor state.
 * 3. The st2 predecessor list is added to st1 predecessor list.
 * 4. All predecessors of st2 are updated to have st1 as a sucessor state.
 *
 * Does not change the type of st1.
 * Assumes that both st1 and st2 are non-emitting.
 */
void DecodingHMM::mergeStates( int st1 , int st2 )
{
#ifdef DEBUG
	if ( (st1 < 0) || (st1 >= n_states) || (st2 < 0) || (st2 >= n_states) )
		error("DecodingHMM::mergeStates - st1 or st2 out of range\n") ;
	if ( (states[st1].type == DHS_EMITTING) || (states[st1].type == DHS_DISABLED) )
		error("DecodingHMM::mergeStates - st1 type invalid\n") ;
	if ( (states[st2].type == DHS_EMITTING) || (states[st2].type == DHS_DISABLED) )
		error("DecodingHMM::mergeStates - st2 type invalid\n") ;
	if ( from_bin_file == true )
		error("DecodingHMM::mergeStates - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	int old_n , ind , i , j ;

	// 1. Add the st2 successor list to st1 successor list.
	old_n = states[st1].n_sucs ;
	states[st1].n_sucs += states[st2].n_sucs ;
	states[st1].sucs = (int *)realloc( states[st1].sucs , states[st1].n_sucs * sizeof(int) ) ;
	states[st1].suc_probs = (real *)realloc( states[st1].suc_probs , 
														  states[st1].n_sucs * sizeof(real) ) ;
	for ( i=0 ; i<states[st2].n_sucs ; i++ )
	{
		states[st1].sucs[old_n+i] = states[st2].sucs[i] ;
		states[st1].suc_probs[old_n+i] = states[st2].suc_probs[i] ;
	}

	// 2. Update all successors of st2 to have st1 as a predecessor state instead of st2.
	for ( i=0 ; i<states[st2].n_sucs ; i++ )
	{
		ind = states[st2].sucs[i] ;
		for ( j=0 ; j<states[ind].n_preds ; j++ )
		{
			if ( states[ind].preds[j] == st2 )
			{
				states[ind].preds[j] = st1 ;
				break ;
			}
		}
#ifdef DEBUG
		if ( j >= states[ind].n_preds )
			error("DecodingHMM::mergeStates - st2 not found in pred list\n") ;
#endif
	}

	// 3. Add the st2 predecessor list to the st1 predecessor list.
	old_n = states[st1].n_preds ;
	states[st1].n_preds += states[st2].n_preds ;
	states[st1].preds = (int *)realloc( states[st1].preds , states[st1].n_preds * sizeof(int) ) ;
	states[st1].pred_probs = (real *)realloc( states[st1].pred_probs , 
															states[st1].n_preds*sizeof(real) ) ;
	for ( i=0 ; i<states[st2].n_preds ; i++ )
	{
		states[st1].preds[old_n+i] = states[st2].preds[i] ;
		states[st1].pred_probs[old_n+i] = states[st2].pred_probs[i] ;
	}

	// 4. All predecessors of st2 are updated to have st1 as a sucessor state.
	for ( i=0 ; i<states[st2].n_preds ; i++ )
	{
		ind = states[st2].preds[i] ;
		for ( j=0 ; j<states[ind].n_sucs ; j++ )
		{
			if ( states[ind].sucs[j] == st2 )
			{
				states[ind].sucs[j] = st1 ;
				break ;
			}
		}
#ifdef DEBUG
		if ( j >= states[ind].n_sucs )
			error("DecodingHMM::mergeStates - st2 not found in suc list\n") ;
#endif
	}

	// 5. Disable st2.
	disableState( st2 ) ;    
}


/**
 * Frees up suc and pred lists and changes type to DHS_DISABLED.
 * Assumes that all predecessor and successor state have already had
 * their pred and suc lists updated to not include this state.
 *
 * Assumes that emitting and nonemitting state lists are not created
 * when this function is used.
 */
void DecodingHMM::disableState( int ind )
{
#ifdef DEBUG
	if ( (ind < 0) || (ind >= n_states) )
		error("DecodingHMM::disableState - ind out of range\n") ;
	if ( n_emit_states > 0 )
		error("DecodingHMM::disableState - function called after emit list created\n");
	if ( n_nonemit_states > 0 )
		error("DecodingHMM::disableState - function called after nonemit list created\n");
	if ( from_bin_file == true )
		error("DecodingHMM::disableState - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	if ( states[ind].n_sucs > 0 )
	{
		free( states[ind].sucs ) ;
		free( states[ind].suc_probs ) ;
	}
	states[ind].n_sucs = 0 ;
	states[ind].sucs = NULL ;
	states[ind].suc_probs = NULL ;

	if ( states[ind].n_preds > 0 )
	{
		free( states[ind].preds ) ;
		free( states[ind].pred_probs ) ;
	}
	states[ind].n_preds = 0 ;
	states[ind].preds = NULL ;
	states[ind].pred_probs = NULL ;

	states[ind].type = DHS_DISABLED ;
}


/**
 * Normalises the transitions from this state so that the sum of all
 * transitions is 1.0 (ie. 0.0 in log).  Updates the predecessor
 * entries of successor states.
 */
void DecodingHMM::normaliseState( int ind )
{
#ifdef DEBUG
	if ( (ind < 0) || (ind >= n_states) )
		error("DecodingHMM::normaliseState - ind out of range\n") ;
	if ( from_bin_file == true )
		error("DecodingHMM::normaliseState - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	real total ;
	int i , j , suc ;

	total = LOG_ZERO ;
	for ( i=0 ; i<states[ind].n_sucs ; i++ )
		total = logAdd( total , states[ind].suc_probs[i] ) ;

	//printf("ind=%d tot=%.4f\n",ind,total) ;

	for ( i=0 ; i<states[ind].n_sucs ; i++ )
	{
		suc = states[ind].sucs[i] ;
		states[ind].suc_probs[i] -= total ;

		for ( j=0 ; j<states[suc].n_preds ; j++ )
		{
			if ( states[suc].preds[j] == ind )
			{
				states[suc].pred_probs[j] -= total ;
				break ;
			}
		}
#ifdef DEBUG
		if ( j >= states[suc].n_preds )
			error("DecodingHMM::normaliseState - ind not found in pred list of suc\n") ;
#endif
	}
}


/**
 * Merges 'st2' into 'st1'.  Successor entries for 'st2' are updated
 * using 'offset' as they are added to 'st1'.  NOTE: Assumes 'st2' has
 * no predecessors.  Does not change the wrd_id or phn_id fields of
 * st1.  'log_scale' is used to scale the new transitions
 */
void DecodingHMM::mergeState(
    DecodingHMMState *st1 , DecodingHMMState *st2 , int offset , 
    real log_scale
)
{
#ifdef DEBUG
	if ( (st1 == NULL) || (st2 == NULL) )
		error("DecodingHMM::mergeState - st1 or st2 is NULL\n") ;
	if ( st2->n_preds != 0 )
		error("DecodingHMM::mergeState - st2 does not have 0 preds\n") ;
	if ( from_bin_file == true )
		error("DecodingHMM::mergeState - changes to DecHMM not allowed if loaded from bin file") ;
#endif
	int i , old_n_sucs ;

	old_n_sucs = st1->n_sucs ;
	st1->n_sucs += st2->n_sucs ;
	st1->sucs = (int *)realloc( st1->sucs , st1->n_sucs * sizeof(int) ) ;
	st1->suc_probs = (real *)realloc( st1->suc_probs , st1->n_sucs * sizeof(real) ) ;
	for ( i=0 ; i<st2->n_sucs ; i++ )
	{
		st1->sucs[i+old_n_sucs] = st2->sucs[i] + offset ;
		st1->suc_probs[i+old_n_sucs] = st2->suc_probs[i] + log_scale ;
	}
}


void DecodingHMM::initState( DecodingHMMState *st1 , DecodingHMMState *st2 , int phn_id_ , 
									  int wrd_id_ , int offset , int initial_index , real log_scale )
{
	int i ;
#ifdef DEBUG
	if ( (st1 == NULL) || (st2 == NULL) )
		error("DecodingHMM::initState(2) - something is NULL\n") ;
	if ( from_bin_file == true )
		error("DecodingHMM::initState(2) - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	st1->type = st2->type ;
	st1->emis_prob_index = st2->emis_prob_index ;

	st1->wrd_id = wrd_id_ ;
	st1->phn_id = phn_id_ ;

	st1->n_sucs = st2->n_sucs ;
	st1->sucs = (int *)malloc( st1->n_sucs * sizeof(int) ) ;
	st1->suc_probs = (real *)malloc( st1->n_sucs * sizeof(real) ) ;
	for ( i=0 ; i<st1->n_sucs ; i++ )
	{
		if ( st2->sucs[i] == 0 )
			st1->sucs[i] = initial_index ;
		else
			st1->sucs[i] = st2->sucs[i] + offset ;
		st1->suc_probs[i] = st2->suc_probs[i] ;
	}

	st1->n_preds = st2->n_preds ;
	st1->preds = (int *)malloc( st1->n_preds * sizeof(int) ) ;
	st1->pred_probs = (real *)malloc( st1->n_preds * sizeof(real) ) ;
	for ( i=0 ; i<st1->n_preds ; i++ )
	{
		if ( st2->preds[i] == 0 )
		{
			st1->preds[i] = initial_index ;
			st1->pred_probs[i] = st2->pred_probs[i] + log_scale ;
		}    
		else
		{
			st1->preds[i] = st2->preds[i] + offset ;
			st1->pred_probs[i] = st2->pred_probs[i] ;
		}
	}
}


void DecodingHMM::initState( DecodingHMMState *state , DecodingHMMStateType type_ , 
									  int phn_id_ , int wrd_id_ , int emis_index_ )
{
#ifdef DEBUG
	if ( (type_ < 0) || (type_ >= DHS_INVALID) )
		error("DecodingHMM::initState - type out of range") ;
	if ( (type_ == DHS_EMITTING) && (emis_index_ < 0) )
		error("DecodingHMM::initState - emitting state has emis_index_ < 0") ;
	if ( from_bin_file == true )
		error("DecodingHMM::initState - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	state->type = type_ ;
	state->wrd_id = wrd_id_ ;
	state->phn_id = phn_id_ ;
	state->emis_prob_index = emis_index_ ;

	state->n_sucs = 0 ;
	state->sucs = NULL ;
	state->suc_probs = NULL ;

	state->n_preds = 0 ;
	state->preds = NULL ;
	state->pred_probs = NULL ;
}


void DecodingHMM::setupSucStates( DecodingHMMState *state , int n_sucs_ , 
											 int *sucs_ , real *suc_probs_ )
{
#ifdef DEBUG
	if ( from_bin_file == true )
		error("DecodingHMM::setupSucStates - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	state->n_sucs = n_sucs_ ;

	if ( n_sucs_ > 0 )
	{
		state->sucs = (int *)malloc( n_sucs_ * sizeof(int) ) ;
		state->suc_probs = (real *)malloc( n_sucs_ * sizeof(real) ) ;
		for ( int i=0 ; i<n_sucs_ ; i++ )
		{
			state->sucs[i] = sucs_[i] ;
			state->suc_probs[i] = suc_probs_[i] ;
		}
	}
}


void DecodingHMM::setupPredStates( DecodingHMMState *state , int n_preds_ , 
											  int *preds_ , real *pred_probs_ )
{
#ifdef DEBUG
	if ( from_bin_file == true )
		error("DecHMM::setupPredStates - changes to DecHMM not allowed if loaded from bin file") ;
#endif

	state->n_preds = n_preds_ ;

	if ( n_preds_ > 0 )
	{
		state->preds = (int *)malloc( n_preds_ * sizeof(int) ) ;
		state->pred_probs = (real *)malloc( n_preds_ * sizeof(real) ) ;
		for ( int i=0 ; i<n_preds_ ; i++ )
		{
			state->preds[i] = preds_[i] ;
			state->pred_probs[i] = pred_probs_[i] ;
		}
	}
}


/**
 * Intended to be called between the HMM being created and it being
 * used.  Makes a note of the locations of all the emitting states and
 * the non-emitting states, and stores them in 'emit_states' and
 * 'nonemit_states' respectively.
 */
void DecodingHMM::calcEmitAndNonEmitStates()
{
	int emit_cnt=0 , nonemit_cnt=0 ;

	if ( emit_states != NULL )
		error("DecodingHMM::calcEmitAndNonEmitStates - emit_states is not NULL") ;
	if ( nonemit_states != NULL )
		error("DecodingHMM::calcEmitAndNonEmitStates - nonemit_states is not NULL") ;

	n_emit_states = 0 ;
	n_nonemit_states = 0 ;

	// 1. count the number of emitting and non-emitting states
	for ( int i=0 ; i<n_states ; i++ )
	{
		if ( states[i].type == DHS_EMITTING )
			n_emit_states++ ;
		else if ( (states[i].type != DHS_DISABLED) && (states[i].type != DHS_INVALID) ) 
			n_nonemit_states++ ;
	}

	// 2. Allocate memory to hold the indices of the emitting and non-emitting states.
	emit_states = new int[n_emit_states] ;
	nonemit_states = new int[n_nonemit_states] ;

	// 3. Populate the index arrays.
	emit_cnt = 0 ;
	nonemit_cnt = 0 ;
	for ( int i=0 ; i<n_states ; i++ )
	{
		if ( states[i].type == DHS_EMITTING )
			emit_states[emit_cnt++] = i ;
		else if ( (states[i].type != DHS_DISABLED) && (states[i].type != DHS_INVALID) ) 
			nonemit_states[nonemit_cnt++] = i ;
	}

	if ( (emit_cnt != n_emit_states) || (nonemit_cnt != n_nonemit_states) )
		error("DecodingHMM::calcEmitAndNonEmitStates - emit_cnt or nonemit_cnt error\n") ;
}


void DecodingHMM::writeBinary(FILE* pFile)
{
	// Header
	fwrite(&n_states, sizeof(int), 1, pFile);
	fwrite(&init_state, sizeof(int), 1, pFile);
	fwrite(&fin_state, sizeof(int), 1, pFile);

	// States
	fwrite(states, sizeof(DecodingHMMState), n_states, pFile);

	// Calculate heap size.
	size_t dataHeapSize = 0;
	for (int i = 0; i < n_states; i++)
		dataHeapSize += (states[i].n_sucs + states[i].n_preds) * (sizeof(int) + sizeof(float));
	fwrite(&dataHeapSize, sizeof(size_t), 1, pFile);

	// Write the heap.
	for (int i = 0; i < n_states; i++) {
		fwrite(states[i].sucs, sizeof(int), states[i].n_sucs, pFile);
		fwrite(states[i].suc_probs, sizeof(real), states[i].n_sucs, pFile);
		fwrite(states[i].preds, sizeof(int), states[i].n_preds, pFile);
		fwrite(states[i].pred_probs, sizeof(real), states[i].n_preds, pFile);
	}
}


void DecodingHMM::readBinary(FILE* pFile)
{
	// Header
	if (	fread(&n_states, sizeof(int), 1, pFile) != 1 ||
			fread(&init_state, sizeof(int), 1, pFile) != 1 ||
			fread(&fin_state, sizeof(int), 1, pFile) != 1)
		error("DecodingHMM::readBinary(). Failed to read file header.");

	// States
	states = new DecodingHMMState[n_states];
	if (states == 0)
		error("DecodingHMM::readBinary(). Failed to allocate states.");
	if (	(int)fread(states, sizeof(DecodingHMMState), n_states, pFile) != n_states)
		error("DecodingHMM::readBinary(). Failed to read states.");

	// Read the data heap.
	size_t dataHeapSize;
	if (	fread(&dataHeapSize, sizeof(size_t), 1, pFile) != 1)
		error("DecodingHMM::readBinary(). Failed to read dataHeapSize.");
	dataHeap = new char[dataHeapSize];
	if (dataHeap == 0)
		error("DecodingHMM::readBinary(). Failed to allocate dataHeap.");
	if (	fread(dataHeap, sizeof(char), dataHeapSize, pFile) != dataHeapSize)
		error("DecodingHMM::readBinary(). Failed to read dataHeap.");

	// Initialize references to the heap.
	char* dataHeapPtr = dataHeap;
	for (int i = 0; i < n_states; i++) {
		states[i].sucs = (int*)dataHeapPtr;		dataHeapPtr += states[i].n_sucs * sizeof(int);
		states[i].suc_probs = (real*)dataHeapPtr;	dataHeapPtr += states[i].n_sucs * sizeof(real);
		states[i].preds = (int*)dataHeapPtr;		dataHeapPtr += states[i].n_preds * sizeof(int);
		states[i].pred_probs = (real*)dataHeapPtr; dataHeapPtr += states[i].n_preds * sizeof(real);
	}
	from_bin_file = true ;
}


void DecodingHMM::outputText()
{
	int i , j ;

	printf("DecodingHMM with %d states\n*************************\n" , n_states) ;
	printf("init=%d , fin=%d\n" , init_state , fin_state ) ;
	for ( i=0 ; i<n_states ; i++ )
	{
		printf( "State %d : " , i ) ;
		switch ( states[i].type )
		{
			case DHS_EMITTING:
				printf("EMITTING : ") ;
				break ;
			case DHS_NONEMITTING:
				printf("NONEMITTING : ") ;
				break ;
			case DHS_NONEMITTING_NEEDLM:
				printf("NONEMITTING_NEEDLM : ") ;
				break ;
			case DHS_PHONE_START:
				printf("PHONE_START : ") ;
				break ;
			case DHS_PHONE_END:
				printf("PHONE_END : ") ;
				break ;
			case DHS_PRONUN_START:
				printf("PRONUN_START : ") ;
				break ;
			case DHS_PRONUN_END:
				printf("PRONUN_END : ") ;
				break ;
			case DHS_WORD_START:
				printf("WORD_START : ") ;
				break ;
			case DHS_WORD_END:
				printf("WORD_END : ") ;
				break ;
			case DHS_NULLWORD_START:
				printf("NULLWORD_START : ") ;
				break ;
			case DHS_NULLWORD_END:
				printf("NULLWORD_END : ") ;
				break ;
			case DHS_BACKOFF_NODE:
				printf("BACKOFF_NODE : ") ;
				break ;
			case DHS_WORDLOOP_FROM:
				printf("WORDLOOP_FROM : ") ;
				break ;
			case DHS_WORDLOOP_TO:
				printf("WORDLOOP_TO : ") ;
				break ;
			case DHS_DISABLED:
				printf("DISABLED : ") ;
				break ;
			default:
				error("DecHMM::outputText - invalid state detected\n") ;
				break ;
		}
		printf( "Phn_ID=%d Wrd_ID=%d Emis_Ind=%d\n" , 
				states[i].phn_id , states[i].wrd_id , states[i].emis_prob_index ) ;

		printf("DecodingHMMState:\n\tn_sucs=%d    ",states[i].n_sucs) ;
		for ( j=0 ; j<states[i].n_sucs ; j++ )
			printf("%d ",states[i].sucs[j]) ;
		printf("   ") ;
		for ( j=0 ; j<states[i].n_sucs ; j++ )
			printf("%.4f ",states[i].suc_probs[j]) ;
		printf("\n\tn_preds=%d    ",states[i].n_preds) ;
		for ( j=0 ; j<states[i].n_preds ; j++ )
			printf("%d ",states[i].preds[j]) ;
		printf("   ") ;
		for ( j=0 ; j<states[i].n_preds ; j++ )
			printf("%.4f ",states[i].pred_probs[j]) ;
		printf("\n") ;
	}
	printf("\n") ;

	printf("n_emit_states = %d\n",n_emit_states) ;
	for ( i=0 ; i<n_emit_states ; i++ )
		printf(" %d",emit_states[i]) ;
	printf("\n") ;
	printf("n_nonemit_states = %d\n",n_nonemit_states) ;
	for ( i=0 ; i<n_nonemit_states ; i++ )
		printf(" %d",nonemit_states[i]) ;
	printf("\n\n") ;
}


}
