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
c
c ... file idvtgs.f
c
c     this file includes documentation and code for
c     subroutine idvtgs         i
c
c ... files which must be loaded with idvtgs.f
c
c     sphcom.f, hrfft.f, vhsgs.f,shags.f, gaqd.f
c
c
c     subroutine idvtgs(nlat,nlon,isym,nt,v,w,idvw,jdvw,ad,bd,av,bv,
c    +mdab,ndab,wvhsgs,lvhsgs,work,lwork,pertbd,pertbv,ierror)
c
c     given the scalar spherical harmonic coefficients ad,bd precomputed
C*PL*ERROR* Comment line too long
c     by subroutine shags for the scalar field divg and coefficients av,bv
C*PL*ERROR* Comment line too long
c     precomputed by subroutine shags for the scalar field vort, subroutine
C*PL*ERROR* Comment line too long
c     idvtgs computes a vector field (v,w) whose divergence is divg - pertbd
C*PL*ERROR* Comment line too long
c     and whose vorticity is vort - pertbv.  w the is east longitude component
C*PL*ERROR* Comment line too long
c     and v is the colatitudinal component of the velocity.  if nt=1 (see nt
C*PL*ERROR* Comment line too long
c     below) pertrbd and pertbv are constants which must be subtracted from
C*PL*ERROR* Comment line too long
c     divg and vort for (v,w) to exist (see the description of pertbd and
C*PL*ERROR* Comment line too long
c     pertrbv below).  usually pertbd and pertbv are zero or small relative
C*PL*ERROR* Comment line too long
c     to divg and vort.  w(i,j) and v(i,j) are the velocity components at
C*PL*ERROR* Comment line too long
c     gaussian colatitude theta(i) (see nlat as input argument) and longitude
c     lambda(j) = (j-1)*2*pi/nlon
c
c     the
c
c            divergence(v(i,j),w(i,j))
c
c         =  [d(sint*v)/dtheta + dw/dlambda]/sint
c
c         =  divg(i,j) - pertbd
c
c     and
c
c            vorticity(v(i,j),w(i,j))
c
c         =  [-dv/dlambda + d(sint*w)/dtheta]/sint
c
c         =  vort(i,j) - pertbv
c
c     where
c
c            sint = cos(theta(i)).
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
c            nlon = 72 for a five degree grid. nlon must be greater
c            than 3. the axisymmetric case corresponds to nlon=1.
c            the efficiency of the computation is improved when nlon
c            is a product of small prime numbers.
c
c
C*PL*ERROR* Comment line too long
c     isym   isym determines whether (v,w) are computed on the full or half
c            sphere as follows:
c
c      = 0
c            divg,vort are neither pairwise symmetric/antisymmetric nor
c            antisymmetric/symmetric about the equator as described for
C*PL*ERROR* Comment line too long
c            isym = 1 or isym = 2  below.  in this case, the vector field
C*PL*ERROR* Comment line too long
c            (v,w) is computed on the entire sphere.  i.e., in the arrays
c            w(i,j) and v(i,j) i=1,...,nlat and j=1,...,nlon.
c
c      = 1
c
C*PL*ERROR* Comment line too long
c            divg is antisymmetric and vort is symmetric about the equator.
C*PL*ERROR* Comment line too long
c            in this case w is antisymmetric and v is symmetric about the
C*PL*ERROR* Comment line too long
c            equator.  w and v are computed on the northern hemisphere only.
c            if nlat is odd they are computed for i=1,...,(nlat+1)/2
c            and j=1,...,nlon.  if nlat is even they are computed for
c            i=1,...,nlat/2 and j=1,...,nlon.
c
c      = 2
c
C*PL*ERROR* Comment line too long
c            divg is symmetric and vort is antisymmetric about the equator.
C*PL*ERROR* Comment line too long
c            in this case w is symmetric and v is antisymmetric about the
C*PL*ERROR* Comment line too long
c            equator.  w and v are computed on the northern hemisphere only.
c            if nlat is odd they are computed for i=1,...,(nlat+1)/2
c            and j=1,...,nlon.  if nlat is even they are computed for
c            i=1,...,nlat/2 and j=1,...,nlon.
c
c
C*PL*ERROR* Comment line too long
c     nt     in the program that calls idvtgs, nt is the number of scalar
C*PL*ERROR* Comment line too long
c            and vector fields.  some computational efficiency is obtained
C*PL*ERROR* Comment line too long
c            for multiple fields.  the arrays ad,bd,av,bv,u, and v can be
c            three dimensional and pertbd,pertbv can be one dimensional
C*PL*ERROR* Comment line too long
c            corresponding to indexed multiple arrays divg, vort.  in this
c            case, multiple synthesis will be performed to compute each
C*PL*ERROR* Comment line too long
c            vector field.  the third index for ad,bd,av,bv,v,w and first
C*PL*ERROR* Comment line too long
c            pertrbd,pertbv is the synthesis index which assumes the values
C*PL*ERROR* Comment line too long
c            k=1,...,nt.  for a single synthesis set nt=1. the description of
C*PL*ERROR* Comment line too long
c            remaining parameters is simplified by assuming that nt=1 or that
c            ad,bd,av,bv,v,w are two dimensional and pertbd,pertbv are
c            constants.
c
c     idvw   the first dimension of the arrays v,w as it appears in
c            the program that calls idvtgs. if isym = 0 then idvw
c            must be at least nlat.  if isym = 1 or 2 and nlat is
c            even then idvw must be at least nlat/2. if isym = 1 or 2
c            and nlat is odd then idvw must be at least (nlat+1)/2.
c
c     jdvw   the second dimension of the arrays v,w as it appears in
c            the program that calls idvtgs. jdvw must be at least nlon.
c
c     ad,bd  two or three dimensional arrays (see input parameter nt)
c            that contain scalar spherical harmonic coefficients
C*PL*ERROR* Comment line too long
c            of the divergence array divg as computed by subroutine shags.
c
c     av,bv  two or three dimensional arrays (see input parameter nt)
c            that contain scalar spherical harmonic coefficients
C*PL*ERROR* Comment line too long
c            of the vorticity array vort as computed by subroutine shags.
C*PL*ERROR* Comment line too long
c     ***    ad,bd,av,bv must be computed by shags prior to calling idvtgs.
c
c     mdab   the first dimension of the arrays ad,bd,av,bv as it appears
C*PL*ERROR* Comment line too long
c            in the program that calls idvtgs (and shags). mdab must be at
c            least min0(nlat,(nlon+2)/2) if nlon is even or at least
c            min0(nlat,(nlon+1)/2) if nlon is odd.
c
C*PL*ERROR* Comment line too long
c     ndab   the second dimension of the arrays ad,bd,av,bv as it appears in
c            the program that calls idvtgs (and shags). ndab must be at
c            least nlat.
c
c  wvhsgs    an array which must be initialized by subroutine vhsgsi.
c            wvhsgs can be used repeatedly by idvtgs as long as nlon
c            and nlat remain unchanged.  wvhsgs must not be altered
c            between calls of idvtgs.
c
c
c  lvhsgs    the dimension of the array wvhsgs as it appears in the
c            program that calls idvtgs. define
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
c               l1*l2*(nlat+nlat-l1+1)+nlon+15+2*nlat
c
c     work   a work array that does not have to be saved.
c
c     lwork  the dimension of the array work as it appears in the
c            program that calls idvtgs. define
c
c               l2 = nlat/2                    if nlat is even or
c               l2 = (nlat+1)/2                if nlat is odd
c               l1 = min(nlat,nlon/2+1)        if nlon is even or
c               l1 = min(nlat,(nlon+1)/2)      if nlon is odd
c
c
c            if isym = 0 then lwork must be at least
c
c                       nlat*((2*nt+1)*nlon+4*nt*l1+1)
c
c            if isym = 1 or 2 then lwork must be at least
c
c                       (2*nt+1)*l2*nlon+nlat*(4*nt*l1+1)
c
c     **************************************************************
c
c     output parameters
c
c
C*PL*ERROR* Comment line too long
c     v,w   two or three dimensional arrays (see input parameter nt) that
c           contain a vector field whose divergence is divg - pertbd and
C*PL*ERROR* Comment line too long
c           whose vorticity is vort - pertbv.  w(i,j) is the east longitude
C*PL*ERROR* Comment line too long
c           component and v(i,j) is the colatitudinal component of velocity
c           at the colatitude theta(i) = (i-1)*pi/(nlat-1) and longitude
C*PL*ERROR* Comment line too long
c           lambda(j) = (j-1)*2*pi/nlon for i=1,...,nlat and j=1,...,nlon.
c
C*PL*ERROR* Comment line too long
c   pertbd  a nt dimensional array (see input parameter nt and assume nt=1
C*PL*ERROR* Comment line too long
c           for the description that follows).  divg - pertbd is a scalar
c           field which can be the divergence of a vector field (v,w).
c           pertbd is related to the scalar harmonic coefficients ad,bd
c           of divg (computed by shags) by the formula
c
c                pertbd = ad(1,1)/(2.*sqrt(2.))
c
c           an unperturbed divg can be the divergence of a vector field
c           only if ad(1,1) is zero.  if ad(1,1) is nonzero (flagged by
c           pertbd nonzero) then subtracting pertbd from divg yields a
c           scalar field for which ad(1,1) is zero.  usually pertbd is
c           zero or small relative to divg.
c
C*PL*ERROR* Comment line too long
c   pertbv a nt dimensional array (see input parameter nt and assume nt=1
C*PL*ERROR* Comment line too long
c           for the description that follows).  vort - pertbv is a scalar
c           field which can be the vorticity of a vector field (v,w).
c           pertbv is related to the scalar harmonic coefficients av,bv
c           of vort (computed by shags) by the formula
c
c                pertbv = av(1,1)/(2.*sqrt(2.))
c
c           an unperturbed vort can be the vorticity of a vector field
c           only if av(1,1) is zero.  if av(1,1) is nonzero (flagged by
c           pertbv nonzero) then subtracting pertbv from vort yields a
c           scalar field for which av(1,1) is zero.  usually pertbv is
c           zero or small relative to vort.
c
c    ierror = 0  no errors
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
      SUBROUTINE DIDVTGS(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,AD,BD,AV,BV,
     +                  MDAB,NDAB,WVHSGS,LVHSGS,WORK,LWORK,PERTBD,
     +                  PERTBV,IERROR)
      DOUBLE PRECISION V
      DOUBLE PRECISION W
      DOUBLE PRECISION AD
      DOUBLE PRECISION BD
      DOUBLE PRECISION AV
      DOUBLE PRECISION BV
      DOUBLE PRECISION WVHSGS
      DOUBLE PRECISION WORK
      DOUBLE PRECISION PERTBD
      DOUBLE PRECISION PERTBV
      DIMENSION W(IDVW,JDVW,NT),V(IDVW,JDVW,NT),PERTBD(NT),PERTBV(NT)
      DIMENSION AD(MDAB,NDAB,NT),BD(MDAB,NDAB,NT)
      DIMENSION AV(MDAB,NDAB,NT),BV(MDAB,NDAB,NT)
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
      IDZ = (MMAX* (NLAT+NLAT-MMAX+1))/2
      LZIMN = IDZ*IMID
      IF (LVHSGS.LT.LZIMN+LZIMN+NLON+15) RETURN
      IERROR = 10
