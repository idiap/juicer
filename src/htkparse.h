/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef HTKPARSE_INC
#define HTKPARSE_INC

#include <stdio.h>
#ifndef NOTORCH
#include "general.h"
#else
typedef float real ;
#endif

typedef enum
{
    CK_DIAGC=0 ,
    CK_INVDIAGC ,
    CK_FULLC ,
    CK_LLTC ,
    CK_XFORMC ,
    CK_INVALID
} CovKind ;


typedef enum
{
    DK_NULLD=0 ,
    DK_POISSOND ,
    DK_GAMMAD ,
    DK_GEND ,
    DK_INVALID
} DurKind ;


/*  We don't use these at the moment - we just  */
/*  save the entire parmkind string             */
/*
typedef enum
{
    BK_DISCRETE=0 ,
    BK_LPC ,
    BK_LPCEPSTRA ,
    BK_MFCC ,
    BK_FBANK ,
    BK_MELSPEC ,
    BK_LPREFC ,
    BK_USER ,
    BK_INVALID    
} ParmKindBase ;

typedef enum
{
    PKQ_D=0 ,
    PKQ_A ,
    PKQ_T ,
    PKQ_E ,
    PKQ_N ,
    PKQ_Z ,
    PKQ_O ,
    PKQ_V ,
    PKQ_C ,
    PKQ_K ,
    PKQ_INVALID
} ParmKindQual ;

typedef struct
{
    ParmKindBase base ;
    int n_quals ;
    ParmKindQual *quals ;
} ParmKind ;
*/

typedef struct
{
    char *hmm_set_id ;
    int n_streams ;
    int *stream_widths ;
    int vec_size ;
    CovKind cov_kind ;
    DurKind dur_kind ;
    char *parm_kind_str ;
} HTKGlobalOpts ;


typedef struct
{
    int id ;
    real weight ;
    int n_means ;
    real *means ;
    int n_vars ;
    real *vars ;
    real gconst ;
} HTKMixture ;


typedef struct
{
    char *name ;
    int n_mixes ;
    HTKMixture **mixes ;
} HTKMixturePool ;


typedef struct
{
    char *sh_name ;
    int id ;
    int n_mixes ;
    
    HTKMixture **mixes ;
    /* -or- */
    int pool_ind ;
    real *weights ;
} HTKHMMState ;


typedef struct
{
   char *sh_name ;
   int n_states ;
   real **transp ;
} HTKTransMat ;


typedef struct
{
    char *name ;
    int n_states ;
    HTKHMMState **emit_states ;
    HTKTransMat *transmat ;
} HTKHMM ;


typedef struct
{
   HTKGlobalOpts global_opts ;

   int n_sh_transmats ;
   HTKTransMat **sh_transmats ;

   int n_sh_states ;
   HTKHMMState **sh_states ;

   int n_mix_pools ;
   HTKMixturePool **mix_pools ;

   int n_hmms ;
   HTKHMM **hmms ;
} HTKDef ;


extern HTKDef htk_def ;
extern int htkparse( void * ) ;
extern void cleanHTKDef() ;


/* extra structures used internally by Bison parser */
typedef struct
{
    int n_elems ;
    real *elems ;
} RealVector ;

typedef struct
{
    int n_elems ;
    int *elems ;
} IntVector ;

typedef struct
{
    int n_mixes ;

    HTKMixture **mixes ;
    /* -or */
    int pool_ind ;
    real *weights ;
} HTKMixtureList ;

typedef struct
{
    int n_states ;
    HTKHMMState **states ;
} HTKHMMStateList ;

typedef struct
{
    int n_hmms ;
    HTKHMM **hmms ;
} HTKHMMList ;


#endif
