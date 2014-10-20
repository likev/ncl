C
C	$Id: fdvdld.f,v 1.6 2008-07-27 00:23:03 haley Exp $
C                                                                      
C                Copyright (C)  2000
C        University Corporation for Atmospheric Research
C                All Rights Reserved
C
C The use of this Software is governed by a License Agreement.
C
      SUBROUTINE FDVDLD (IENTRY,IIX,IIY)
C
C Software dashed line package with character capability and smoothing
C
C PURPOSE                DASHSMTH is a software dashed line package with
C                        smoothing capabilities.  DASHSMTH is DASHCHAR
C                        with smoothing features added.
C
C USAGE                  First, either
C                             CALL DASHDB (IPAT)
C                        where IPAT is a 16-bit dash pattern as
C                        described in the subroutine DASHDB (see
C                        DASHLINE documentation), or
C                             CALL DASHDC (IPAT,JCRT,JSIZE)
C                        as described below.
C
C                        Then, call any of the following:
C                             CALL CURVED (X,Y,N)
C                             CALL FRSTD (X,Y)
C                             CALL VECTD (X,Y)
C                             CALL LASTD
C
C                        LASTD is called only after the last
C                        point of a line has been processed in VECTD.
C
C                        The following may also be called, but no
C                        smoothing will result:
C                             CALL LINED (XA,YA,XB,YB)
C
C
C ARGUMENTS              IPAT
C ON INPUT                 A character string of arbitrary length
C TO DASHDC                (60 characters seems to be a practical
C                          limit) which specifies the dash pattern
C                          to be used.  A dollar sign in IPAT
C                          indicates solid; an apostrophe indicates
C                          a gap; blanks are ignored.  Any character
C                          in IPAT which is not a dollar sign,
C                          apostrophe, or blank is considered to be
C                          part of a line label.  Each line label
C                          can be at most 15 characters in length.
C                          Sufficient white space is reserved in the
C                          dashed line for writing line labels.
C
C                        JCRT
C                          The length in plotter address units per
C                          $ or apostrophe.
C
C                        JSIZE
C                          Is the size of the plotted characters:
C                          . If between 0 and 3 , it is 1., 1.5, 2.
C                            and 3. times an 8 plotter address unit
C                            width.
C                          . If greater than 3, it is the character
C                            width in plotter address units.
C
C
C ARGUMENTS TO           CURVED(X,Y,N)
C OTHER LINE-DRAWING       X and Y are arrays of world coordinate values
C ROUTINES                 of length N or greater.  Line segments obeying
C                          the specified dash pattern are drawn to
C                          connect the N points.
C
C                        FRSTD(X,Y)
C                          The current pen position is set to
C                          the world coordinate value (X,Y)
C
C                        VECTD(X,Y)
C                          A line segment is drawn between the
C                          world coordinate value (X,Y) and the
C                          most recent pen position.  (X,Y) then
C                          becomes the most recent pen position.
C
C                        LINED(XA,XB,YA,YB)
C                          A line is drawn between world coordinate
C                          values (XA,YA) and (XB,YB).
C
C                        LASTD
C                          When using FRSTD and VECTD, LASTD must be
C                          called (no arguments needed).  LASTD sets up
C                          the calls to the smoothing routines MSKRV1
C                          and MSKRV2.
C
C ON OUTPUT                All arguments are unchanged for all routines.
C
C
C NOTE                     When switching from the regular plotting
C                          routines to a dashed line package the first
C                          call should not be to VECTD.
C
C ENTRY POINTS             DASHDB, DASHDC, CURVED, FRSTD, VECTD, LINED,
C                          RESET, LASTD, CFVLD, FDVDLD, DRAWPV, DASHBD,
C                          DASHBDX
C
C COMMON BLOCKS            INTPR, DASHD1, DASHD2, DDFLAG, DCFLAG, DSAVE1,
C                          DSAVE6, DSAVE3, DSAVE5, CFFLAG, SMFLAG, DFFLAG,
C                          FDFLAG
C
C REQUIRED LIBRARY         The ERPRT77 package and the SPPS.
C ROUTINES
C
C REQUIRED GKS LEVEL       0A
C
C I/O                      Plots solid or dashed lines, possibly with
C                          characters at intervals in the line.
C                          The lines may also be smoothed.
C
C PRECISION                Single
C
C LANGUAGE                 FORTRAN
C
C HISTORY                  Written in October 1973.
C                          Made portable in September 1977 for use
C                          with all machines which
C                          support plotters with up to 15 bit resolution.
C                          Converted to FORTRAN77 and GKS in June, 1984.
C
C ALGORITHM                Points for each line
C                          segment are processed and passed to the
C                          routines, MSKRV1 and MSKRV2, which compute
C                          splines under tension passing through these
C                          points.  New points are generated between the
C                          given points, resulting in smooth lines.
C
C ACCURACY                 Plus or minus .5 plotter address units per call.
C                          There is no cumulative error.
C
C TIMING                   About three times as long as DASHCHAR.
C
C
C
C
C
C
C
C
C***********************************************************************
C
C FDVDLD RECEIVES IN ITS ARGUMENTS THE POINTS TO BE PROCESSED FOR A
C LINE SEGMENT. IT PASSES THESE POINTS TO THE ROUTINES MSKRV1 AND MSKRV2
C WHICH COMPUTE SPLINES UNDER TENSION PASSING THROUGH THESE POINTS.
C FDVDLD THEN CALLS CFVLD TO CONNECT THE POINTS GENERATED IN MSKRV2.
C
      DIMENSION       XP(70),  YP(70),  TEMP(70),  SLEN(70)
