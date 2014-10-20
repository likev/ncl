CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
C
C   FFTPACK 5.0
C   Copyright (C) 1995-2004, Scientific Computing Division,
C   University Corporation for Atmospheric Research
C   Licensed under the GNU General Public License (GPL)
C
C   Authors:  Paul N. Swarztrauber and Richard A. Valent
C
C   $Id: cfft1b.f,v 1.2 2006-11-21 01:10:15 haley Exp $
C
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC

      SUBROUTINE DCFFT1B(N,INC,C,LENC,WSAVE,LENSAV,WORK,LENWRK,IER)
      INTEGER N,INC,LENC,LENSAV,LENWRK,IER
      DOUBLE COMPLEX C(LENC)
      DOUBLE PRECISION WSAVE(LENSAV),WORK(LENWRK)
C
      IER = 0
C
      IF (LENC.LT.INC* (N-1)+1) THEN
          IER = 1
          CALL DXERFFT('CFFT1B ',6)
      ELSE IF (LENSAV.LT.2*N+INT(LOG(DBLE(N)))+4) THEN
          IER = 2
          CALL DXERFFT('CFFT1B ',8)
      ELSE IF (LENWRK.LT.2*N) THEN
          IER = 3
          CALL DXERFFT('CFFT1B ',10)
      END IF
C
      IF (N.EQ.1) RETURN
C
      IW1 = N + N + 1
      CALL DC1FM1B(N,INC,C,WORK,WSAVE,WSAVE(IW1),WSAVE(IW1+1))
      RETURN
      END