C
C	$Id: gwpart.f,v 1.4 2008-07-27 00:21:08 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE GWPART(NBYTES,GKSERR)
C
C  This subroutine loads the next partition of an active instruction.
C
C  OUTPUT
C    GKSERR - The error status flag.
C
C  All data is type INTEGER unless otherwise indicated.
C
      IMPLICIT INTEGER (A-Z)
C
      include 'gwiins.h'
      include 'gwiio.h'
C
      SAVE
C
C  Define the CGM data element partition size.  To make the arithmetic
C  come out right in other parts of the code, this should be a multiple
C  of 256.
C
      DATA PARSIZ/32256/
C
      DATA ALLOK/0/
C
C  Define the short format length, short format count, long format flag,
C  continue flag on, continue flag off, continue length, long format
C  length.
C
      DATA CONON,CONOFF,CFMLNG,LFMLNG
     +    /    1,     0,     1,    15/
C
      GKSERR = ALLOK
C
C  Set the current partition byte count and the remainder byte count.
C
      IF (NBYTES .GT. PARSIZ) THEN
        MCCBYT = PARSIZ
        MCNBYT = NBYTES - PARSIZ
      ELSE
        MCCBYT = NBYTES
        MCNBYT = 0
      END IF
C
C  Set the continue flag.
C
      IF (MCNBYT .NE. 0) THEN
C
C  There is another partition.
C
        CALL GWILOD(CONON,CFMLNG,1,GKSERR)
      ELSE
C
C  Last partition.
C
        CALL GWILOD(CONOFF,CFMLNG,1,GKSERR)
      END IF
C
      IF (GKSERR .NE. ALLOK) RETURN
C
C  Set the long format operand list size.
C
      CALL GWILOD(MCCBYT,LFMLNG,1,GKSERR)
C
      RETURN
      END
