C
C	$Id: lined.f,v 1.4 2008-07-27 00:23:02 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE LINED (XA,YA,XB,YB)
      SAVE
C USER ENTRY POINT.
      DATA IDUMMY /0/
      CALL FL2INT (XA,YA,IXA,IYA)
      CALL FL2INT (XB,YB,IXB,IYB)
C
      CALL CFVLD (1,IXA,IYA)
      CALL CFVLD (2,IXB,IYB)
      CALL CFVLD (3,IDUMMY,IDUMMY)
C
      RETURN
      END
