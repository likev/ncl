C
C	$Id: vectd.f,v 1.4 2008-07-27 00:23:02 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE VECTD (X,Y)
      SAVE
C USER ENTRY POINT.
      CALL FL2INT (X,Y,IIX,IIY)
      CALL CFVLD (2,IIX,IIY)
      RETURN
      END