C
C THE VARIABLES IN DSAVE5 HAVE TO BE SAVED FOR THE NEXT CALL TO FDVDLD.
C
      COMMON /DSAVE5/ XSAVE(70), YSAVE(70), XSVN, YSVN, XSV1, YSV1,
     1                SLP1, SLPN, SSLP1, SSLPN, N, NSEG
C
C IOFFS IS AN INTERNAL PARAMETER. IT IS INITIALIZED IN DASHBDX AND
C REFERENCED IN FDVDLD AND DRAWPV.
C
      COMMON /SMFLAG/ IOFFS
C
C IFSTF2 IS A FLAG TO CONTROL THAT FRSTD IS CALLED BEFORE VECTD IS
C CALLED.
C
      COMMON /DFFLAG/ IFSTF2
C
C IFLAG CONTROLS IF LASTD CAN BE CALLED DIRECTLY OR IF IT WAS JUST
C CALLED FROM BY VECTD SO THAT THIS CALL CAN BE IGNORED.
C
      COMMON /FDFLAG/ IFLAG
C
C NOTE THAT THIS IFSTF2 FLAG CANNOT BE IDENTICAL TO THE IFSTFL FLAG
C IN THE ROUTINE CFVLD, BECAUSE A CALL TO THE FRSTD ENTRY OF FDVDLD DOES
C NOT ELIMINATE THE NECESSITY OF A CALL TO THE FRSTD ENTRY OF CFVLD,
C AND REVERSE.
C
      COMMON/INTPR/IPAU,FPART,TENSN,NP,SMALL,L1,ADDLR,ADDTB,MLLINE,
     1    ICLOSE
      SAVE
C
C
C OTHER CONSTANTS.
C
      DATA PI /3.14159265358/
      DATA IDUMMY /0/
C
C
      GO TO (10,15,35),IENTRY
C
C *************************************
C
C ENTRY  FRSTD (XX,YY)
C
   10 DEG = 180./PI
C
      MX = IIX
      MY = IIY
      IFSTF2 = 0
      SSLP1 = 0.0
      SSLPN = 0.0
      XSVN = 0.0
      YSVN = 0.0
      IF (IOFFS .GE. 1) CALL CFVLD (1,MX,MY)
      IF (IOFFS .GE. 1) RETURN
C
C INITIALIZE THE POINT AND SEGMENT COUNTER
C N COUNTS THE NUMBER OF POINTS/SEGMENT
C
      N = 0
C
C NSEG = 0       FIRST SEGMENT
C NSEG = 1       MORE THAN ONE SEGMENT
C
      NSEG = 0
