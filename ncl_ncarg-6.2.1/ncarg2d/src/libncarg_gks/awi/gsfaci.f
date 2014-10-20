C
C	$Id: gsfaci.f,v 1.5 2008-07-27 00:21:01 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE GSFACI (COLI)
C
C  SET FILL AREA COLOR INDEX
C
      INTEGER ESFACI
      PARAMETER (ESFACI=38)
C
      include 'gkscom.h'
C
      INTEGER COLI
C
C  Check if GKS is in the proper state.
C
      CALL GZCKST(8,ESFACI,IER)
      IF (IER .NE. 0) RETURN
C
C  Check that the index is valid.
C
      IF (COLI .LT. 0) THEN
        ERS = 1
        CALL GERHND(92,ESFACI,ERF)
        ERS = 0
        RETURN
      ENDIF
C
C  Set the current color index in the GKS state list.
C
      CFACI = COLI
C
C  Invoke the workstation interface.
C
      FCODE = 40
      CONT  = 0
      CALL GZROI(0)
      IC1   = 1
      IC2   = 1
      IC(1) = COLI
      CALL GZTOWK
      IF (RERR .NE. 0) THEN
        ERS = 1
        CALL GERHND(RERR,ESFACI,ERF)
        ERS = 0
      ENDIF
C
      RETURN
      END