c
c     verify unsaved work space length
c
      MN = MMAX*NLAT*NT
      IF (ISYM.NE.0 .AND. LWORK.LT. (2*NT+1)*IMID*NLON+4*MN+NLAT) RETURN
      IF (ISYM.EQ.0 .AND. LWORK.LT. (2*NT+1)*NLAT*NLON+4*MN+NLAT) RETURN
      IERROR = 0
c
c     set work space pointers
c
      IBR = 1
      IBI = IBR + MN
      ICR = IBI + MN
      ICI = ICR + MN
      IS = ICI + MN
      IWK = IS + NLAT
      LIWK = LWORK - 4*MN - NLAT
      CALL DIDVTGS1(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,WORK(IBR),WORK(IBI),
     +             WORK(ICR),WORK(ICI),MMAX,WORK(IS),MDAB,NDAB,AD,BD,AV,
     +             BV,WVHSGS,LVHSGS,WORK(IWK),LIWK,PERTBD,PERTBV,IERROR)
      RETURN
      END

      SUBROUTINE DIDVTGS1(NLAT,NLON,ISYM,NT,V,W,IDVW,JDVW,BR,BI,CR,CI,
     +                   MMAX,SQNN,MDAB,NDAB,AD,BD,AV,BV,WVHSGS,LVHSGS,
     +                   WK,LWK,PERTBD,PERTBV,IERROR)
      DOUBLE PRECISION V
      DOUBLE PRECISION W
      DOUBLE PRECISION BR
      DOUBLE PRECISION BI
      DOUBLE PRECISION CR
      DOUBLE PRECISION CI
      DOUBLE PRECISION SQNN
      DOUBLE PRECISION AD
      DOUBLE PRECISION BD
      DOUBLE PRECISION AV
      DOUBLE PRECISION BV
      DOUBLE PRECISION WVHSGS
      DOUBLE PRECISION WK
      DOUBLE PRECISION PERTBD
      DOUBLE PRECISION PERTBV
      DOUBLE PRECISION FN
      DIMENSION W(IDVW,JDVW,NT),V(IDVW,JDVW,NT)
      DIMENSION BR(MMAX,NLAT,NT),BI(MMAX,NLAT,NT),SQNN(NLAT)
      DIMENSION CR(MMAX,NLAT,NT),CI(MMAX,NLAT,NT)
      DIMENSION AD(MDAB,NDAB,NT),BD(MDAB,NDAB,NT)
      DIMENSION AV(MDAB,NDAB,NT),BV(MDAB,NDAB,NT)
      DIMENSION WVHSGS(LVHSGS),WK(LWK)
      DIMENSION PERTBD(NT),PERTBV(NT)
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
c     set divergence,vorticity perturbation constants
c
          PERTBD(K) = AD(1,1,K)/ (2.D0*SQRT(2.D0))
          PERTBV(K) = AV(1,1,K)/ (2.D0*SQRT(2.D0))
