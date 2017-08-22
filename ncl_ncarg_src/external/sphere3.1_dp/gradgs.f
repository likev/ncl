c
c  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
c  .                                                             .
c  .                  copyright (c) 1998 by UCAR                 .
c  .                                                             .
c  .       University Corporation for Atmospheric Research       .
c  .                                                             .
c  .                      all rights reserved                    .
c  .                                                             .
c  .                                                             .
c  .                         SPHEREPACK                          .
c  .                                                             .
c  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
c
c
c ... file gradgs.f
c
c     this file includes documentation and code for
c     subroutine gradgs         i
c
c ... files which must be loaded with gradgec.f
c
c     sphcom.f, hrfft.f, shags.f,vhsgs.f
c
c     subroutine gradgs(nlat,nlon,isym,nt,v,w,idvw,jdvw,a,b,mdab,ndab,
c    +                  wvhsgs,lvhsgs,work,lwork,ierror)
c
C*PL*ERROR* Comment line too long
c     given the scalar spherical harmonic coefficients a and b, precomputed
C*PL*ERROR* Comment line too long
c     by subroutine shags for a scalar field sf, subroutine gradgs computes
c     an irrotational vector field (v,w) such that
c
c           gradient(sf) = (v,w).
c
c     v is the colatitudinal and w is the east longitudinal component
c     of the gradient.  i.e.,
c
c            v(i,j) = d(sf(i,j))/dtheta
c
c     and
c
c            w(i,j) = 1/sint*d(sf(i,j))/dlambda
c
c     at the gaussian colatitude point theta(i) (see nlat as input
c     parameter) and longitude lambda(j) = (j-1)*2*pi/nlon where
c     sint = sin(theta(i)).
c
c
c     input parameters
c
c     nlat   the number of points in the gaussian colatitude grid on the
C*PL*ERROR* Comment line too long
c            full sphere. these lie in the interval (0,pi) and are computed
C*PL*ERROR* Comment line too long
c            in radians in theta(1) <...< theta(nlat) by subroutine gaqd.
C*PL*ERROR* Comment line too long
c            if nlat is odd the equator will be included as the grid point
c            theta((nlat+1)/2).  if nlat is even the equator will be
c            excluded as a grid point and will lie half way between
c            theta(nlat/2) and theta(nlat/2+1). nlat must be at least 3.
c            note: on the half sphere, the number of grid points in the
c            colatitudinal direction is nlat/2 if nlat is even or
c            (nlat+1)/2 if nlat is odd.
c
c     nlon   the number of distinct londitude points.  nlon determines
c            the grid increment in longitude as 2*pi/nlon. for example
c            nlon = 72 for a five degree grid. nlon must be greater than
c            3.  the efficiency of the computation is improved when nlon
c            is a product of small prime numbers.
c
c
c     isym   this has the same value as the isym that was input to
c            subroutine shags to compute the arrays a and b from the
c            scalar field sf.  isym determines whether (v,w) are
c            computed on the full or half sphere as follows:
c
c      = 0
c
c           sf is not symmetric about the equator. in this case
c           the vector field (v,w) is computed on the entire sphere.
c           i.e., in the arrays  v(i,j),w(i,j) for i=1,...,nlat and
c           j=1,...,nlon.
c
c      = 1
c
c           sf is antisymmetric about the equator. in this case w is
c           antisymmetric and v is symmetric about the equator. w
c           and v are computed on the northern hemisphere only.  i.e.,
c           if nlat is odd they are computed for i=1,...,(nlat+1)/2
c           and j=1,...,nlon.  if nlat is even they are computed for
c           i=1,...,nlat/2 and j=1,...,nlon.
c
c      = 2
c
c           sf is symmetric about the equator. in this case w is
c           symmetric and v is antisymmetric about the equator. w
c           and v are computed on the northern hemisphere only.  i.e.,
c           if nlat is odd they are computed for i=1,...,(nlat+1)/2
c           and j=1,...,nlon.  if nlat is even they are computed for
c           i=1,...,nlat/2 and j=1,...,nlon.
c
c
c     nt     nt is the number of scalar and vector fields.  some
c            computational efficiency is obtained for multiple fields.
C*PL*ERROR* Comment line too long
c            the arrays a,b,v, and w can be three dimensional corresponding
c            to an indexed multiple array sf.  in this case, multiple
c            vector synthesis will be performed to compute each vector
c            field.  the third index for a,b,v, and w is the synthesis
c            index which assumes the values k = 1,...,nt.  for a single
c            synthesis set nt = 1.  the description of the remaining
C*PL*ERROR* Comment line too long
c            parameters is simplified by assuming that nt=1 or that a,b,v,
c            and w are two dimensional arrays.
c
c     idvw   the first dimension of the arrays v,w as it appears in
c            the program that calls gradgs. if isym = 0 then idvw
c            must be at least nlat.  if isym = 1 or 2 and nlat is
c            even then idvw must be at least nlat/2. if isym = 1 or 2
c            and nlat is odd then idvw must be at least (nlat+1)/2.
c
c     jdvw   the second dimension of the arrays v,w as it appears in
c            the program that calls gradgs. jdvw must be at least nlon.
c
c     a,b    two or three dimensional arrays (see input parameter nt)
c            that contain scalar spherical harmonic coefficients
C*PL*ERROR* Comment line too long
c            of the scalar field array sf as computed by subroutine shags.
c     ***    a,b must be computed by shags prior to calling gradgs.
c
c     mdab   the first dimension of the arrays a and b as it appears in
c            the program that calls gradgs (and shags). mdab must be at
c            least min0(nlat,(nlon+2)/2) if nlon is even or at least
c            min0(nlat,(nlon+1)/2) if nlon is odd.
c
c     ndab   the second dimension of the arrays a and b as it appears in
c            the program that calls gradgs (and shags). ndab must be at
c            least nlat.
c
c
c     wvhsgs an array which must be initialized by subroutine vhsgsi.
c            once initialized,
c            wvhsgs can be used repeatedly by gradgs as long as nlon
c            and nlat remain unchanged.  wvhsgs must not be altered
c            between calls of gradgs.
c
c
c     lvhsgs the dimension of the array wvhsgs as it appears in the
c            program that calls grradgs.  define
c
c               l1 = min0(nlat,nlon/2) if nlon is even or
c               l1 = min0(nlat,(nlon+1)/2) if nlon is odd
c
c            and
c
c               l2 = nlat/2        if nlat is even or
c               l2 = (nlat+1)/2    if nlat is odd
c
c            then lvhsgs must be at least
c
c                 l1*l2*(nlat+nlat-l1+1)+nlon+15+2*nlat
c
c
c     work   a work array that does not have to be saved.
c
c     lwork  the dimension of the array work as it appears in the
c            program that calls gradgs. define
c
c               l1 = min0(nlat,nlon/2) if nlon is even or
c               l1 = min0(nlat,(nlon+1)/2) if nlon is odd
c
c            and
c
c               l2 = nlat/2                  if nlat is even or
c               l2 = (nlat+1)/2              if nlat is odd
c
c            if isym = 0, lwork must be greater than or equal to
c
c               nlat*((2*nt+1)*nlon+2*l1*nt+1).
c
c            if isym = 1 or 2, lwork must be greater than or equal to
c
c               (2*nt+1)*l2*nlon+nlat*(2*l1*nt+1).
c
c
c     **************************************************************
c
c     output parameters
c
c
C*PL*ERROR* Comment line too long
c     v,w   two or three dimensional arrays (see input parameter nt) that
C*PL*ERROR* Comment line too long
c           contain an irrotational vector field such that the gradient of
c           the scalar field sf is (v,w).  w(i,j) is the east longitude
C*PL*ERROR* Comment line too long
c           component and v(i,j) is the colatitudinal component of velocity
C*PL*ERROR* Comment line too long
c           at gaussian colatitude and longitude lambda(j) = (j-1)*2*pi/nlon
c           the indices for v and w are defined at the input parameter
C*PL*ERROR* Comment line too long
c           isym.  the vorticity of (v,w) is zero.  note that any nonzero
C*PL*ERROR* Comment line too long
c           vector field on the sphere will be multiple valued at the poles
c           [reference swarztrauber].
c
c
c  ierror   = 0  no errors
c           = 1  error in the specification of nlat
c           = 2  error in the specification of nlon
c           = 3  error in the specification of isym
c           = 4  error in the specification of nt
c           = 5  error in the specification of idvw
c           = 6  error in the specification of jdvw
c           = 7  error in the specification of mdab
c           = 8  error in the specification of ndab
c           = 9  error in the specification of lvhsgs
c           = 10 error in the specification of lwork
c **********************************************************************
c
c
      SUBROUTINE DGRADGS(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,A,B,MDAB,NDAB,
     +                  WVHSGS,LVHSGS,WORK,LWORK,IERROR)
      DOUBLE PRECISION V
      DOUBLE PRECISION W
      DOUBLE PRECISION A
      DOUBLE PRECISION B
      DOUBLE PRECISION WVHSGS
      DOUBLE PRECISION WORK
      DIMENSION V(IDVW,JDVW,NT),W(IDVW,JDVW,NT)
      DIMENSION A(MDAB,NDAB,NT),B(MDAB,NDAB,NT)
      DIMENSION WVHSGS(LVHSGS),WORK(LWORK)
