C
C      $Id: cn06f.f,v 1.11 2010-03-15 22:49:23 haley Exp $
C
C***********************************************************************
C                                                                      *
C                Copyright (C)  1996                                   *
C        University Corporation for Atmospheric Research               *
C     The use of this Software is governed by a License Agreement      *
C                                                                      *
C***********************************************************************
C
C  File:       cn06f.f
C
C  Author:     Ethan Alpert (converted to Fortran by Mary Haley)
C          National Center for Atmospheric Research
C          PO 3000, Boulder, Colorado
C
C  Date:       Thu Jan  4 14:22:28 MDT 1996
C
C  Description:    Reads a netCDF file and produces a series of
C                  temperature contour plots.
C
      external NhlFAppClass
      external NhlFNcgmWorkstationClass
      external NhlFPSWorkstationClass
      external NhlFPDFWorkstationClass
      external NhlFCairoPSPDFWorkstationClass
      external NhlFCairoImageWorkstationClass
      external NhlFCairoWindowWorkstationClass
      external NhlFScalarFieldClass
      external NhlFContourPlotClass

      parameter(NLON=36,NLAT=33,NCOLORS=17)
      integer xbvalues(6),ylvalues(5)
      character*4 xblabels(6)
      character*3 yllabels(5)
      data xbvalues/-60,-75,-90,-105,-120,-135/
      data ylvalues/60,50,40,30,20/
      data xblabels/'60W','75W','90W','105W','120W','135W'/
      data yllabels/'60N','50N','40N','30N','20N'/
C
C Declare variables for the HLU routine calls.
C
      integer appid, workid, field1, con1
      integer srlist, i
      real   cmap(3,NCOLORS)
C
C Declare variables for getting information from netCDF file.
C
      integer  ncid, lon_id, lat_id, frtime_id, T_id, frtime(7)
      real T(NLON,NLAT), spv
      real lon(NLON), lat(NLAT)
      integer start(4), count(4), lonlen, latlen, frtimelen
      integer t_type, tlen, hlen
      character*260 filename, full_title
      character*50 recname
      character*128 lat_name, lon_name, title, hist
      character*7  wks_type
C
C Default is to create an NCGM file.
C
      wks_type = "ncgm"
C
C Initialize the HLU library and set up resource template.
C
      call NhlFInitialize
      call NhlFRLCreate(srlist,'setrl')
      call NhlFRLClear(srlist)
      call NhlFRLSetString(srlist,'appDefaultParent','True',ierr)
      call NhlFRLSetString(srlist,'appUsrDir','./',ierr)
      call NhlFCreate(appid,'cn06',NhlFAppClass,0,srlist,ierr)

      cmap(1,1) = 0.0
      cmap(2,1) = 0.0
      cmap(3,1) = 0.0
      cmap(1,2) = 1.0
      cmap(2,2) = 1.0
      cmap(3,2) = 1.0
      cmap(1,3) = 0.0
      cmap(2,3) = 0.0
      cmap(3,3) = 0.0
      cmap(1,4) = 1.0
      cmap(2,4) = 0.0
      cmap(3,4) = 0.0
      cmap(1,5) = 0.0
      cmap(2,5) = 1.0
      cmap(3,5) = 0.0
      cmap(1,6) = 0.0
      cmap(2,6) = 0.0
      cmap(3,6) = 1.0
      cmap(1,7) = 1.0
      cmap(2,7) = 1.0
      cmap(3,7) = 0.0
      cmap(1,8) = 0.0
      cmap(2,8) = 1.0
      cmap(3,8) = 1.0
      cmap(1,9) = 1.0
      cmap(2,9) = 0.0
      cmap(3,9) = 1.0
      cmap(1,10) = 0.5
      cmap(2,10) = 0.0
      cmap(3,10) = 0.0
      cmap(1,11) = 0.5
      cmap(2,11) = 1.0
      cmap(3,11) = 1.0
      cmap(1,12) = 0.0
      cmap(2,12) = 0.0
      cmap(3,12) = 0.5
      cmap(1,13) = 1.0
      cmap(2,13) = 1.0
      cmap(3,13) = 0.5
      cmap(1,14) = 0.5
      cmap(2,14) = 0.0
      cmap(3,14) = 1.0
      cmap(1,15) = 1.0
      cmap(2,15) = 0.5
      cmap(3,15) = 0.0
      cmap(1,16) = 0.0
      cmap(2,16) = 0.5
      cmap(3,16) = 1.0
      cmap(1,17) = 0.5
      cmap(2,17) = 1.0
      cmap(3,17) = 0.0

      count(1) = 3
      count(2) = NCOLORS
      if (wks_type.eq."ncgm".or.wks_type.eq."NCGM") then
C
C Create an NCGM workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkMetaName','./cn06f.ncgm',ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFNcgmWorkstationClass,0,srlist,ierr)
      else if (wks_type.eq."x11".or.wks_type.eq."X11") then
