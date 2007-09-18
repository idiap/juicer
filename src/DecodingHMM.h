/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECODINGHMM_INC
#define DECODINGHMM_INC

#include "general.h"
#include "Models.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecodingHMM.h,v 1.4 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {

/**
 * Decoding HMM state type
 */
typedef enum
{
	DHS_EMITTING=0 ,
	DHS_PHONE_START ,
	DHS_PHONE_END ,
	DHS_PRONUN_START ,
	DHS_PRONUN_END ,
	DHS_WORD_START ,
	DHS_WORD_END ,
	DHS_NULLWORD_START ,
	DHS_NULLWORD_END ,
	DHS_WORDLOOP_FROM ,
	DHS_WORDLOOP_TO ,
	DHS_NONEMITTING ,
	DHS_NONEMITTING_NEEDLM ,
	DHS_BACKOFF_NODE ,
	DHS_DISABLED ,
	DHS_INVALID
} 
DecodingHMMStateType ;


/**
 * This structure is used to store all HMM state information required
 * for decoding. Each state contains a list of successor states and
 * associated transition probabilities.
 */
typedef struct
{
	DecodingHMMStateType type ;

	// for non-emitting states
	int		wrd_id ;     // identifies word
	int		phn_id ;     // identifies phoneme

	// for emitting states
	int		emis_prob_index ;  // index into list of distributions in the 'phone_models' object

	// common to all types of states
	int		n_sucs ;        // number of successor states
	int		*sucs ;         // indices of the successor states
	real		*suc_probs ;   // transition probabilities to the successor states

	int		n_preds ;
	int		*preds ;
	real		*pred_probs ;
}
DecodingHMMState ;



struct HMM ;
struct TransMatrix ;

/**
 * This class contains all HMM information required for decoding.
 * Most information is embedded in the states themselves
 * (DecodingHMMState structures).  The DecodingHMM can be created a
 * number of ways to facilitate compatibility with the Torch HMM class
 * and to allow easy concatenation of models (eg. when constructing
 * word models from phoneme models).
 *  
 * @author Darren Moore (moore@idiap.ch)
 */
class DecodingHMM
{
public:
	// Public member variables
	int						n_states ;
	DecodingHMMState		*states ;

	int						init_state ;
	int						fin_state ;

	int						n_emit_states ;
	int						n_nonemit_states ;
	int						*emit_states ;
	int						*nonemit_states ;

	bool 						from_bin_file ;

    real teeProb;

	// Constructors / destructor
	DecodingHMM() ;
    DecodingHMM(
        HMM *hmm , TransMatrix *tm
    ) ;

	// Creates a DecodingHMM using the distributions in 'dists_' and the log
	//   transition probabilties in 'log_trans_probs_'.
	DecodingHMM(
        int n_states_ , real **log_trans_probs_ , int *emis_prob_indices
    ) ; 
	virtual ~DecodingHMM() ;

	// Public methods
	// Configures successor state information for the state denoted by 'state'.
	void setupSucStates( DecodingHMMState *state , int n_sucs_ , int *sucs_ , real *suc_probs_ ) ;

	// Configures predecessor state information for the state denoted by 'state'.
	void setupPredStates( DecodingHMMState *state , int n_preds_ , int *preds_ , 
								 real *pred_probs_ ) ;

	// Initialises the state denoted by 'state' with a Distribution and
	//   optionally an index into the vector of emission probabilities.
	void initState( DecodingHMMState *state , DecodingHMMStateType type_ , 
						 int phn_id_=-1 , int wrd_id_=-1 , int emis_index_=-1 ) ;
	void initState( DecodingHMMState *st1 , DecodingHMMState *st2 , int phn_id_ , 
						 int wrd_id_ , int offset , int initial_index , real log_scale ) ;
	void skipState( int skip_state ) ;
	void disableState( int ind ) ;
	void normaliseState( int ind ) ;
	void linkStates( int from_ind , int to_ind , real log_prob_ ) ;
	void mergeStates( int st1 , int st2 ) ;
	void mergeState( DecodingHMMState *st1 , DecodingHMMState *st2 , int offset , 
						  real log_scale=0.0 ) ;
	void calcEmitAndNonEmitStates() ;

	void writeBinary( FILE *fd ) ;
	void readBinary( FILE *fd ) ;

//#ifdef DEBUG
	void outputText() ;
//#endif

private:
	char*		dataHeap;
};
    

}

#endif
