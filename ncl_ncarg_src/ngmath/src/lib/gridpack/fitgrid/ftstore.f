      function ftstore(x)
c
      real x
c
c                                     coded by russell lynch
c                              from fitpack -- april 8, 1991
c                        a curve and surface fitting package
c                      a product of pleasant valley software
c                  8603 altus cove, austin, texas 78759, usa
c
c this function forces the placement of the value of its input
c to be placed in a single precision storage cell consistant
c with fortran 66. it is useful for comparing values nearly
c equal with respect to the precision of the computer.
c
c on input--
c
c   x is a real variable.
c
c on output--
c
c   store is a single precision version of x.
c
c and x is unaltered.
c
c this function references module strhlp and requires a
c common block named vcom.
c
c-------------------------------------------------------------
c
c
      common/vcom/vc
      call strhlp(x)
      ftstore = vc
      return
      end
