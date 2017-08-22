C
C $Id: phi2sp.f,v 1.5 2008-07-27 00:17:10 haley Exp $
C
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      REAL FUNCTION PHI2SP (ECCENT,TS)
C
C FUNCTION TO COMPUTE LATITUDE ANGLE (PHI-2).
C
      IMPLICIT REAL (A-Z)
      INTEGER II,NIT
      COMMON /ERRMZ0/ IERR
        INTEGER IERR
      SAVE   /ERRMZ0/
      COMMON /PRINZ0/ IPEMSG,IPELUN,IPPARM,IPPLUN
        INTEGER IPEMSG,IPELUN,IPPARM,IPPLUN
      SAVE   /PRINZ0/
      DATA HALF,ONE,TWO /0.5E0,1.0E0,2.0E0/
      DATA TOL,NIT /1.0E-10,15/
      DATA HALFPI /1.5707963267948966E0/
C
      ECCNTH = HALF * ECCENT
      PHI = HALFPI - TWO * ATAN (TS)
      DO 020 II = 1,NIT
      SINPI = SIN (PHI)
      CON = ECCENT * SINPI
      DPHI = HALFPI - TWO * ATAN (TS * ((ONE - CON) /
     .       (ONE + CON)) ** ECCNTH) - PHI
      PHI = PHI + DPHI
      IF (ABS(DPHI) .GT. TOL) GO TO 020
      PHI2SP = PHI
      RETURN
  020 CONTINUE
C
      IF (IPEMSG .EQ. 0) WRITE (IPELUN,2000) NIT,ECCENT,TS
 2000 FORMAT (/' ERROR PHI2SP' /
     .         ' LATITUDE FAILED TO CONVERGE AFTER',I3,' ITERATIONS'/
     .         ' ECCENTRICITY =',E25.16,'   TS =',E25.16)
      IERR = 002
      RETURN
C
      END
