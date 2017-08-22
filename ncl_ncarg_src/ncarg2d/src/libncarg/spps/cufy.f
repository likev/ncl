C
C $Id: cufy.f,v 1.7 2008-07-27 00:17:23 haley Exp $
C
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      FUNCTION CUFY (RY)
C
C Given a Y coordinate RY in the user system, CUFY(RY) is a Y
C coordinate in the fractional system.
C
      COMMON /IUTLCM/ LL,MI,MX,MY,IU(96)
      SAVE /IUTLCM/
      DIMENSION WD(4),VP(4)
      CALL GQCNTN (IE,NT)
      IF (IE.NE.0) THEN
        CALL SETER ('CUFY - ERROR EXIT FROM GQCNTN',1,1)
        CUFY=0.
        RETURN
      END IF
      CALL GQNT (NT,IE,WD,VP)
      IF (IE.NE.0) THEN
        CALL SETER ('CUFY - ERROR EXIT FROM GQNT',2,1)
        CUFY=0.
        RETURN
      END IF
      I=3
      IF (MI.EQ.2.OR.MI.GE.4) I=4
      IF (LL.LE.1.OR.LL.EQ.3) THEN
        CUFY=(RY-WD(I))/(WD(7-I)-WD(I))*(VP(4)-VP(3))+VP(3)
      ELSE
        CUFY=(ALOG10(RY)-WD(I))/(WD(7-I)-WD(I))*(VP(4)-VP(3))+VP(3)
      ENDIF
      RETURN
      END