C
C Create an X11 workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkPause','True',ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFCairoWindowWorkstationClass,
     +        0,srlist,ierr)
      else if (wks_type.eq."oldps".or.wks_type.eq."OLDPS") then
C
C Create an older-style PostScript workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkPSFileName','./cn06f.ps',ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFPSWorkstationClass,0,srlist,ierr)
      else if (wks_type.eq."oldpdf".or.wks_type.eq."OLDPDF") then
C
C Create an older-style PDF workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkPDFFileName','./cn06f.pdf',ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFPDFWorkstationClass,0,srlist,ierr)
      else if (wks_type.eq."pdf".or.wks_type.eq."PDF".or.
     +         wks_type.eq."ps".or.wks_type.eq."PS") then
C
C Create a cairo PS/PDF workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkFileName','./cn06f',ierr)
         call NhlFRLSetString(srlist,'wkFormat',wks_type,ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFCairoPSPDFWorkstationClass,0,srlist,ierr)
      else if (wks_type.eq."png".or.wks_type.eq."PNG") then
C
C Create a cairo PNG workstation.
C
         call NhlFRLClear(srlist)
         call NhlFRLSetMDFloatArray(srlist,'wkColorMap',cmap,2,count,
     +        ierr)
         call NhlFRLSetString(srlist,'wkFileName','./cn06f',ierr)
         call NhlFRLSetString(srlist,'wkFormat',wks_type,ierr)
         call NhlFCreate(workid,'cn06Work',
     +        NhlFCairoImageWorkstationClass,0,srlist,ierr)
      endif
C
C Open NetCDF file.
C
      call gngpat(filename,'data',ierr)
      do 10 i=1,256
         if( filename(i:i).eq.char(0) ) then
            filename(i:i+15)='/cdf/contour.cdf'
            goto 15
         endif
 10   continue
C      
C The second argument to 'ncopn' should be NCNOWRIT, but since we
C can't include 'netcdf.inc', we are using the value '0' instead.
C
 15   ncid = ncopn(filename,0,ierr)
C
C Get title and history.
C
      call ncainq(ncid,0,'title',t_type,tlen,ierr)
      call ncagtc(ncid,0,'title',title,128,ierr)
      call ncainq(ncid,0,'history',t_type,hlen,ierr)
      call ncagtc(ncid,0,'history',hist,128,ierr)
      full_title = title(1:tlen-1)//'#C#'//hist(1:hlen-1)
C
C Get the lat/lon dimensions.
C
      lat_id = ncdid(ncid,'lat',ierr)
      lon_id = ncdid(ncid,'lon',ierr)
      frtime_id = ncdid(ncid,'frtime',ierr)
      call ncdinq(ncid,lat_id,recname,latlen,ierr)
      call ncdinq(ncid,lon_id,recname,lonlen,ierr)
      call ncdinq(ncid,frtime_id,recname,frtimelen,ierr)
      frtime_id = ncvid(ncid,'frtime',ierr)
C
C Read in T.
C
      T_id = ncvid(ncid,'T',ierr)
      start(1) = 1
      start(2) = 1
      start(3) = 1
      start(4) = 1
      count(1) = lonlen
      count(2) = latlen
      count(3) = 1
      count(4) = 1
      call ncvgt(ncid,T_id,start,count,T,ierr)
      call ncagt(ncid,T_id,'_FillValue',spv,ierr)
C
C Read in lat/lon/frtime values.
C
      lat_id = ncvid(ncid,'lat',ierr)
      count(1) = latlen
      call ncvgt(ncid,lat_id,start,count,lat,ierr)
      call ncagtc(ncid,lat_id,'long_name',lat_name,128,ierr)

      lon_id = ncvid(ncid,'lon',ierr)
      count(1) = lonlen
      call ncvgt(ncid,lon_id,start,count,lon,ierr)
      call ncagtc(ncid,lon_id,'long_name',lon_name,128,ierr)

      frtime_id = ncvid(ncid,'frtime',ierr)
      count(1) = frtimelen
      call ncvgt(ncid,frtime_id,start,count,frtime,ierr)
C
C Convert T from Degrees K to Degrees F.
C
      call KtoF(T)
C
C Create a scalar field object and configure the missing values and
C the start and end information.
C
      count(1) = lonlen
      count(2) = latlen
      call NhlFRLClear(srlist)
      call NhlFRLSetMDFloatArray(srlist,'sfDataArray',T,2,count,
     +     ierr)
      call NhlFRLSetFloat(srlist,'sfMissingValueV',spv,ierr)
      call NhlFRLSetFloat(srlist,'sfXCStartV',lon(1),ierr)
      call NhlFRLSetFloat(srlist,'sfXCEndV',lon(lonlen),ierr)
      call NhlFRLSetFloat(srlist,'sfYCStartV',lat(1),ierr)
      call NhlFRLSetFloat(srlist,'sfYCEndV',lat(latlen),ierr)
      call NhlFCreate(field1,'field1',NhlFscalarFieldClass,appid,srlist,
     +     ierr)
