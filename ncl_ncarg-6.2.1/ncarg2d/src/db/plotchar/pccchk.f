C
C $Id: pccchk.f,v 1.4 2008-07-27 01:04:31 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C

      SUBROUTINE PCCCHK (IERR)
C
C The subroutine PCCCHK does a short test for correctness of the
C complex character set.
C
C IERR is an output variable, taking on the following values:
C
C   0 if two short tests of the digitization retrieval process
C     were successful.
C   1 if the test was successful for 12-bit digitization units
C     but not successful for 6-bit digitization units.
C   2 if the test was successful for 6-bit digitization units
C     but not successful for 12-bit digitization units.
C   3 if neither of the two tests was successful.
C
C COMMON block declarations.
C
      COMMON /PCCMN3/ RDGU(7000)
C
C LCPC contains the correct digitization of the character '+'.
C
      DIMENSION LCPC(14)
C
C LCOC contains the correct digitization of the character defined by
C octal number 751.
C
      DIMENSION LCOC(30)
C
C The correct digitization of the character '+'.
C
      DATA LCPC / -13,13,0,9,0,-9,-2048,0,-9,0,9,0,-2048,0 /
C
C The correct digitization of the character defined by the octal number
C 751.
C
      DATA LCOC / -9,9,-5,39,-5,0,-5,-39,-2048,0,-4,39,-4,0,-4,-39,
     +            -2048,0,-5,39,6,39,-2048,0,-5,-39,6,-39,-2048,0 /
C
C Initialize the error flags for the two tests.
C
      IER1=0
      IER2=0
C
C Test 1.  Retrieve the digitization of the character '+' and compare
C it to the correct digitization.
C
      CALL PCEXCD (37,2,NDGU)
C
      IF (NDGU.NE.14) THEN
        IER1=1
      ELSE
        DO 101 I=1,14
          IF (I.GE.4) THEN
            IF (MOD(I,2).EQ.0) THEN
              IF (RDGU(I-1).NE.-2048.) RDGU(I)=RDGU(I)+1.5
            END IF
          END IF
          IF (RDGU(I).NE.REAL(LCPC(I))) THEN
            IER1=1
            GO TO 102
          END IF
  101   CONTINUE
      END IF
C
C Test 2.  Retrieve the digitization of the character with octal number
C 751 and compare it to the correct digitization.
C
  102 CALL PCEXCD (489,2,NDGU)
C
      IF (NDGU.NE.30) THEN
        IER2=2
      ELSE
        DO 103 I=1,30
          IF (I.GE.4) THEN
            IF (MOD(I,2).EQ.0) THEN
              IF (RDGU(I-1).NE.-2048.) RDGU(I)=RDGU(I)+1.5
            END IF
          END IF
          IF (RDGU(I).NE.REAL(LCOC(I))) THEN
            IER1=1
            GO TO 104
          END IF
  103   CONTINUE
      END IF
C
C Total error flag is the sum of the flags from tests 1 and 2.
C
  104 IERR=IER1+IER2
C
C Done.
C
      RETURN
C
      END
