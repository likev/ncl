/*
 *	$Id: c_lbsetr.c,v 1.5 2008-07-23 16:16:57 haley Exp $
 */
/************************************************************************
*                                                                       *
*                Copyright (C)  2000                                    *
*        University Corporation for Atmospheric Research                *
*                All Rights Reserved                                    *
*                                                                       *
*    The use of this Software is governed by a License Agreement.       *
*                                                                       *
************************************************************************/

#include <ncarg/ncargC.h>

extern void NGCALLF(lbsetr,LBSETR)(NGstring,float*,int);

void c_lbsetr
#ifdef NeedFuncProto
(
    char *whch,
    float rval
)
#else
(whch,rval)
    char *whch;
    float rval;
#endif
{
    NGstring whch2;
    int len;
/*
 * Make sure parameter name is not NULL
 */
    if( !whch ) {
        fprintf( stderr, "c_lbsetr:  illegal parameter string (NULL)\n" );
        return;
    }

    len = NGSTRLEN(whch);
    whch2 = NGCstrToFstr(whch,len);
    NGCALLF(lbsetr,LBSETR)(whch2,&rval,len);
}
