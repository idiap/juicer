/* 
// Copyright (c) 2005, Darren Moore (IDIAP)
//
// This file is part of the Juicer decoding package.
//
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer. 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution. 
// 3. Neither the name of the author nor the names of its contributors may be
//    used to endorse or promote products derived from this software without
//    specific prior written permission. 
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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