c
c     check input parameters
c
      IERROR = 1
      IF (NLAT.LT.3) RETURN
      IERROR = 2
      IF (NLON.LT.4) RETURN
      IERROR = 3
      IF (ISYM.LT.0 .OR. ISYM.GT.2) RETURN
      IERROR = 4
      IF (NT.LT.0) RETURN
      IERROR = 5
      IMID = (NLAT+1)/2
      IF ((ISYM.EQ.0.AND.IDVW.LT.NLAT) .OR.
     +    (ISYM.NE.0.AND.IDVW.LT.IMID)) RETURN
      IERROR = 6
      IF (JDVW.LT.NLON) RETURN
      IERROR = 7
      MMAX = MIN0(NLAT, (NLON+1)/2)
      IF (MDAB.LT.MIN0(NLAT, (NLON+2)/2)) RETURN
      IERROR = 8
      IF (NDAB.LT.NLAT) RETURN
      IERROR = 9
c
c     verify minimum saved work space length
c
      IDZ = (MMAX* (NLAT+NLAT-MMAX+1))/2
      LZIMN = IDZ*IMID
      LGDMIN = LZIMN + LZIMN + NLON + 15
      IF (LVHSGS.LT.LGDMIN) RETURN
      IERROR = 10
c
c     verify minimum unsaved work space length
c
      MN = MMAX*NLAT*NT
      IDV = NLAT
      IF (ISYM.NE.0) IDV = IMID
      LNL = NT*IDV*NLON
      LWKMIN = LNL + LNL + IDV*NLON + 2*MN + NLAT
      IF (LWORK.LT.LWKMIN) RETURN
      IERROR = 0
