C
C	$Id: drawpv.f,v 1.6 2008-07-27 00:23:04 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE DRAWPV (IX,IY,IND)
C
C DRAWPV INTERCEPTS THE CALL TO PLOTIT.
C FOR IND=1 (PEN DOWN) IT CHECKS IF A LINE (PART OF IT) HAS TO BE DRAWN
C OR REMOVED.
C FOR IND=0 (PEN UP) IT CHECKS IF THE PEN HAS TO BE MOVED OR IF IT IS
C ALREADY CLOSE ENOUGH TO THE WANTED POSITION.
C FOR IND=2 DO NOT MOVE PEN.JUST SET POSITION OF PEN TO (IMPOS,IMPOS).
C
      COMMON/INTPR/IPAU,FPART,TENSN,NP,SMALL,L1,ADDLR,ADDTB,MLLINE,
     1    ICLOSE
C IOFFS IS AN INTERNAL PARAMETER. IT IS REFERENCED IN  DRAWPV AND
C FDVDLD AND INITIALIZED IN DASHBDX.
C
      COMMON /SMFLAG/ IOFFS
C
C IN DSAVE3 THE STARTING POINT OF THE NEXT LINE SEGMENT TO BE DRAWN
C OR REMOVED IS SAVED.
C
      COMMON /DSAVE3/ IXSTOR,IYSTOR
C
C IX1,IY1,IX2 AND IY2 ARE USED TO STORE THE NEXT LINE SEGMENT TO BE
C MARKED, WHICH IS THE SEGMENT THAT WAS DRAWN LAST.
C
      COMMON /DSAVE4/ IX1,IY1,IX2,IY2
C
C ISKIP IS USED TO ADJUST DASHSUPR TO THE SIZE OF THE MODEL PICTURE AS
C DEFINED IN SUBROUTINE REMOVE. ISKIP IS INITIALIZED IN DASHBDX.
C
      COMMON /DSUP1/ ISKIP
C
      LOGICAL LI, LM, HID
      LOGICAL LDUMMY
      SAVE
C
      DATA LDUMMY /.TRUE./
C
C BRANCH DEPENDING ON FUNCTION TO BE PERFORMED.
C
      IIND = IND + 1
      GOTO (2,1,3), IIND
C
    1 CONTINUE
C
C *************************************
C
C INTERCEPT PLOTIT(IX,IY,1) TO CHECK IF THE LINE TO BE DRAWN (PART OF
C IT) HAS TO BE REMOVED. DRAW AND MARK LINE IF FEASIBLE.
C
C
      IF (IOFFS .EQ. 0) GO TO 10
C
C DO NOT MARK LINES IF NOT REQUIRED.
C
      IXSTOR = IX
      IYSTOR = IY
      CALL PLOTIT (IX,IY,1)
      RETURN
C
C LINE HAS TO BE DRAWN AND MARKED.
C
   10 CONTINUE
      MX = IXSTOR
      MY = IYSTOR
      IXSTOR = IX
      IYSTOR = IY
      DX = IXSTOR - MX
      DY = IYSTOR - MY
C
C CHECK BEGINNING AND END POINT OF LINE, IF THEY ARE ALREADY MARKED.
C
      CALL REMOVE (IXSTOR,IYSTOR,LI,3)
      CALL REMOVE (MX,MY,LM,3)
C
C NOTHING TO DO , IF BEGINNING AND END POINT ARE ALREADY MARKED.
C
      IF (LI .AND. LM) RETURN
C
C DRAW A POINT IF LINE IS OF LENGTH 0.
C
      IF (DX .NE. 0. .OR. DY .NE. 0.) GO TO 15
      CALL PLOTIT (IXSTOR,IYSTOR,1)
      RETURN
   15 CONTINUE
C
      IF (LI .OR. LM) GO TO 20
C
C IF NEITHER STARTING NOR END POINT IS MARKED, DRAW LINE AND MARK IT.
C
      CALL PLOTIT (IXSTOR,IYSTOR,1)
      GO TO 80
C
C EITHER BEGINNING OR END POINT OF LINE WERE ALREADY MARKED.
C
   20 D = MAX(ABS(DX),ABS(DY))
      ID = D/REAL(ISKIP)
      DX = (DX/D)*REAL(ISKIP)
      DY = (DY/D)*REAL(ISKIP)
      IF (LM) GO TO 50
C
C STARTING POINT WAS NOT MARKED. FIND THE FIRST MARKED POINT.
C
      X = MX
      Y = MY
      DO 30 I=1,ID
         X = X+DX
         Y = Y+DY
         CALL REMOVE (INT(X+.5),INT(Y+.5),HID,3)
         IF (HID) GO TO 40
   30 CONTINUE
      X = REAL(IXSTOR)+DX
      Y = REAL(IYSTOR)+DY
   40 IXP = X+.5-DX
      IYP = Y+.5-DY
C FIRST MARKED POINT FOUND.
C DRAW LINE, SET PEN, MARK LINE.
      CALL PLOTIT (IXP,IYP,1)
      CALL PLOTIT (IXSTOR,IYSTOR,0)
      GO TO 80
C
C     FIRST PART IS HIDDEN
C       FIND THE FIRST VISIBLE POINT
C
   50 X = MX
      Y = MY
      DO 60 I=1,ID
         X = X+DX
         Y = Y+DY
         CALL REMOVE (INT(X+.5),INT(Y+.5),HID,3)
         IF (.NOT.HID) GO TO 70
   60 CONTINUE
      X = IXSTOR
      Y = IYSTOR
   70 IXP = X+.5
      IYP = Y+.5
C FIRST VISIBLE POINT FOUND.
C SET PEN, DRAW LINE, MARK LINE.
      CALL PLOTIT (IXP,IYP,0)
      CALL PLOTIT (IXSTOR,IYSTOR,1)
C
C
   80 CONTINUE
      CALL MARKL (IX1,IY1,IX2,IY2)
      IX1 = MX
      IY1 = MY
      IX2 = IXSTOR
      IY2 = IYSTOR
      RETURN
C
    2 CONTINUE
C
C *************************************
C
C INTERCEPT PLOTIT(IX,IY,0) TO SET THE STARTING POINT FOR THE NEXT
C LINE TO BE DRAWN OR REMOVED.
C
C CHECK IF PEN IS ALREADY CLOSE ENOUGH TO THE WANTED POSITION.
      DIFF = REAL(ABS(IXSTOR-IX)+ABS(IYSTOR-IY))
      IF (DIFF .LE. REAL(ICLOSE)) GO TO 110
C
      IXSTOR = IX
      IYSTOR = IY
      CALL PLOTIT (IXSTOR,IYSTOR,0)
      RETURN
C
    3 CONTINUE
C
C ****************************
C
C DO NOT MOVE PEN. JUST SET POSITION OF PEN TO (IMPOS,IMPOS).
C
      IXSTOR = IX
      IYSTOR = IY
C
  110 CONTINUE
C
      RETURN
      END