C
C SAVE THE X,Y COORDINATES OF THE FIRST POINT
C XSV1           CONTAINS THE X COORDINATE OF THE FIRST POINT
C                OF A LINE
C YSV1           CONTAINS THE Y COORDINATE OF THE FIRST POINT
C                OF A LINE
C
      XSV1 = MX
      YSV1 = MY
      GO TO 30
C
C *************************************
C
C     ENTRY VECTD (XX,YY)
C
   15 CONTINUE
C
C TEST FOR PREVIOUS FRSTD CALL
C
      IF (IFSTF2 .EQ. 0) GO TO 20
C
C INFORM USER - NO PREVIOUS CALL TO FRSTD. TREAT CALL AS FRSTD CALL.
C
      CALL SETER(' FDVDLD- VECTD CALL OCCURS BEFORE A CALL TO FRSTD.',
     -            1,1)
      GO TO 10
   20 MX = IIX
      MY = IIY
C
C VECTD          SAVES THE X,Y COORDINATES OF THE ACCEPTED
C                POINTS ON A LINE SEGMENT
C
      IF (IOFFS .GE. 1) CALL CFVLD (2,MX,MY)
      IF (IOFFS .GE. 1) RETURN
C
C IF THE NEW POINT IS TOO CLOSE TO THE PREVIOUS POINT, IGNORE IT
C
      IF (ABS(REAL(INT(XSVN)-MX))+ABS(REAL(INT(YSVN)-MY)) .LT.
     1    SMALL) RETURN
      IFLAG = 0
   30 N = N+1
C
C SAVE THE X,Y COORDINATES OF EACH POINT OF THE SEGMENT
C XSAVE          THE ARRAY OF X COORDINATES OF LINE SEGMENT
C YSAVE          THE ARRAY OF Y COORDINATES OF LINE SEGMENT
C
      XSAVE(N) = MX
      YSAVE(N) = MY
      XSVN = XSAVE(N)
      YSVN = YSAVE(N)
      IF (N .GE. L1-1) GO TO 40
      RETURN
C
C *************************************
C
C     ENTRY LASTD
C
   35 CONTINUE
      IF (IFSTF2 .NE. 0) RETURN
      IFSTF2 = 1
C
C LASTD          CHECKS FOR PERIODIC LINES AND SETS UP
C                  THE CALLS TO MSKRV1 AND MSKRV2
C
      IF (IOFFS .GE. 1) CALL CFVLD (3,IDUMMY,IDUMMY)
      IF (IOFFS .GE. 1) RETURN
C
C IFLAG = 0      OK TO CALL LASTD DIRECTLY
C IFLAG = 1      LASTD WAS JUST CALLED FROM BY VECTD
C                IGNORE CALL TO LASTD
C
      IF (IFLAG .EQ. 1) RETURN
C
C COMPARE THE LAST POINT OF SEGMENT WITH FIRST POINT OF LINE
C
   40 IFLAG = 1
C
C IPRD = 0       PERIODIC LINE
C IPRD = 1       NON-PERIODIC LINE
C
      IPRD = 1
      IF (ABS(XSV1-XSVN)+ABS(YSV1-YSVN) .LT. SMALL) IPRD = 0
C
C TAKE CARE OF THE CASE OF ONLY TWO DISTINCT P0INTS ON A LINE
C
      IF (NSEG .GE. 1) GO TO 60
      IF (N-2) 150,140,50
   50 IF (N .GE. 4) GO TO 60
C
      IF (IPRD .NE. 0) GO TO 60
      DX = XSAVE(2)-XSAVE(1)
      DY = YSAVE(2)-YSAVE(1)
      SLOPE = ATAN2(DY,DX)*DEG+90.
      IF (SLOPE .GE. 360.) SLOPE = SLOPE-360.
      IF (SLOPE .LE. 0.) SLOPE = SLOPE+360.
      SLP1 = SLOPE
      SLPN = SLOPE
      ISLPSW = 0
      SIGMA = TENSN
      GO TO 100
   60 SIGMA = TENSN
      IF (IPRD .GE. 1) GO TO 80
      IF (NSEG .GE. 1) GO TO 70