C
C Create contour object.
C
      call NhlFRLClear(srlist)
      call NhlFRLSetFloat(srlist,'vpXF',.2,ierr)
      call NhlFRLSetFloat(srlist,'vpYF',.8,ierr)
      call NhlFRLSetFloat(srlist,'vpWidthF',.6,ierr)
      call NhlFRLSetFloat(srlist,'vpHeightF',.6,ierr)
      call NhlFRLSetInteger(srlist,'cnScalarFieldData',field1,ierr)
      call NhlFRLSetString(srlist,'cnLevelSelectionMode',
     +     'ManualLevels',ierr)
      call NhlFRLSetFloat(srlist,'cnMinLevelValF',-40.0,ierr)
      call NhlFRLSetFloat(srlist,'cnMaxLevelValF',110.0,ierr)
      call NhlFRLSetFloat(srlist,'cnLevelSpacingF',10.0,ierr)
      call NhlFRLSetString(srlist,'cnFillOn','True',ierr)
      call NhlFRLSetString(srlist,'cnMonoFillPattern','True',ierr)
      call NhlFRLSetInteger(srlist,'cnFillPatterns',0,ierr)
      call NhlFRLSetFloat(srlist,'trXMinF',-140.0,ierr)
      call NhlFRLSetFloat(srlist,'trXMaxF',-52.5,ierr)
      call NhlFRLSetFloat(srlist,'trYMinF',20.0,ierr)
      call NhlFRLSetFloat(srlist,'trYMaxF',60.0,ierr)
      call NhlFRLSetString(srlist,'tiMainFuncCode','#',ierr)
      call NhlFRLSetString(srlist,'tiMainString',full_title,ierr)
      call NhlFRLSetFloat(srlist,'tiMainFontHeightF',0.02,ierr)
      call NhlFRLSetString(srlist,'tiXAxisString',lon_name,ierr)
      call NhlFRLSetString(srlist,'tiYAxisString',lat_name,ierr)
      call NhlFRLSetString(srlist,'tmXBMode','EXPLICIT',ierr)
      call NhlFRLSetIntegerArray(srlist,'tmXBValues',xbvalues,6,ierr)
      call NhlFRLSetStringArray(srlist,'tmXBLabels',xblabels,6,ierr)
      call NhlFRLSetString(srlist,'tmYLMode','EXPLICIT',ierr)
      call NhlFRLSetIntegerArray(srlist,'tmYLValues',ylvalues,5,ierr)
      call NhlFRLSetStringArray(srlist,'tmYLLabels',yllabels,5,ierr)
      call NhlFRLSetString(srlist,'cnLowLabelsOn','True',ierr)
      call NhlFRLSetString(srlist,'cnHighLabelsOn','True',ierr)
      call NhlFRLSetString(srlist,'tmXMajorGrid','True',ierr)
      call NhlFRLSetString(srlist,'tmYMajorGrid','True',ierr)
      call NhlFRLSetString(srlist,'tmXBMinorOn','False',ierr)
      call NhlFRLSetString(srlist,'tmYLMinorOn','False',ierr)
      call NhlFCreate(con1,'con1',NhlFcontourPlotClass,workid,srlist,
     +     ierr)
C
C Draw first frame
C
      call NhlFDraw(con1,ierr)
      call NhlFFrame(workid,ierr)
C
C Loop on remaining time steps.
C
      do 60 i = 2,frtimelen
C
C Read in new section of T.
C
         start(1) = 1
         start(2) = 1
         start(3) = 1
         start(4) = i
         count(1) = lonlen
         count(2) = latlen
         count(3) = 1
         count(4) = 1
         call ncvgt(ncid,T_id,start,count,T,ierr)
C
C Convert T from Degrees K to Degrees F.
C
         call KtoF(T)
C
C Create new scalar field.
C
         call NhlFRLClear(srlist)
         count(1) = lonlen
         count(2) = latlen
         call NhlFRLSetMDFloatArray(srlist,'sfDataArray',T,2,count,
     +        ierr)
         call NhlFSetValues(field1,srlist,ierr)
         call NhlFDraw(con1,ierr)
         call NhlFFrame(workid,ierr)
 60   continue
C
C Close the netCDF file.
C
      call ncclos(ncid,ierr)
C
C NhlFDestroy destroys the given id and all of its children.
C
      call NhlFRLDestroy(srlist)
      call NhlFDestroy(appid,ierr)
C
C Restores state.
C
      call NhlFClose
      stop
      end

      subroutine KtoF(T)
      parameter(NLON=36,NLAT=33)
      real T(NLON,NLAT)
C
C Convert T from Degrees K to Degrees F.
C
      do 100 i = 1,NLON
        do 90 j = 1,NLAT
           T(i,j) = (T(i,j)-273.15) * 9./5. + 32.
 90     continue
 100    continue
        return
        end


