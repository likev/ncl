C
C $Id: mpipar.f,v 1.8 2008-07-27 00:17:07 haley Exp $
C
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      INTEGER FUNCTION MPIPAR (IAIN)
        INTEGER IAIN
        MPIPAR=MDIPAR(IAIN)
        RETURN
      END