C
C SET UP FLAGS FOR A  1  SEGMENT, PERIODIC LINE
C
      ISLPSW = 4
      XSAVE(N) = XSV1
      YSAVE(N) = YSV1
      GO TO 100
C
C SET UP FLAGS FOR AN N-SEGMENT, PERIODIC LINE
C
   70 SLP1 = SSLPN
      SLPN = SSLP1
      ISLPSW = 0
      GO TO 100
   80 IF (NSEG .GE. 1) GO TO 90
C
C SET UP FLAGS FOR THE 1ST SEGMENT OF A NON-PERIODIC LINE
C
      ISLPSW = 3
      GO TO 100
C
C SET UP FLAGS FOR THE NTH SEGMENT OF A NON-PERIODIC LINE
C
   90 SLP1 = SSLPN
      ISLPSW = 1
C
C CALL THE SMOOTHING ROUTINES
C
  100 CALL MSKRV1 (N,XSAVE,YSAVE,SLP1,SLPN,XP,YP,TEMP,SLEN,SIGMA,ISLPSW)
C
C DETERMINE THE NUMBER OF POINTS TO INTERPOLATE FOR EACH SEGMENT
C
      IF (NSEG.GE.1 .AND. N.LT.L1-1) GO TO 110
      NPRIME = REAL(NP)-(SLEN(N)*REAL(NP)*.5)/32767.
      IF (SLEN(N) .GE. 32767.) NPRIME = .5*REAL(NP)
      NPL = MAX(REAL(NPRIME)*SLEN(N)/32767.,2.5)
  110 DT = 1./REAL(NPL)
      IX = INT (XSAVE(1))
      IY = INT (YSAVE(1))
      IF (NSEG .LE. 0) GO TO 112
      CALL DRAWPV (IX,IY,0)
      GO TO 114
  112 CONTINUE
      CALL CFVLD (1,IX,IY)
  114 CONTINUE
      T = 0.0
      NSLPSW = 1
      IF (NSEG .GE. 1) NSLPSW = 0
      NSEG = 1
      CALL MSKRV2 (T,XS,YS,N,XSAVE,YSAVE,XP,YP,SLEN,SIGMA,NSLPSW,SLP)
C
C SAVE SLOPE AT THE FIRST POINT OF THE LINE
C
      IF (NSLPSW .GE. 1) SSLP1 = SLP
      NSLPSW = 0
      DO 120 I=1,NPL
         T = T+DT
         IF (I .EQ. NPL) NSLPSW = 1
         CALL MSKRV2 (T,XS,YS,N,XSAVE,YSAVE,XP,YP,SLEN,SIGMA,NSLPSW,SLP)
C
C SAVE THE LAST SLOPE OF THIS LINE SEGMENT
C
         IF (NSLPSW .GE. 1) SSLPN = SLP
C
C DRAW EACH PART OF THE LINE SEGMENT
C
         IX = INT(XS)
         IY = INT (YS)
         CALL CFVLD (2,IX,IY)
  120 CONTINUE
      IF (IPRD .NE. 0) GO TO 130
C
C CONNECT THE LAST POINT WITH THE FIRST POINT OF A PERIODIC LINE
C
      IX = INT (XSV1)
      IY = INT (YSV1)
      CALL CFVLD (2,IX,IY)
C
C BEGIN THE NEXT LINE SEGMENT WITH THE LAST POINT OF THIS SEGMENT
C
  130 XSAVE(1) = XS
      YSAVE(1) = YS
      N = 1
      IF (IFSTF2 .EQ. 1) CALL CFVLD (3,IDUMMY,IDUMMY)
      GO TO 150
C
C FOR THE CASE WHEN THERE ARE ONLY 2 DISTINCT POINTS ON A LINE.
C
  140 IX = INT (XSAVE(1))
      IY = INT (YSAVE(1))
      CALL CFVLD (1,IX,IY)
      IX = INT (XSAVE(N))
      IY = INT (YSAVE(N))
      CALL CFVLD (2,IX,IY)
      IF (IFSTF2 .EQ. 1) CALL CFVLD (3,IDUMMY,IDUMMY)
C
  150 CONTINUE
      RETURN
      END
