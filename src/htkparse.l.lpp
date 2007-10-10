/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "htkparse.h"
#include "htkparse.y.h"

#define YY_DECL int htklex( void *lvalp , void *fd )
%}

%option noyywrap
%option never-interactive

D                      [0-9]
W                      [ \t\r\n]
INT                    -?{D}+
RE                     (\+|-)?({D}+\.?{D}*|\.{D}+)([eE](\+|-)?{D}+)?
STR                    [a-zA-Z0-9\+\-\$\^#@_\&\[\]:]+
QSTR                   \"{STR}\"
PkBase                 Discrete|LPCepstra|FBank|MelSpec|LPRefC|User
PKBASE                 DISCRETE|LPC|LPCEPSTRA|MFCC|PLP|FBANK|MELSPEC|LPREFC|USER
pkbase                 discrete|lpc|lpcepstra|mfcc|plp|fbank|melspec|lprefc|user
PKQUAL                 _D|_A|_T|_E|_N|_Z|_O|_V|_C|_K|_0
pkqual                 _d|_a|_t|_e|_n|_z|_o|_v|_c|_k|_0

%%
                        yyin = (FILE *)fd ;
                        

{INT}                   {
                            ((YYSTYPE *)lvalp)->ival = atoi(htktext) ;
                            return INTEGER ;
                        }

{RE}                    {
                            ((YYSTYPE *)lvalp)->fval = (float)atof(htktext) ;
                            return REAL ;
                        }

{QSTR}                  {
									char *tmp ;
									tmp = strtok( htktext , "\"" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - QSTR malloc failed\n") ;
										fprintf(stderr,"QSTR: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return QUOTEDSTRING ;
                        }

{STR}                   {
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(htktext)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - STR malloc failed\n") ;
										fprintf(stderr,"STR: %s\n",htktext) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , htktext ) ;
									return STRING ;
                        }

\<(BeginHMM|BEGINHMM|beginhmm)\> {
                            return BEGINHMM ;
                        }
                        
\<(EndHMM|ENDHMM|endhmm)\> {
                            return ENDHMM ;
                        }

\<(NumStates|NUMSTATES|numstates)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return NUMSTATES ;
                        }
                        
\<(State|STATE|state)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return STATE ;
                        }
                        
\<(NumMixes|NUMMIXES|nummixes)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return NUMMIXES ;
                        }

\<(Mixture|MIXTURE|mixture)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return MIXTURE ;
                        }
                        
\<(Mean|MEAN|mean)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return MEAN ;
                        }

\<(Variance|VARIANCE|variance)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return VARIANCE ;
                        }

\<(GConst|GCONST|gconst)\>{W}*({RE}|{INT}) {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->fval = (float)atof( tmp ) ;
                            return GCONST ;
                        }
                        
\<(TransP|TRANSP|transp)\>{W}*{INT} {
                            char *tmp = strtok( htktext , " \r\n\t>" ) ;
                            tmp = strtok( NULL , " \r\n\t>" ) ;
                            ((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
                            return TRANSP ;
                        }
                        
\<(HmmSetId|HMMSETID|hmmsetid)\>{W}*({STR}|{QSTR}) {
									char *tmp = strtok( htktext , " \r\n\t\">" ) ;
									tmp = strtok( NULL , " \r\n\t\">" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - HMMSETID malloc failed\n") ;
										fprintf(stderr,"HMMSETID: %s\n",tmp) ;
										exit(1) ;
									}

									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return HMMSETID ;
                        }

\<(TMix|TMIX|tmix)\>{W}*({STR}|{QSTR}) {
									char *tmp = strtok( htktext , " \r\n\t\">" ) ;
									tmp = strtok( NULL , " \r\n\t\">" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - TMIX malloc failed\n") ;
										fprintf(stderr,"TMIX: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return TMIX ;
                        }
                        
\<(VecSize|VECSIZE|vecsize)\>{W}*{INT} {
									char *tmp = strtok( htktext , " \r\n\t>" ) ;
									tmp = strtok( NULL , " \r\n\t>" ) ;
									((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
									return VECSIZE ;
								}
                        
\<(StreamInfo|STREAMINFO|streaminfo)\>{W}*{INT} {
									char *tmp = strtok( htktext , " \r\n\t>" ) ;
									tmp = strtok( NULL , " \r\n\t>" ) ;
									((YYSTYPE *)lvalp)->ival = atoi( tmp ) ;
									return STREAMINFO ;
								}
                        
\<(DiagC|DIAGC|diagc)\>                 { return DIAGC ; }
\<(InvDiagC|INVDIAGC|invdiagc)\>        { return INVDIAGC ; }
\<(FullC|FULLC|fullc)\>                 { return FULLC ; }
\<(LLTC|lltc)\>                         { return LLTC ; }
\<(XFormC|XFORMC|xformc)\>              { return XFORMC ; }
\<(NullD|NULLD|nulld)\>                 { return NULLD ; }
\<(PoissonD|POISSOND|poissond)\>        { return POISSOND ; }
\<(GammaD|GAMMAD|gammad)\>              { return GAMMAD ; }
\<(GenD|GEND|gend)\>                    { return GEND ; }

\<({PkBase}|{PKBASE}|{pkbase})({PKQUAL}|{pkqual})*\> {
									char *tmp = strtok( htktext , "<>" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - PARMKIND malloc failed\n") ;
										fprintf(stderr,"PARMKIND: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return PARMKIND ;
                        }

~h{W}+{QSTR}            {
									char *tmp = strtok( htktext , " \"\r\n\t" ) ;
									tmp = strtok( NULL , " \"\r\n\t" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - HMACRO malloc failed\n") ;
										fprintf(stderr,"HMACRO: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return HMACRO ;
                        }
                        
~s{W}+{QSTR}            {
									char *tmp = strtok( htktext , " \r\n\t" ) ;
									tmp = strtok( NULL , " \"\r\n\t" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - SMACRO malloc failed\n") ;
										fprintf(stderr,"SMACRO: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return SMACRO ;
                        }

~m{W}+{QSTR}            {
									char *tmp = strtok( htktext , " \r\n\t" ) ;
									tmp = strtok( NULL , " \"\r\n\t" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - MMACRO malloc failed\n") ;
										fprintf(stderr,"MMACRO: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return MMACRO ;
                        }
                        
~t{W}+{QSTR}            {
									char *tmp = strtok( htktext , " \r\n\t" ) ;
									tmp = strtok( NULL , " \"\r\n\t" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - TMACRO malloc failed\n") ;
										fprintf(stderr,"TMACRO: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return TMACRO ;
                        }

~v{W}+{QSTR}            {
									char *tmp = strtok( htktext , " \r\n\t" ) ;
									tmp = strtok( NULL , " \"\r\n\t" ) ;
									if ( (((YYSTYPE *)lvalp)->cptr = (char *)malloc( (strlen(tmp)+1)*sizeof(char) )) == NULL )
									{
										fprintf(stderr,"htkparse.l - VMACRO malloc failed\n") ;
										fprintf(stderr,"VMACRO: %s\n",tmp) ;
										exit(1) ;
									}
									strcpy( ((YYSTYPE *)lvalp)->cptr , tmp ) ;
									return VMACRO ;
                        }

~o{W}+                  {
                            return OMACRO ;
                        }
   
{W}                     ;
.                       ;