c
c     set work space pointers
c
      IBR = 1
      IBI = IBR + MN
      IS = IBI + MN
      IWK = IS + NLAT
      LIWK = LWORK - 2*MN - NLAT
      CALL DGRADGS1(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,WORK(IBR),WORK(IBI),
     +             MMAX,WORK(IS),MDAB,NDAB,A,B,WVHSGS,LVHSGS,WORK(IWK),
     +             LIWK,IERROR)
      RETURN
      END

      SUBROUTINE DGRADGS1(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,BR,BI,MMAX,
     +                   SQNN,MDAB,NDAB,A,B,WVHSGS,LVHSGS,WK,LWK,IERROR)
      DOUBLE PRECISION V
      DOUBLE PRECISION W
      DOUBLE PRECISION BR
      DOUBLE PRECISION BI
      DOUBLE PRECISION SQNN
      DOUBLE PRECISION A
      DOUBLE PRECISION B
      DOUBLE PRECISION WVHSGS
      DOUBLE PRECISION WK
      DOUBLE PRECISION FN
      DOUBLE PRECISION CR
      DOUBLE PRECISION CI
      DIMENSION V(IDVW,JDVW,NT),W(IDVW,JDVW,NT)
      DIMENSION BR(MMAX,NLAT,NT),BI(MMAX,NLAT,NT),SQNN(NLAT)
      DIMENSION A(MDAB,NDAB,NT),B(MDAB,NDAB,NT)
      DIMENSION WVHSGS(LVHSGS),WK(LWK)
c
c     preset coefficient multiplyers in vector
c
      DO 1 N = 2,NLAT
          FN = DBLE(N-1)
          SQNN(N) = SQRT(FN* (FN+1.D0))
    1 CONTINUE
c
c     compute multiple vector fields coefficients
c
      DO 2 K = 1,NT
c
c     preset br,bi to 0.0
c
          DO 3 N = 1,NLAT
              DO 4 M = 1,MMAX
                  BR(M,N,K) = 0.0D0
                  BI(M,N,K) = 0.0D0
    4         CONTINUE
    3     CONTINUE
c
c     compute m=0 coefficients
c
          DO 5 N = 2,NLAT
              BR(1,N,K) = SQNN(N)*A(1,N,K)
              BI(1,N,K) = SQNN(N)*B(1,N,K)
    5     CONTINUE
c
c     compute m>0 coefficients
c
          DO 6 M = 2,MMAX
              DO 7 N = M,NLAT
                  BR(M,N,K) = SQNN(N)*A(M,N,K)
                  BI(M,N,K) = SQNN(N)*B(M,N,K)
    7         CONTINUE
    6     CONTINUE
    2 CONTINUE
c
c     set ityp for irrotational vector synthesis to compute gradient
c
      IF (ISYM.EQ.0) THEN
          ITYP = 1
      ELSE IF (ISYM.EQ.1) THEN
          ITYP = 4
      ELSE IF (ISYM.EQ.2) THEN
          ITYP = 7
      END IF
c
c     vector sythesize br,bi into (v,w) (cr,ci are dummy variables)
c
      CALL DVHSGS(NLAT,NLON,ITYP,NT,V,W,IDVW,JDVW,BR,BI,CR,CI,MMAX,NLAT,
     +           WVHSGS,LVHSGS,WK,LWK,IERROR)
      RETURN
      END
