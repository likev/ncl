      subroutine strhlpdp(x)
c
      double precision x
c
c                                     coded by russell lynch
c                              from fitpack -- april 8, 1991
c                        a curve and surface fitting package
c                      a product of pleasant valley software
c                  8603 altus cove, austin, texas 78759, usa
c
c this subroutine is used by the function ftstoredp to force the
c storage of a value into a single precision storage cell
c consistant with fortran 66.
c
c on input--
c
c   x is a double precision variable.
c
c on output--
c
c   v in the common block dvcom contains a double precision
c   version of x.
c
c and x is unaltered.
c
c this subroutine requires a common block named dvcom.
c
c-------------------------------------------------------------
c
      common/dvcom/vc
      double precision vc
c
      vc = x
      return
      end