c
c     preset br,bi,cr,ci to 0.0
c
          DO 3 N = 1,NLAT
              DO 4 M = 1,MMAX
                  BR(M,N,K) = 0.0D0
                  BI(M,N,K) = 0.0D0
                  CR(M,N,K) = 0.0D0
                  CI(M,N,K) = 0.0D0
    4         CONTINUE
    3     CONTINUE
c
c     compute m=0 coefficients
c
          DO 5 N = 2,NLAT
              BR(1,N,K) = -AD(1,N,K)/SQNN(N)
              BI(1,N,K) = -BD(1,N,K)/SQNN(N)
              CR(1,N,K) = AV(1,N,K)/SQNN(N)
              CI(1,N,K) = BV(1,N,K)/SQNN(N)
    5     CONTINUE
c
c     compute m>0 coefficients
c
          DO 6 M = 2,MMAX
              DO 7 N = M,NLAT
                  BR(M,N,K) = -AD(M,N,K)/SQNN(N)
                  BI(M,N,K) = -BD(M,N,K)/SQNN(N)
                  CR(M,N,K) = AV(M,N,K)/SQNN(N)
                  CI(M,N,K) = BV(M,N,K)/SQNN(N)
    7         CONTINUE
    6     CONTINUE
    2 CONTINUE
c
c     set ityp for vector synthesis without assuming div=0 or curl=0
c
      IF (ISYM.EQ.0) THEN
          ITYP = 0
      ELSE IF (ISYM.EQ.1) THEN
          ITYP = 3
      ELSE IF (ISYM.EQ.2) THEN
          ITYP = 6
      END IF
c
c     sythesize br,bi,cr,ci into the vector field (v,w)
c
      CALL DVHSGS(NLAT,NLON,ITYP,NT,V,W,IDVW,JDVW,BR,BI,CR,CI,MMAX,NLAT,
     +           WVHSGS,LVHSGS,WK,LWK,IERROR)
      RETURN
      END
