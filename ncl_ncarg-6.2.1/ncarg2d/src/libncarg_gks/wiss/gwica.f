C
C	$Id: gwica.f,v 1.7 2008-07-27 00:21:07 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE GWICA
C
C  Process GKS CELL ARRAY elements, put out CGM CELL ARRAY.
C
      include 'gksin.h'
      include 'gwiio.h'
      include 'gwiins.h'
      include 'gwiwsl.h'
      include 'gwienu.h'
      include 'gwiopc.h'
C
      INTEGER  DX, DY, KALL, NBYPCW, ROWLEN, ROWPAD, NBYTES
      INTEGER  NBYTE1, NBYTE2, GPAD, GDFCP, GNORUN
      INTEGER  ROWPOS, IDPOS, NCLOUT, NBPROW, IX
C
      SAVE
C
      DATA  KALL/0/,  NBYPCW/2/,  GPAD/0/,  GDFCP/0/,  GNORUN/1/
C
      KALL = KALL+1
      IF (KALL .EQ. 1) THEN
C
C  Treat the first case (set-up call, P,Q,DX,DY passed,
C  first color indices processed).
C
C  If the picture is empty, send the clip indicator and rectangle;
C  set the WSL entry "DISPLAY SURFACE EMPTY" to "NOT EMPTY".
C
        IF (MDEMPT .EQ. GEMPTY) CALL GWICLP(1)
        MDEMPT = GNEMPT
C
C  Transform P and Q corner points to VDC, generate the
C  R corner point, store all in WPXPY.
C
        WPXPY(1) = MXOFF+INT(REAL(MXSCAL)*RX(1))
        WPXPY(2) = MXOFF+INT(REAL(MYSCAL)*RY(1))
        WPXPY(3) = MYOFF+INT(REAL(MXSCAL)*RX(2))
        WPXPY(4) = MYOFF+INT(REAL(MYSCAL)*RY(2))
        WPXPY(5) = WPXPY(3)
        WPXPY(6) = WPXPY(2)
C
C  Save DX,DY for subsequent calls.
C
        DX = ID(1)
        DY = ID(2)
C
C  Compute the total byte length -- rows must all start on
C  CGM word boundaries, including the first row.
C
C  Byte count for P,Q,R points, DX,DY, local color precision,
C  run-length/packed flag, round to nearest CGM metafile word.
C
        NBYTE1 = 1 + (6*MVDCFW + 2*MINTFW + MINTFW + MEFW -1)/8
        NBYTE2 = NBYPCW*( 1 + (NBYTE1-1)/NBYPCW )
C
C  Number of bytes to hold a row of color indexes, round to
C  CGM word boundary, compute padding bytes per row (if any).
C
        NBPROW = 1 + (DX*MCIXFW-1)/8
        ROWLEN = NBYPCW*( 1 + (NBPROW-1)/NBYPCW )
        ROWPAD = ROWLEN - NBPROW
C
C  Total byte length of cell array element.
C
        NBYTES = NBYTE2 + DY*ROWLEN
C
C  Put out opcode (class and ID) and total length.
C
        CALL GWPTNI (CLCARY, IDCARY, NBYTES, RERR)
C
C  Put out P, Q, R points.
C
        CALL GWPTPR (WPXPY, MVDCFW, 6, RERR)
C
C  Put out DX and DY, local color precision, packed/run-length flg.
C
        CALL GWPTPR (ID, MINTFW, 2, RERR)
        CALL GWPTPR (GDFCP,  MINTFW, 1, RERR)
        CALL GWPTPR (GNORUN, MEFW, 1, RERR)
C
C  Pad to start of first row if necessary.
C
        IF (NBYTE1 .LT. NBYTE2)  THEN
           DO 10 IX=1, (NBYTE2-NBYTE1)
              CALL GWPTPR (GPAD, 8, 1, RERR)
 10        CONTINUE
        END IF
C
        IF (RERR .NE. 0) RETURN
C
C  Initialize pointer into current row.
C
        ROWPOS = 1
C
C  Begin processing of color index array.
C
C
C  Initialize pointer into current chunk of cell array.
C
        IDPOS  = 1
C
   25   CONTINUE
C
C  Number of cells to put out, to end of current row or
C  all cells remaining, whichever is less.
C
        NCLOUT = MIN (DX-ROWPOS+1, IC2-IDPOS+1)
        CALL GWPTPR (IC(IDPOS), MCIXFW, NCLOUT, RERR)
        IF (RERR .NE. 0)  RETURN
C
C  Update row position, pad to CGM word boundary if
C  just completed a row (next GPUTPR starts new row).
C
        ROWPOS = ROWPOS + NCLOUT
        IF (ROWPOS .GT. DX)  ROWPOS = 1
        IF (ROWPAD.GT.0 .AND. ROWPOS.EQ.1)  THEN
          DO 35 IX=1,ROWPAD
            CALL GWPTPR (GPAD, 8, 1, RERR)
   35     CONTINUE
        END IF
C
C  Update position in index array.
C
        IDPOS  = IDPOS  + NCLOUT
C
C  Go back to process next row (or partial row) if more
C  cells remain in this call.
C
        IF (IDPOS .LE. IC2)  GOTO 25
C
C  Processing of first call complete.
C
      END IF
C
C  Treat the continuation calls (the color index array).
C
      IF (KALL .GT. 1) THEN
C
C  Initialize pointer into current chunk of cell array.
C
        IDPOS  = 1
C
   20   CONTINUE
C
C  Number of cells to put out, to end of current row or
C  all cells remaining, whichever is less.
C
        NCLOUT = MIN (DX-ROWPOS+1, IC2-IDPOS+1)
        CALL GWPTPR (IC(IDPOS), MCIXFW, NCLOUT, RERR)
        IF (RERR .NE. 0)  RETURN
C
C  Update row position, pad to CGM word boundary if
C  just completed a row (next GPUTPR starts new row).
C
        ROWPOS = ROWPOS + NCLOUT
        IF (ROWPOS .GT. DX)  ROWPOS = 1
        IF (ROWPAD.GT.0 .AND. ROWPOS.EQ.1)  THEN
          DO 30 IX=1, ROWPAD
            CALL GWPTPR (GPAD, 8, 1, RERR)
   30     CONTINUE
        END IF
C
C  Update position in index array.
C
        IDPOS = IDPOS+NCLOUT
C
C  Go back to process next row (or partial row) if more
C  cells remain this call.
C
        IF (IDPOS .LE. IC2)  GOTO 20
C
      END IF
C
      IF (CONT .EQ. 0)  KALL = 0
C
      RETURN
C
      END
