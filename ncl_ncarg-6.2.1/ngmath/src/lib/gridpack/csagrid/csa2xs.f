C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE CSA2XS (NI,XI,UI,WTS,KNOTS,SSMTH,NDERIV,
     +                   NXO,NYO,XO,YO,UO,NWRK,WORK,IER)
C
      DIMENSION XI(2,NI),UI(NI),WTS(*),KNOTS(2),XO(NXO),YO(NYO),
     +          UO(NXO,NYO),WORK(NWRK),NDERIV(2)
      DIMENSION XMN(2),XMX(2),XV(2)
C
C  Check on the number of knots.
C
      NTOT = KNOTS(1)*KNOTS(2)
C
      DO 20 I=1,2
        IF (KNOTS(I) .LT. 4) THEN
          CALL CFAERR (202,' CSA2XS - must have at least four knots in e
     +very coordinate direction',68)       
          IER = 202
          RETURN
        ENDIF
   20 CONTINUE
C
C  Check on the size of the workspace.
C
      IF (NWRK .LT. NTOT*(NTOT+3)) THEN
        CALL CFAERR (203,' CSA2XS - workspace too small',28)
        IER = 203
        RETURN
      ENDIF
C
C  Calculate the min and max for the knots as the minimum value of
C  the input data and output data and the maximum of the input and
C  output data.
C
      DO 30 J=1,2
        XMN(J) = XI(J,1)
        XMX(J) = XI(J,1)
        DO 10 I=2,NI
          XMN(J) = MIN(XMN(J),XI(J,I))
          XMX(J) = MAX(XMX(J),XI(J,I))
   10   CONTINUE
   30 CONTINUE
      XMN(1) = MIN(XMN(1),XO(1))
      XMX(1) = MAX(XMX(1),XO(NXO))
      XMN(2) = MIN(XMN(2),YO(1))
      XMX(2) = MAX(XMX(2),YO(NYO))
C
C  Find the coefficients.
C
      CALL SPLCW(2,XI,2,UI,WTS,NI,XMN,XMX,KNOTS,SSMTH,WORK,NTOT,       
     +           WORK(NTOT+1),NWRK-NTOT,IERR)
      IF (IERR .NE. 0) RETURN
C
C  Calculate the approximated values (coefficients are stored at
C  the beginnig of WORK).
C
      DO 60 I=1,NXO
        XV(1) = XO(I)
        DO 40 J=1,NYO
          XV(2) = YO(J)
          UO(I,J) = SPLDE(2,XV,NDERIV,WORK,XMN,XMX,KNOTS,IER)
          IF (IERR .NE. 0) RETURN
   40   CONTINUE
   60 CONTINUE
C
      RETURN
      END
