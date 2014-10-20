CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
C
C   FFTPACK 5.0
C   Copyright (C) 1995-2004, Scientific Computing Division,
C   University Corporation for Atmospheric Research
C   Licensed under the GNU General Public License (GPL)
C
C   Authors:  Paul N. Swarztrauber and Richard A. Valent
C
C   $Id: sintmi.f,v 1.2 2006-11-21 01:10:20 haley Exp $
C
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC

      SUBROUTINE DSINTMI(N,WSAVE,LENSAV,IER)
      DOUBLE PRECISION PI
      DOUBLE PRECISION DT
      INTEGER N,LENSAV,IER
      DOUBLE PRECISION WSAVE(LENSAV)
C
      IER = 0
C
      IF (LENSAV.LT.N/2+N+INT(LOG(DBLE(N)))+4) THEN
          IER = 2
          CALL DXERFFT('SINTMI',3)
          GO TO 300
      END IF
C
      PI = 4.D0*ATAN(1.D0)
      IF (N.LE.1) RETURN
      NS2 = N/2
      NP1 = N + 1
      DT = PI/DBLE(NP1)
      DO 101 K = 1,NS2
          WSAVE(K) = 2.D0*SIN(K*DT)
  101 CONTINUE
      LNSV = NP1 + INT(LOG(DBLE(NP1))) + 4
      CALL DRFFTMI(NP1,WSAVE(NS2+1),LNSV,IER1)
      IF (IER1.NE.0) THEN
          IER = 20
          CALL DXERFFT('SINTMI',-5)
      END IF
C
  300 CONTINUE
      RETURN
      END
