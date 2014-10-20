/************************************************************************
*ID: $Id: NclNewHDF5.c 15249 2014-04-21 16:03:44Z huangwei $
*                                                                       *
*                 Copyright (C)  2012                                   *
*         University Corporation for Atmospheric Research               *
*                 All Rights Reserved                                   *
*                                                                       *
*************************************************************************/

/*
 * $URL
 * $Rev
 * $Id
 * $Author
 * $Date
 */

/*
 * Description:    
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdlib.h>
#include <assert.h>

#include <math.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <hdf5.h>

#ifdef NIO_LIB_ONLY
#include "niohlu.h"
#include "nioNresDB.h"
#else
#include <ncarg/hlu/hlu.h>
#include <ncarg/hlu/NresDB.h>
#endif

#ifndef MAX_NCL_GROUP_LEVEL
#define MAX_NCL_GROUP_LEVEL    32
#endif

#ifndef MAX_NCL_NAME_LENGTH
#define MAX_NCL_NAME_LENGTH    1024
#endif

#ifndef MAX_NCL_BUFFER_LENGTH
#define MAX_NCL_BUFFER_LENGTH    1048576
#endif

#include "AdvancedFileSupport.h"
#include "NclData.h"
#include "ListSupport.h"

static NrmQuark Qmissing_val;
static NrmQuark Qfill_val;

#define H5_USE_CACHE_OPT         0
#define H5_COMPRESSION_LEVEL_OPT 1
#define H5_CACHE_SIZE_OPT        2
#define H5_CACHE_NELEMS_OPT      3
#define H5_CACHE_PREEMPTION_OPT  4
#define H5_NUM_OPTIONS           5

#ifndef FALSE
#define FALSE           0
#endif

#ifndef TRUE
#define TRUE            1
#endif

#ifndef SUCCEED
#define SUCCEED         0
#endif

#ifndef FAILED
#define FAILED          (-1)
#endif

void *_Ncl2HDF5type(NclBasicDataTypes type);

herr_t _searchH5obj(char *name, H5O_info_t *oinfo, void *_H5obj, char *already_seen);
herr_t _searchH5link(char *name, H5O_info_t *oinfo, void *_H5link);

/* Typedefs for serach functions */
typedef herr_t (*_searchH5obj_func_t) (char *path_name, H5O_info_t *oinfo, void *udata, char *already_seen);
typedef herr_t (*_searchH5link_func_t) (char *path_name, H5L_info_t *linfo, void *udata);

typedef struct
{
    _searchH5obj_func_t  _searchH5obj;		/* Callback for objects */
    _searchH5link_func_t _searchH5link;		/* Callback for links */
    void *udata;				/* User data pass to callbacks */
} H5searcher_t;

typedef struct
{
    haddr_t  addr;
    NrmQuark path;
} H5obj_t;

typedef struct
{
    size_t   nalloc;
    size_t   nused;

    H5obj_t* objs;
} H5_addr_t;

typedef struct
{
    H5_addr_t *seen;		/* List of addresses seen already */
    H5searcher_t *searcher;	/* Information for visiting each link/object */
    hbool_t is_absolute;	/* Whether the traversal has absolute paths */
    const char *base_grp_name;	/* Name of the group that serves as the base */
} H5_ud_traverse_t;

typedef struct
{
    int size;
    char short_name[MAX_NCL_GROUP_LEVEL][MAX_NCL_NAME_LENGTH];
    char parent_name[MAX_NCL_GROUP_LEVEL][MAX_NCL_NAME_LENGTH];
} h5_group_name_struct_t;

void *H5OpenFile(void *rootgrp, NclQuark path, int status);
static NclBasicDataTypes string2NclType(char* name);
static int _buildH5dimlist(NclFileGrpNode **rootgrp);
static NhlErrorTypes _addH5dim(NclFileDimRecord **grpdimrec, NclQuark dimname,
                               ng_size_t dimsize, int is_unlimited);
static NclFileEnumRecord *readH5EnumAtt(hid_t type);

NclFileGrpNode *_getGrpNodeByName(NclFileGrpNode *rootgrp, NclQuark gn);

herr_t _writeH5variableAttribute(hid_t did, NclFileAttNode *attnode);

hid_t toH5type(const char *type);
hid_t Ncltype2HDF5type(NclBasicDataTypes type);
hid_t h5memtype2filetype(hid_t memtype);

extern int _MachineIsBigEndian();

static H5_ud_traverse_t tudata;

void my_hdf5_error_handler(herr_t ecode, const char* flnm, int ln)
{
   fprintf(stderr, "\nAn HDF5 error was detected.\n");
   fprintf(stderr, "\tError code: %ld, at line: %d, in file: <%s>\n",
                      (long) ecode, ln, flnm);
}

hid_t h5memtype2filetype(hid_t memtype)
{
    hid_t h5type = -1;

    int is64bitMachine = (sizeof(int) == sizeof(long)) ? 0 : 1;

    if(_MachineIsBigEndian())
    {
       if((H5T_NATIVE_CHAR == memtype) || (H5T_NATIVE_SCHAR == memtype))
            h5type = H5T_STD_I8BE;
       else if(H5T_NATIVE_UCHAR == memtype)
            h5type = H5T_STD_U8BE;
       else if(H5T_NATIVE_SHORT == memtype)
            h5type = H5T_STD_I16BE;
       else if(H5T_NATIVE_USHORT == memtype)
            h5type = H5T_STD_U16BE;
       else if(H5T_NATIVE_INT == memtype)
            h5type = H5T_STD_I32BE;
       else if(H5T_NATIVE_UINT == memtype)
            h5type = H5T_STD_U32BE;
       else if(H5T_NATIVE_LONG == memtype)
       {
            if(is64bitMachine)
                h5type = H5T_STD_I64BE;
            else
                h5type = H5T_STD_I32BE;
       }
       else if(H5T_NATIVE_ULONG == memtype)
       {
            if(is64bitMachine)
                h5type = H5T_STD_U64BE;
            else
                h5type = H5T_STD_U32BE;
       }
       else if(H5T_NATIVE_FLOAT == memtype)
            h5type = H5T_IEEE_F32BE;
       else if(H5T_NATIVE_LLONG == memtype)
            h5type = H5T_STD_I64BE;
       else if(H5T_NATIVE_ULLONG == memtype)
            h5type = H5T_STD_U64BE;
       else if(H5T_NATIVE_DOUBLE == memtype)
            h5type = H5T_IEEE_F64BE;
       else
            fprintf(stderr, "\nUNKOWN TYPE: <%d>. file: %s, line: %d\n",
                            memtype, __FILE__, __LINE__);
    }
    else
    {
        if((H5T_NATIVE_CHAR == memtype) || (H5T_NATIVE_SCHAR == memtype))
            h5type = H5T_STD_I8LE;
        else if(H5T_NATIVE_UCHAR == memtype)
            h5type = H5T_STD_U8LE;
        else if(H5T_NATIVE_SHORT == memtype)
            h5type = H5T_STD_I16LE;
        else if(H5T_NATIVE_USHORT == memtype)
            h5type = H5T_STD_U16LE;
        else if(H5T_NATIVE_INT == memtype)
            h5type = H5T_STD_I32LE;
        else if(H5T_NATIVE_UINT == memtype)
            h5type = H5T_STD_U32LE;
        else if(H5T_NATIVE_LONG == memtype)
        {
            if(is64bitMachine)
                h5type = H5T_STD_I64LE;
            else
                h5type = H5T_STD_I32LE;
        }
        else if(H5T_NATIVE_ULONG == memtype)
        {
            if(is64bitMachine)
                h5type = H5T_STD_U64LE;
            else
                h5type = H5T_STD_U32LE;
        }
        else if(H5T_NATIVE_FLOAT == memtype)
            h5type = H5T_IEEE_F32LE;
        else if(H5T_NATIVE_LLONG == memtype)
            h5type = H5T_STD_I64LE;
        else if(H5T_NATIVE_ULLONG == memtype)
            h5type = H5T_STD_U64LE;
        else if(H5T_NATIVE_DOUBLE == memtype)
            h5type = H5T_IEEE_F64LE;
        else
            fprintf(stderr, "\nUNKOWN TYPE: <%d>. file: %s, line: %d\n",
                               memtype, __FILE__, __LINE__);
    }

    return h5type;
}

void *_Ncl2HDF5type(NclBasicDataTypes type)
{ 
    char *HDF5type;

    switch(type)
    {
        case NCL_float:
            HDF5type = strdup("float");
            break;
        case NCL_double:
            HDF5type = strdup("double");
            break;
        case NCL_string:
          /*Need to return NULL, as NclFile.c which check for NCL_string again.
           *Wei, 3/31/2010
           *HDF5type = strdup("string");
           */
            return(NULL);
            break;
        case NCL_int64:
            HDF5type = strdup("int64");
            break;
        case NCL_uint64:
            HDF5type = strdup("uint64");
            break;
        case NCL_long:
            HDF5type = strdup("long");
            break;
        case NCL_ulong:
            HDF5type = strdup("ulong");
            break;
        case NCL_int:
            HDF5type = strdup("int");
            break;
        case NCL_uint:
            HDF5type = strdup("uint");
            break;
        case NCL_short:
            HDF5type = strdup("short");
            break;
        case NCL_ushort:
            HDF5type = strdup("ushort");
            break;
        case NCL_char:
            HDF5type = strdup("char");
            break;
        case NCL_byte:
            HDF5type = strdup("byte");
            break;
        case NCL_ubyte:
            HDF5type = strdup("ubyte");
            break;
        case NCL_compound:
            HDF5type = strdup("compound");
            break;
        default:
            return(NULL);
    }
    
    return (void *)HDF5type;
}

hid_t Ncltype2HDF5type(NclBasicDataTypes type)
{
    hid_t h5type = -1;

    switch(type)
    {
         case NCL_int:
              h5type = H5T_NATIVE_INT;
              break;
         case NCL_uint:
              h5type = H5T_NATIVE_UINT;
              break;
         case NCL_long:
              h5type = H5T_NATIVE_LONG;
              break;
         case NCL_ulong:
              h5type = H5T_NATIVE_ULONG;
              break;
         case NCL_int64:
              h5type = H5T_NATIVE_LLONG;
              break;
         case NCL_uint64:
              h5type = H5T_NATIVE_ULLONG;
              break;
         case NCL_short:
              h5type = H5T_NATIVE_SHORT;
              break;
         case NCL_ushort:
              h5type = H5T_NATIVE_USHORT;
              break;
         case NCL_byte:
              h5type = H5T_NATIVE_CHAR;
              break;
         case NCL_ubyte:
              h5type = H5T_NATIVE_UCHAR;
              break;
         case NCL_char:
              h5type = H5T_NATIVE_CHAR;
              break;
         case NCL_float:
              h5type = H5T_NATIVE_FLOAT;
              break;
         case NCL_double:
              h5type = H5T_NATIVE_DOUBLE;
              break;
         case NCL_string:
              h5type = H5T_STRING;
              break;
         case NCL_compound:
              h5type = H5T_COMPOUND;
              break;
         default:
              if(type & NCL_enum)
                  h5type = H5T_ENUM;
              else if(type & NCL_opaque)
                  h5type = H5T_OPAQUE;
              else if(type & NCL_vlen)
                  h5type = H5T_VLEN;
              else
                  fprintf(stderr, "\nUNKOWN TYPE: <%d>. file: %s, line: %d\n", type, __FILE__, __LINE__);
              break;
    }

    return h5type;
}

hid_t toH5type(const char *type)
{
    hid_t h5type = -1;
    if(strcmp("integer", type) == 0)
    {
        h5type = H5T_NATIVE_INT;
    }
    else if(strcmp("int", type) == 0)
    {
        h5type = H5T_NATIVE_INT;
    }
    else if(strcmp("uint", type) == 0)
    {
        h5type = H5T_NATIVE_UINT;
    }
    else if(strcmp("long", type) == 0)
    {
        h5type = H5T_NATIVE_LONG;
    }
    else if(strcmp("ulong", type) == 0)
    {
        h5type = H5T_NATIVE_ULONG;
    }
    else if(strcmp("int64", type) == 0)
    {
        h5type = H5T_NATIVE_LLONG;
    }
    else if(strcmp("uint64", type) == 0)
    {
        h5type = H5T_NATIVE_ULLONG;
    }
    else if(strcmp("short", type) == 0)
    {
        h5type = H5T_NATIVE_SHORT;
    }
    else if(strcmp("ushort", type) == 0)
    {
        h5type = H5T_NATIVE_USHORT;
    }
    else if(strcmp("byte", type) == 0)
    {
        h5type = H5T_NATIVE_CHAR;
    }
    else if(strcmp("ubyte", type) == 0)
    {
        h5type = H5T_NATIVE_UCHAR;
    }
    else if(strcmp("char", type) == 0)
    {
        h5type = H5T_NATIVE_CHAR;
    }
    else if(strcmp("float", type) == 0)
    {
        h5type = H5T_NATIVE_FLOAT;
    }
    else if(strcmp("double", type) == 0)
    {
        h5type = H5T_NATIVE_DOUBLE;
    }
    else if(strcmp("string", type) == 0)
    {
        h5type = H5T_STRING;
    }
    else if(strcmp("compound", type) == 0)
    {
        h5type = H5T_COMPOUND;
    }
    else if(strcmp("enum", type) == 0)
    {
        h5type = H5T_ENUM;
    }
    else if(strcmp("vlen", type) == 0)
    {
        h5type = H5T_VLEN;
    }
    else
    {
        fprintf(stderr, "\nUNKOWN TYPE: <%s>. file: %s, line: %d\n", type, __FILE__, __LINE__);
    }

    return h5type;
}

NclQuark _string2quark(char *nm)
{
    NclQuark qn;
    char newnm[MAX_NCL_NAME_LENGTH];
    int n;

    memset(newnm, 0, MAX_NCL_NAME_LENGTH);
    strcpy(newnm, nm);

    for(n = 0; n < strlen(nm); ++n)
        if(! isalnum(nm[n]))
            newnm[n] = '_';

    qn = NrmStringToQuark(newnm);

    return qn;
}

static void _setpid(NclFileGrpNode *grpnode)
{
    if(grpnode->pid < 0)
        _setpid(grpnode->parent);
    grpnode->gid = H5Gopen(grpnode->pid, NrmQuarkToString(grpnode->name), H5P_DEFAULT);
}

static int _getH5grpID(NclFileGrpNode *grpnode)
{
    hid_t id = -1;

  /*
   *fprintf(stderr, "\nEntering _getH5grpID, int file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tname = <%s>, id: %ld\n",
   *                   NrmQuarkToString(grpnode->name), grpnode->gid);
   */

    if(0 <= grpnode->gid)
        id = grpnode->gid;
    else
        id = grpnode->fid;

    if(id <= 0)
    {

        if(strcmp(NrmQuarkToString(grpnode->real_name), "/"))
        {
          /*
           *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tNeed to figure out how to open group, name: <%s>, real_name: <%s>.\n",
           *                   NrmQuarkToString(grpnode->name),
           *                   NrmQuarkToString(grpnode->real_name));
           */

            if(grpnode->pid > 0)
            {
                id = H5Gopen(grpnode->pid, NrmQuarkToString(grpnode->name), H5P_DEFAULT);
                grpnode->pid = id;
                grpnode->gid = id;
            }
            else
            {
                _setpid(grpnode);
                id = grpnode->fid;
            }
        }
        else
        {
            id = H5Fopen(NrmQuarkToString(grpnode->path), H5F_ACC_RDWR, H5P_DEFAULT);
            grpnode->fid = id;
            grpnode->gid = id;
            grpnode->pid = -1;
        }

        if(id < 0)
        {
            NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                  "NclNewHDF5 _getH5grpID: Could not reopen the file (%s) for writing",
                  NrmQuarkToString(grpnode->path)));
            return(NhlFATAL);
        }
    }
    grpnode->define_mode = 0;
    grpnode->open = 1;

  /*
   *fprintf(stderr, "\tid: %d\n", id);
   *fprintf(stderr, "Leaving _getH5grpID, at file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (int)id;
}

/*
 ***********************************************************************
 * Function:	_getH5typeName
 *
 * Purpose:	Get typename based on type
 *
 * Return:	char *
 *
 * Programmer:	Wei Huang
 * Created:	April 11, 2012
 *
 ***********************************************************************
 */

char *_getH5typeName(hid_t type, int ind)
{
    char *attTypeName;
    char endian[16];
    char format[128];
    H5T_class_t type_class;
    hsize_t     size;
    int         bit, usign;

    attTypeName = (char *)NclMalloc(MAX_NCL_NAME_LENGTH);
    strcpy(attTypeName, "Bad Data Type");
    /* Bad data type */
    if(type < 0)
    {
        fprintf(stderr, "\n\t********************************************\n");
        fprintf(stderr, "\tBad data type: %d\n", type);
        fprintf(stderr, "\t<ERROR ERROR ERROR ERROR ERROR ERROR ERROR>\n");
        fprintf(stderr, "\tBad data type: %d\n", type);
        fprintf(stderr, "\t********************************************\n\n");
        return attTypeName;
    }

    /* Shared? If so then print the type's OID */
    if(H5Tcommitted(type))
    {
        H5O_info_t  oi;

        if(H5Oget_info(type, &oi) >= 0)
        {
          /*
           *fprintf(stderr, "shared-%lu:"H5_PRINTF_HADDR_FMT" ",
           *        oi.fileno, oi.addr);
           *fprintf(stderr, "shared-%lu:"H5_PRINTF_HADDR_FMT" ",
           */
            strcpy(attTypeName, "shared");
        }
        else
        {
            strcpy(attTypeName, "shared");
        }

        return attTypeName;
    }

    if (H5Tequal(type, H5T_IEEE_F32BE)==TRUE)
    {
        bit = 32;
        strcpy(attTypeName, "float");
        strcpy(endian, "big-endian");
        strcpy(format, "IEEE");
        return attTypeName;
    }
    else if (H5Tequal(type, H5T_IEEE_F32LE)==TRUE)
    {
        bit = 32;
        strcpy(attTypeName, "float");
        strcpy(endian, "little-endian");
        strcpy(format, "IEEE");
        return attTypeName;
    }
    else if (H5Tequal(type, H5T_IEEE_F64BE)==TRUE)
    {
        bit = 64;
        strcpy(attTypeName, "double");
        strcpy(endian, "big-endian");
        strcpy(format, "IEEE");
        return attTypeName;
    }
    else if (H5Tequal(type, H5T_IEEE_F64LE)==TRUE)
    {
        bit = 64;
        strcpy(attTypeName, "double");
        strcpy(endian, "little-endian");
        strcpy(format, "IEEE");
        return attTypeName;
    }
#if 0
    else
    {
        fprintf(stderr, "\n\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
        fprintf(stderr, "\tNOT a Simple Type.\n");
    }
#endif

    size = H5Tget_size(type);
    bit = (unsigned) (8*size);

    usign = bit;

    type_class = H5Tget_class(type);

    switch (type_class)
    {
        case H5T_COMPOUND:
            {
                char        *name=NULL;     /* member name */
                hid_t       subtype;        /* member data type */
                unsigned    nmembs;         /* number of members */
                unsigned    i;              /* miscellaneous counters */

                strcpy(attTypeName, "compound");

                nmembs=H5Tget_nmembers(type);

                for (i=0; i<nmembs; i++)
                {
                    /* Name and offset */
                    name = H5Tget_member_name(type, i);
                    free(name);
            
                    /* Member's type */
                    subtype = H5Tget_member_type(type, i);
                    H5Tclose(subtype);
                }
                size = H5Tget_size(type);

                bit = size;
            }
            return attTypeName;
            break;
        case H5T_INTEGER:
            {
                H5T_sign_t  sign;           /* sign scheme value */
                char  var_name[32];
#if 0
                const char  *sign_s="unknown-sign";   /* sign scheme string */
#endif
                /* Sign */
                sign = H5Tget_sign(type);
                if((sign >= 0) && (H5T_SGN_NONE == sign))
                {
                    usign = 1;
                    strcpy(attTypeName, "uint");
                }

#if 0
                if((sign >= 0) && (H5T_SGN_2==sign))
                {
                    sign_s = "";
                }
#endif

                strcpy(attTypeName, "integer");

                if(size == (hsize_t) sizeof(char))
                {
                    if(1 == usign)
                        strcpy(var_name, "ubyte");
                    else
                        strcpy(var_name, "byte");
                }
                else if(size == (hsize_t) sizeof(short))
                {
                    if(1 == usign)
                        strcpy(var_name, "ushort");
                    else
                        strcpy(var_name, "short");
                }
                else if(size == (hsize_t) sizeof(int))
                {
                    if(1 == usign)
                        strcpy(var_name, "uint");
                    else
                        strcpy(var_name, "integer");
                }
                else if(size == (hsize_t) sizeof(long long))
                {
                    if(1 == usign)
                        strcpy(var_name, "uint64");
                    else
                        strcpy(var_name, "int64");
                }
                else if(size == (hsize_t) sizeof(long))
                {
                    if(1 == usign)
                        strcpy(var_name, "ulong");
                    else
                        strcpy(var_name, "long");
                }
                else
                {
                    fprintf(stderr, "file: %s, line: %d\n", __FILE__, __LINE__);
                    fprintf(stderr, "size: <%ld>\n", (long)size);
                    strcpy(var_name, "unknown integer");
                }
                strcpy(attTypeName, var_name);
            }
            return attTypeName;
            break;
        case H5T_FLOAT:
            {
                size_t      spos;           /* sign bit position */
                size_t      esize, epos;    /* exponent size and position */
                size_t      msize, mpos;    /* significand size and position */
#if 0
                size_t      ebias;          /* exponent bias */
                H5T_norm_t  norm;           /* significand normalization */
                const char  *norm_s=NULL;   /* normalization string */
                H5T_pad_t   pad;            /* internal padding value */
                const char  *pad_s=NULL;    /* internal padding string */
#endif

                strcpy(attTypeName, "float");

                /* Print sizes, locations, and other information about each field */
                H5Tget_fields (type, &spos, &epos, &esize, &mpos, &msize);
#if 0
                ebias = H5Tget_ebias(type);
                norm = H5Tget_norm(type);
                switch (norm)
                {
                    case H5T_NORM_IMPLIED:
                        norm_s = ", msb implied";
                        break;
                    case H5T_NORM_MSBSET:
                        norm_s = ", msb always set";
                        break;
                    case H5T_NORM_NONE:
                        norm_s = ", no normalization";
                        break;
                    case H5T_NORM_ERROR:
                        norm_s = ", unknown normalization";
                        break;
                }
              /*
               *fprintf(stderr, "\n%*s(significant for %lu bit%s at bit %lu%s)", ind, "",
               *        (unsigned long)msize, 1==msize?"":"s", (unsigned long)mpos,
               *        norm_s);
               *fprintf(stderr, "\n%*s(exponent for %lu bit%s at bit %lu, bias is 0x%lx)",
               *        ind, "", (unsigned long)esize, 1==esize?"":"s",
               *        (unsigned long)epos, (unsigned long)ebias);
               *fprintf(stderr, "\n%*s(sign bit at %lu)", ind, "", (unsigned long)spos);
               */
    
                /* Display internal padding */
                if (1+esize+msize<H5Tget_precision(type))
                {
                    pad = H5Tget_inpad(type);
                    switch (pad)
                    {
                        case H5T_PAD_ZERO:
                            pad_s = "zero";
                            break;
                        case H5T_PAD_ONE:
                            pad_s = "one";
                            break;
                        case H5T_PAD_BACKGROUND:
                            pad_s = "bkg";
                            break;
                        case H5T_PAD_ERROR:
                        case H5T_NPAD:
                            pad_s = "unknown";
                            break;
                    }
                }
              /*
               *fprintf(stderr, "\n%*s(internal padding bits are %s)", ind, "", pad_s);
               */
#endif
            }
            return attTypeName;
            break;
        case H5T_ENUM:
            {
#if 0
                char        **name=NULL;    /* member names */
                unsigned char *value=NULL;  /* value array */
                unsigned char *copy = NULL; /* a pointer to value array */
                unsigned    nmembs;         /* number of members */
                hid_t       super;          /* enum base integer type */
                hid_t       native=-1;      /* native integer data type */
                hsize_t     dst_size;       /* destination value type size */
                unsigned    i;              /* miscellaneous counters */
                hsize_t     j;
#endif

                strcpy(attTypeName, "enum");

#if 0
                nmembs = H5Tget_nmembers(type);
                assert(nmembs>0);
                super = H5Tget_super(type);
                fprintf(stderr, "\n%*s", ind, "enum {");

                /* Determine what data type to use for the native values.  To simplify
                 * things we entertain three possibilities:
                 *  1. long_long -- the largest native signed integer
                 * 2. unsigned long_long -- the largest native unsigned integer
                 *     3. raw format */
                if (H5Tget_size(type)<=sizeof(long long))
                {
                    dst_size = sizeof(long long);
                    if (H5T_SGN_NONE==H5Tget_sign(type))
                    {
                        native = H5T_NATIVE_ULLONG;
                    }
                    else
                    {
                        native = H5T_NATIVE_LLONG;
                    }
                }
                else
                {
                    dst_size = size;
                }

                bit = (unsigned) (8*dst_size);

                /* Get the names and raw values of all members */
                name = NclCalloc(nmembs, sizeof(char*));
                if(dst_size > size)
                    value = NclCalloc(nmembs, dst_size);
                else
                    value = NclCalloc(nmembs, size);

                for (i=0; i<nmembs; i++)
                {
                    name[i] = H5Tget_member_name(type, i);
                    H5Tget_member_value(type, i, value+i*H5Tget_size(type));
                }

                /* Convert values to native data type */
                if (native>0) H5Tconvert(super, native, nmembs, value, NULL, H5P_DEFAULT);

                /* Sort members by increasing value */
                /*not implemented yet*/

                /* Print members */
                for (i=0; i<nmembs; i++)
                {
                    fprintf(stderr, "\n%*s", ind+4, "");
                    fprintf(stderr, "%s = ", name[i]);

                    if (native<0)
                    {
                        fprintf(stderr, "0x");
                        for (j=0; j<dst_size; j++)
                            fprintf(stderr, "%02x", value[i*dst_size+j]);
                    }
                    else if (H5T_SGN_NONE==H5Tget_sign(native))
                    {
 	                /*On SGI Altix(cobalt), wrong values were printed out with "value+i*dst_size"
 	                 *strangely, unless use another pointer "copy".*/
 	                copy = value+i*dst_size;
                        fprintf(stderr,"%"H5_PRINTF_LL_WIDTH"u",
                        *((unsigned long long*)((void*)copy)));
                    }
                    else
                    {
 	                /*On SGI Altix(cobalt), wrong values were printed out with "value+i*dst_size"
 	                 *strangely, unless use another pointer "copy".*/
 	                copy = value+i*dst_size;
                        fprintf(stderr,"%"H5_PRINTF_LL_WIDTH"d",
                        *((long long*)((void*)copy)));
                    }
                }

                /* Release resources */
                for (i=0; i<nmembs; i++) free(name[i]);
                free(name);
                free(value);
                H5Tclose(super);

                if (0==nmembs) fprintf(stderr, "\n%*s <empty>", ind+4, "");
                fprintf(stderr, "\n%*s}\n\n", ind, "");
#endif
            }
            return attTypeName;
            break;
        case H5T_STRING:
            {
#if 0
                H5T_cset_t  cset;
                H5T_str_t  pad;
                const char  *pad_s=NULL;
                const char  *cset_s=NULL;

                /* Padding */
                pad = H5Tget_strpad(type);
                switch (pad)
                {
                    case H5T_STR_NULLTERM:
                        pad_s = "null-terminated";
                        break;
                    case H5T_STR_NULLPAD:
                        pad_s = "null-padded";
                        break;
                    case H5T_STR_SPACEPAD:
                        pad_s = "space-padded";
                        break;
                    case H5T_STR_RESERVED_3:
                    case H5T_STR_RESERVED_4:
                    case H5T_STR_RESERVED_5:
                    case H5T_STR_RESERVED_6:
                    case H5T_STR_RESERVED_7:
                    case H5T_STR_RESERVED_8:
                    case H5T_STR_RESERVED_9:
                    case H5T_STR_RESERVED_10:
                    case H5T_STR_RESERVED_11:
                    case H5T_STR_RESERVED_12:
                    case H5T_STR_RESERVED_13:
                    case H5T_STR_RESERVED_14:
                    case H5T_STR_RESERVED_15:
                    case H5T_STR_ERROR:
                        pad_s = "unknown-format";
                        break;
                }

                /* Character set */
                cset = H5Tget_cset(type);
                switch (cset)
                {
                    case H5T_CSET_ASCII:
                        cset_s = "ASCII";
                        break;
                    case H5T_CSET_UTF8:
                        cset_s = "UTF-8";
                        break;
                    case H5T_CSET_RESERVED_2:
                    case H5T_CSET_RESERVED_3:
                    case H5T_CSET_RESERVED_4:
                    case H5T_CSET_RESERVED_5:
                    case H5T_CSET_RESERVED_6:
                    case H5T_CSET_RESERVED_7:
                    case H5T_CSET_RESERVED_8:
                    case H5T_CSET_RESERVED_9:
                    case H5T_CSET_RESERVED_10:
                    case H5T_CSET_RESERVED_11:
                    case H5T_CSET_RESERVED_12:
                    case H5T_CSET_RESERVED_13:
                    case H5T_CSET_RESERVED_14:
                    case H5T_CSET_RESERVED_15:
                    case H5T_CSET_ERROR:
                        cset_s = "unknown-character-set";
                        break;
                }
#endif

                strcpy(attTypeName, "string");

                if (H5Tis_variable_str(type))
                {
                    size = H5Tget_size(type);
                  /*
                   *fprintf(stderr, "\n\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
                   *fprintf(stderr, "\tsize = %d\n", size);
                   *fprintf(stderr, "variable-length");
                   *fprintf(stderr, " %s %s string\n", pad_s, cset_s);
                   */
                    bit = 0;
                }
                else
                {
                  /*
                   *printf("%lu-byte", (unsigned long)size);
                   */
                    bit = (unsigned) (8*size);
                }
            }
            return attTypeName;
            break;
        case H5T_REFERENCE:
            if (H5Tequal(type, H5T_STD_REF_OBJ))
            {
              /*
               *fprintf(stderr, "object reference, file: %s, line: %d\n\n", __FILE__, __LINE__);
               */
                strcpy(attTypeName, "object reference");
            }
            else if (H5Tequal(type, H5T_STD_REF_DSETREG))
            {
              /*
               *fprintf(stderr, "file: %s, line: %d\n\n", __FILE__, __LINE__);
               *fprintf(stderr, "dataset region reference\n");
               */
                strcpy(attTypeName, "dataset region reference");
            }
            else
            {
                strcpy(attTypeName, "unknown reference");
                bit = (unsigned) (8*size);
              /*
               *fprintf(stderr, "file: %s, line: %d\n\n", __FILE__, __LINE__);
               *fprintf(stderr, "%lu-byte unknown reference\n", (unsigned long)size);
               */
            }

            return attTypeName;
            break;
        case H5T_OPAQUE:
            {
              /*
               *char *tag;

               *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\t%lu-byte opaque type\n", (unsigned long)size);
               *if ((tag=H5Tget_tag(type)))
               *{
               *    printf("\n%*s(tag = \"", ind, "");
               *    printf("%s\")", tag);
               *    strcpy(format, tag);
               *    free(tag);
               *}
               */

                strcpy(attTypeName, "opaque");
                bit = (unsigned) (8*size);
            }
            return attTypeName;
            break;
        case H5T_VLEN:
            {
                hid_t       super;

                strcpy(attTypeName, "vlen");

                super = H5Tget_super(type);
              /*
               *attTypeName = _getH5typeName(super, ind+4);
               *fprintf(stderr, "file: %s, line: %d\n\n", __FILE__, __LINE__);
               *fprintf(stderr, "variable length of %s\n", attTypeName);
               */
                H5Tclose(super);
            }
            return attTypeName;
            break;
        case H5T_ARRAY:
            {
                hid_t       super;
                int         ndims;
                hsize_t     *dims=NULL;

                strcpy(attTypeName, "array");

                ndims = H5Tget_array_ndims(type);
                if (ndims)
                {
                    dims = NclMalloc(ndims*sizeof(dims[0]));
                    H5Tget_array_dims2(type, dims);

                    free(dims);
                }
                else
                {
                    strcpy(attTypeName, "scalar");
                    fputs(" [SCALAR]", stderr);
                }

                /* Print parent type */
                putchar(' ');
                super = H5Tget_super(type);
                attTypeName = _getH5typeName(super, ind+4);
                H5Tclose(super);
            }
            return attTypeName;
            break;
        case H5T_BITFIELD:
            strcpy(attTypeName, "bitfield");

            bit = (unsigned) (8*size);
        
            return attTypeName;
            break;
        default:
            printf("Unknon type_class in file: %s, line: %d\n", __FILE__, __LINE__);
            /* Unknown type */
            printf("%lu-byte class-%u unknown",
                    (unsigned long)H5Tget_size(type), (unsigned)type_class);
            return attTypeName;
            break;
    }
}

NclFileEnumRecord *readH5EnumAtt(hid_t type)
{
    NclFileEnumRecord *enumrec;

    char          *membname=NULL;    /* member names */
    unsigned char *membvalue=NULL;  /* value array */
    unsigned int   nmembs;         /* number of members */
    unsigned int   n;              /* miscellaneous counters */
    size_t         size;

    nmembs = H5Tget_nmembers(type);
    assert(nmembs>0);

  /*
   *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tfound enum data, nmembs = %d\n", nmembs);
   */

  /*Let add two more attributes to varnode here.
   *1. an array of the enum names
   *2. an array of the enum value
   */

    enumrec = _NclFileEnumAlloc(nmembs);
    enumrec->name = NrmStringToQuark("ENUM record");
    enumrec->size = nmembs;

    size = H5Tget_size(type);

    switch(size)
    {
        case 1:
             enumrec->type = NCL_ubyte;
             break;
        case 2:
             enumrec->type = NCL_ushort;
             break;
        case 4:
             enumrec->type = NCL_uint;
             break;
        default:
             enumrec->type = NCL_uint64;
             break;
    }

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tenumrec->name: <%s>, enumrec->type: <%s>, enumrec->size = %d\n",
   *                 NrmQuarkToString(enumrec->name),
   *                 _NclBasicDataTypeToName(enumrec->type), enumrec->size);
   */

    membname = NclCalloc(256, sizeof(char));
    membvalue = NclCalloc(1, size);

    for(n=0; n<nmembs; ++n)
    {
        membname = H5Tget_member_name(type, n);
        enumrec->enum_node[n].name = NrmStringToQuark(membname);
    }

    switch(enumrec->type)
    {
        case NCL_ubyte:
             {
             unsigned char *iptr = (unsigned char *) membvalue;
             for(n = 0; n < enumrec->size; n++)
             {
                 H5Tget_member_value(type, n, membvalue);
                 enumrec->enum_node[n].value = iptr[0];
             }
             }
             break;
        case NCL_ushort:
             {
             unsigned short *iptr = (unsigned short *) membvalue;
             for(n = 0; n < enumrec->size; n++)
             {
                 H5Tget_member_value(type, n, membvalue);
                 enumrec->enum_node[n].value = iptr[0];
             }
             }
             break;
        case NCL_uint:
             {
             unsigned int *iptr = (unsigned int *) membvalue;
             for(n = 0; n < enumrec->size; n++)
             {
                 H5Tget_member_value(type, n, membvalue);
                 enumrec->enum_node[n].value = iptr[0];
             }
             }
             break;
        default:
             {
             uint64 *iptr = (uint64 *) membvalue;
             for(n = 0; n < enumrec->size; n++)
             {
                 H5Tget_member_value(type, n, membvalue);
                 enumrec->enum_node[n].value = iptr[0];
             }
             }
             break;
    }
    
    NclFree(membname);
    NclFree(membvalue);

    return ((void *) enumrec);
}


/*
 *************************************************************************
 * Function:	_checkH5attribute
 *
 * Purpose:	Check information about attributes.
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 *              April 11, 2012
 *
 *************************************************************************
 */

herr_t _checkH5attribute(hid_t obj_id, char *attr_name, const H5A_info_t *ainfo,
                          void *attr_data)
{
    hid_t       attr_id, space, type, p_type;
    hsize_t     size[H5S_MAX_RANK], nelmts = 1;
    hsize_t     temp_need;
    hsize_t need;

    H5S_class_t class;
    H5T_class_t type_class;

    herr_t      status;

    int  ndims, i;
  
    NclFileAttRecord **attr_record = (NclFileAttRecord **) attr_data;

    NclFileAttRecord *attrec = *attr_record;
    NclFileAttNode   *attnode;

    char *type_name = NULL;

  /*
   *fprintf(stderr, "\nEntering _checkH5attribute, at file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tattr_name = <%s>\n", attr_name);
   */

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);

    if(attr_id < 0)
    {
        fprintf(stderr, "\n\n\n");
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "PROBLEM TO FIND ATTRIBUTE in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tobj_id: %d\n", obj_id);
        fprintf(stderr, "\tattr_name: <%s>\n", attr_name);
        fprintf(stderr, "\tattr_id: %d\n", attr_id);
        fprintf(stderr, "PROBLEM TO FIND ATTRIBUTE in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "\n\n\n");
      /*
       *fprintf(stderr, "Leaving _checkH5attribute, at file: %s, line: %d\n\n", __FILE__, __LINE__);
       */
        H5Aclose(attr_id);
        return FAILED;
    }

    type = H5Aget_type(attr_id);

    if(type < 0)
    {
        fprintf(stderr, "\n\n\n");
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "PROBLEM TO FIND ATTRIBUTE in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tobj_id: %d\n", obj_id);
        fprintf(stderr, "\tattr_name: <%s>\n", attr_name);
        fprintf(stderr, "\tattr_id: %d\n", attr_id);
        fprintf(stderr, "\ttype: %d\n", type);
        fprintf(stderr, "PROBLEM TO FIND ATTRIBUTE in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "\n\n\n");
        H5Tclose(type);
        H5Aclose(attr_id);

      /*
       *fprintf(stderr, "Leaving _checkH5attribute, at file: %s, line: %d\n\n", __FILE__, __LINE__);
       */
        return FAILED;
    }

    space = H5Aget_space(attr_id);

    if(NULL == attrec)
    {
        attrec = _NclFileAttAlloc(4);
        *attr_record = attrec;
        attrec->n_atts = 0;
        attrec->gid = obj_id;
        attrec->aid = attr_id;
        attrec->id = -1;
    }
    else if(attrec->n_atts >= attrec->max_atts)
    {
        _NclFileAttRealloc(&attrec);
    }
    
    attnode = &(attrec->att_node[attrec->n_atts]);

    attnode->name = NrmStringToQuark(attr_name);
    attnode->type = type;

    ++attrec->n_atts;

  /*
   *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tattr_id   = %d\n", attr_id);
   *fprintf(stderr, "\tattnode->type = %d\n", attnode->type);
   *fprintf(stderr, "\tattnode->name = <%s>\n", NrmQuarkToString(attnode->name));
   *fprintf(stderr, "\tattrec->n_atts = %d\n", attrec->n_atts);
   */

    /* Data space */
    ndims = H5Sget_simple_extent_ndims(space);
    H5Sget_simple_extent_dims(space, size, NULL);

    if(ndims > 10)
    {
        fprintf(stderr, "\n\n\n");
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "PROBLEM WITH NDIMS in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tobj_id: %d\n", obj_id);
        fprintf(stderr, "\tattr_name: <%s>\n", attr_name);
        fprintf(stderr, "\tattr_id: %d\n", attr_id);
        fprintf(stderr, "\tndims: %d\n", ndims);
        fprintf(stderr, "PROBLEM WITH NDIMS in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "\n\n\n");

        H5Tclose(type);
        H5Aclose(attr_id);

      /*
       *fprintf(stderr, "Leaving _checkH5attribute, at file: %s, line: %d\n\n", __FILE__, __LINE__);
       */

        return FAILED;
    }

    class = H5Sget_simple_extent_type(space);

  /*
   *fprintf(stderr, "\tn file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tndims = %d\n", ndims);
   */

    switch(class)
    {
        case H5S_SCALAR:
            /* scalar dataspace */
            attnode->n_elem = 1;
            break;
        case H5S_SIMPLE:
            /* simple dataspace */
            attnode->n_elem = 1;
            for (i=0; i<ndims; i++)
            {
                attnode->n_elem *= size[i];
            }
            break;
        case H5S_NULL:
            /* null dataspace */
            attnode->n_elem = 0;
            break;

        default:
            /* Unknown dataspace type */
            attnode->n_elem = 0;
            break;
    }

  /*Data type name*/
    type_name = _getH5typeName(type, 15);

  /*
   *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\ttype_name: <%s>\n", type_name);
   */

    attnode->type = string2NclType(type_name);

    /* values of type reference */
    type_class = H5Tget_class(type);
    if(H5T_REFERENCE == type_class)
    {
        H5O_type_t obj_type;
        hobj_ref_t *rbuf;

      /*
       *hid_t dataset_id;
       *hid_t type_id;
       */

      /*
       *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\ttype_class = %d, H5T_REFERENCE = %d\n", type_class, H5T_REFERENCE);
       */

        rbuf = (hobj_ref_t *) NclMalloc(attnode->n_elem * sizeof(hobj_ref_t));

        p_type=H5Tcopy(type);

        status = H5Aread(attr_id, H5T_STD_REF_OBJ, rbuf);

      /*
       *Find the type of referenced objects.
       */
        for(i = 0; i < attnode->n_elem; ++i)
        {
            status = H5Rget_obj_type2(attr_id, H5R_OBJECT, &rbuf[i], &obj_type);

            if(0 != status)
            {
                fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
            }

#if 0
            switch(obj_type)
            {
                case H5O_TYPE_GROUP:
                     fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
                     fprintf(stderr, "\tdereferenced object is a group. \n");
                     break;
                case H5O_TYPE_DATASET:
                     fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
                     fprintf(stderr, "\tdereferenced object is a dataset. \n");
                   /*
                    *dataset_id = H5Rdereference(attr_id, H5R_OBJECT, &rbuf[i]);
                    *type_id = H5Dget_type(dataset_id);

                    *if(H5Tequal(H5T_NATIVE_FLOAT, type_id))
                    *{
                    *    fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
                    *    fprintf(stderr, "\tDatatype of the dataset is H5T_NATIVE_FLOAT.\n\n");
                    *}
                    *else if(H5Tequal(H5T_NATIVE_INT, type_id))
                    *{
                    *    fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
                    *    fprintf(stderr, "\tDatatype of the dataset is H5T_NATIVE_INT.\n\n");
                    *}
                    */
                     break;
                default:
                     fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
                     fprintf(stderr, "\tdereferenced object is UNKNOWN. \n");
                     break;
            }
#endif
        }

      /*
       *Get datatype of the dataset "B"

       *did_b = H5Rdereference(attr_id, H5R_OBJECT, &rbuf[1]);
       *tid_b = H5Dget_type(did_b);
       *if(H5Tequal(tid_b, H5T_NATIVE_FLOAT))
       *    printf("Datatype of the dataset is H5T_NATIVE_FLOAT.\n");
       *printf("\n");
       
       *NclFree(rbuf);
       */

        attnode->value = (void *)rbuf;

        free(type_name);

        H5Tclose(p_type);

        H5Sclose(space);
        H5Tclose(type);

      /*
       *fprintf(stderr, "Leaving _checkH5attribute, at file: %s, line: %d\n\n", __FILE__, __LINE__);
       */

        return SUCCEED;
    }
    else if(H5T_BITFIELD == type_class)
        p_type=H5Tcopy(type);
    else
        p_type = H5Tget_native_type(type,H5T_DIR_DEFAULT);

    for(i=0; i<ndims; ++i)
    {
        nelmts *= size[i];
    }

    if(p_type >= 0)
    {
        char *tmpstr;

        hsize_t t_size = H5Tget_size(type);
        hsize_t p_size = H5Tget_size(p_type);

        if(p_size > t_size)
            temp_need = nelmts * p_size;
        else
            temp_need = nelmts * t_size;

        need = (size_t)temp_need;

      /*
        assert(temp_need == (hsize_t)((size_t)temp_need));
       */
      /*
       *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tt_size = %d, p_size = %d\n", t_size, p_size);
       *fprintf(stderr, "\tnelmts = %d, need = %d\n", nelmts, need);
       */

        if(0 == strcmp(type_name, "string"))
        {
            size_t      str_size=0;
          /*
           *H5T_str_t   str_pad;
           *H5T_cset_t  cset;
           */
            hid_t       tmp_type;
            htri_t      is_vlstr=FALSE;
            NrmQuark   *qptr;

            tmp_type = H5Tcopy(type);
            str_size = H5Tget_size(tmp_type);
          /*
           *str_pad = H5Tget_strpad(tmp_type);
           *cset = H5Tget_cset(tmp_type);
           */
            is_vlstr = H5Tis_variable_str(tmp_type);

            tmpstr = (char *)NclMalloc(str_size + 1);
            assert(tmpstr);
            tmpstr[str_size] = '\0';

            attnode->value = NclMalloc(attnode->n_elem * sizeof(NrmQuark));
            assert(attnode->value);
            qptr = (NrmQuark *)attnode->value;

          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\ttype_name=<%s>\n", type_name);
           *fprintf(stderr, "\tstr_size=<%d>\n", str_size);
           *fprintf(stderr, "\tis_vlstr=<%d>\n", is_vlstr);
           */

            if(is_vlstr)
            {
                char      **vlstr;
                vlstr = (char **) NclCalloc(attnode->n_elem, sizeof(char *));
                assert(vlstr);
                status = H5Aread(attr_id, type, vlstr);

                for(i = 0; i < nelmts; ++i)
                {
                    qptr[i] = NrmStringToQuark(vlstr[i]);
                    NclFree(vlstr[i]);
                }

                NclFree(vlstr);
            }
            else
            {
                char cp[MAX_NCL_BUFFER_LENGTH];

                memset(cp, 0, MAX_NCL_BUFFER_LENGTH);

                status = H5Aread(attr_id, tmp_type, cp);

                for(i = 0; i < nelmts; ++i)
                {
                    memcpy(tmpstr, cp + i * str_size, str_size);
                    qptr[i] = NrmStringToQuark(tmpstr);
                }
            }
            free(tmpstr);
        }
        else if(NCL_enum == attnode->type)
        {
            NclFileEnumRecord *enumrec = readH5EnumAtt(type);
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tatt name: <%s>, att type: <%s>\n",
           *                 NrmQuarkToString(attnode->name),
           *                 _NclBasicDataTypeToName(attnode->type));
           *fprintf(stderr, "\tNeed to read enum att.\n\n");
           */

            attnode->n_elem = nelmts;
            enumrec->n_enums = nelmts;
            enumrec->values = (void *)NclMalloc(need);
            assert(enumrec->values);

            status = H5Aread(attr_id, p_type, enumrec->values);
            attnode->value = (void *) enumrec;
            attnode->is_enum = 1;
        }
        else if(NCL_opaque == attnode->type)
        {
            NclFileOpaqueRecord *opaquerec = (NclFileOpaqueRecord *)NclMalloc(sizeof(NclFileOpaqueRecord));
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tatt name: <%s>, att type: <%s>\n",
           *                 NrmQuarkToString(attnode->name),
           *                 _NclBasicDataTypeToName(attnode->type));
           *fprintf(stderr, "\tNeed to read opaque att.\n\n");
           */

            attnode->n_elem = nelmts;
            opaquerec->n_opaques = nelmts;
            opaquerec->size = need/nelmts;
            opaquerec->type = NCL_opaque;
            opaquerec->name = NrmStringToQuark("opaque");
            opaquerec->values = (void *)NclMalloc(need);
            assert(opaquerec->values);

            status = H5Aread(attr_id, p_type, opaquerec->values);
            attnode->value = (void *) opaquerec;
            attnode->is_opaque = 1;

          /*
           *tmpstr = (char *)opaquerec->values;
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tatt value: <%s>\n", tmpstr);
           */
        }
        else if(NCL_compound == attnode->type)
        {
            char    *mname = NULL;     /* member name */
            char    *typename = NULL;
            hid_t    subtype;        /* member data type */
            unsigned nmembs;         /* number of members */
            unsigned i;              /* miscellaneous counters */

            NclFileCompoundRecord *comprec = NULL;
            NclFileCompoundNode   *compnode = NULL;

          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tatt name: <%s>, att type: <%s>\n",
           *                 NrmQuarkToString(attnode->name),
           *                 _NclBasicDataTypeToName(attnode->type));
           *fprintf(stderr, "\tNeed to read compound att.\n\n");
           */

            nmembs=H5Tget_nmembers(p_type);

          /*
           *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tfound compound data, nmembs = %d\n", nmembs);
           */

            comprec = _NclFileCompoundAlloc((int) nmembs);

            comprec->n_comps = nmembs;
            comprec->type = type;
            comprec->size = H5Tget_size(p_type);
            comprec->name = NrmStringToQuark("Compound");

            comprec->value = (void *) NclMalloc(nelmts * comprec->size);
            assert(comprec->value);

            status = H5Aread(attr_id, p_type, comprec->value);
          /*
           *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tcompound data nelmts = %d, need = %d\n", nelmts, need);
           *fprintf(stderr, "\tcompound data size = %d\n", comprec->size);
           */

            for(i = 0; i < nmembs; ++i)
            {
                compnode = &(comprec->compnode[i]);
              /*Name and offset*/
                mname = H5Tget_member_name(p_type, i);
                compnode->name = NrmStringToQuark(mname);
                compnode->offset = (size_t) H5Tget_member_offset(p_type, i);
    
              /*Member's type*/
                subtype = H5Tget_member_type(p_type, i);
                typename = _getH5typeName(subtype, i+4);
                compnode->type = string2NclType(typename);

              /*
               *fprintf(stderr, "\tcomponent no %d name: <%s>, offset = %d, type: <%s>\n",
               *                   i, mname, compnode->offset, typename);
               */

                H5Tclose(subtype);
                NclFree(typename);
                NclFree(mname);
            }

            attnode->n_elem = nelmts;
            attnode->value = (void *) comprec;
            attnode->is_compound = 1;

          /*
           *H5Tclose(p_type);
           *H5Tclose(type);
           *H5Sclose(space);
           *H5Dclose(attr_id);
           *status = H5Dvlen_reclaim(type, space, H5P_DEFAULT, comprec->value);
           *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
           */
        }
        else if(NCL_vlen == attnode->type)
        {
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tatt name: <%s>, att type: <%s>\n",
           *                 NrmQuarkToString(attnode->name),
           *                 _NclBasicDataTypeToName(attnode->type));
           *fprintf(stderr, "\tNeed to read vlen att.\n\n");
           */
        }
        else
        {
            attnode->n_elem = nelmts;
            attnode->value = NclMalloc(need);
            assert(attnode->value);

            status = H5Aread(attr_id, p_type, attnode->value);
        }

        free(type_name);

        H5Tclose(p_type);
    }

    H5Sclose(space);
    H5Tclose(type);

  /*
   *fprintf(stderr, "Leaving _checkH5attribute, at file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return SUCCEED;
}

/*
 ***********************************************************************
 * Function:    _addH5address
 *
 * Purpose:     Recursively check a group and add a hardlink address to visited data structure
 *
 * Return:      void
 *
 * Programmer:  Wei Huang
 * Created:     July 1, 2009
 *
 ***********************************************************************
 */

static void _addH5address(H5_addr_t *visited, haddr_t addr, char *path)
{
    hsize_t idx;         /* Index of address to use */

  /*Allocate space if necessary*/
    if(visited->nused == visited->nalloc)
    {
        visited->nalloc *= 2;

        visited->objs = realloc(visited->objs, visited->nalloc * sizeof(visited->objs[0]));
    }

  /*Append it*/
    idx = visited->nused;
    visited->objs[idx].addr = addr;
    visited->objs[idx].path = NrmStringToQuark(path);
    ++visited->nused;

  /*
   *fprintf(stderr, "\nIn _addH5address, at file: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tadd address[%d]: <%s>\n", idx, visited->objs[idx].path);
   */
}

/*
 ***********************************************************************
 * Function:    _visitedH5address
 *
 * Purpose:     Recursively check a groupdd a hardlink address to visited data structure
 *
 * Return:      void
 *
 * Programmer:  Wei Huang
 * Created:     April 10, 2012
 *
 ***********************************************************************
 */

char *_visitedH5address(H5_addr_t *visited, haddr_t addr)
{
    hsize_t u;           /* Local index variable */

    if(NULL == visited)
        return(NULL);

    /* Look for address */
    for(u = 0; u < visited->nused; u++)
    {
        /* Check for address already in array */
        if(visited->objs[u].addr == addr)
            return NrmQuarkToString(visited->objs[u].path);
    }

    /* Didn't find address */
    return(NULL);
}

/*
 ***********************************************************************
 * Function:	_searchH5byName
 *
 * Purpose:	Iterator callback for traversing objects in file
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 * Created:	April 10, 2012
 *
 ***********************************************************************
 */

herr_t _searchH5byName(hid_t loc_id, char *path, H5L_info_t *linfo)
{
    char *new_name = NULL;
    char *full_name = NULL;
    char *visited_address = NULL; /* Whether the link/object was already visited */

  /*
   *fprintf(stderr, "\nEntering _searchH5byName, in file: %s, at line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tid: %d, path: <%s>\n", loc_id, path);
   */

    /* Create the full path name for the link */
    if(tudata.is_absolute)
    {
        hsize_t base_len = strlen(tudata.base_grp_name);
        hsize_t add_slash = base_len ? ((tudata.base_grp_name)[base_len-1] != '/') : 1;
            
        if(NULL == (new_name = NclMalloc(base_len + add_slash + strlen(path) + 1)))
            return(H5_ITER_ERROR);
        strcpy(new_name, tudata.base_grp_name);
        if (add_slash)
            new_name[base_len] = '/';
        strcpy(new_name + base_len + add_slash, path);
        full_name = strdup(new_name);
    }
    else
        full_name = strdup(path);

    /* Perform the correct action for different types of links */
    if(linfo->type == H5L_TYPE_HARD)
    {
        H5O_info_t oinfo;

        /* Get information about the object */
        if(H5Oget_info_by_name(loc_id, path, &oinfo, H5P_DEFAULT) < 0)
        {
            if(new_name)
                free(new_name);
                if(full_name)
                    free(full_name);
            return(H5_ITER_ERROR);
        }

        /* If the object has multiple links, add it to the list of addresses
         *  already visited, if it isn't there already
         */
        if(oinfo.rc > 0)
        {
            visited_address = _visitedH5address(tudata.seen, oinfo.addr);
            if(NULL == visited_address)
                _addH5address(tudata.seen, oinfo.addr, full_name);
        }

        /* Make 'visit object' callback */
        if(tudata.searcher->_searchH5obj)
        {
            if((*tudata.searcher->_searchH5obj)(full_name, &oinfo, tudata.searcher->udata, visited_address) < 0)
            {
                if(new_name)
                    free(new_name);
                if(full_name)
                    free(full_name);
                return(H5_ITER_ERROR);
            }
        }
    }
    else
    {
        /* Make 'visit link' callback */
        if(tudata.searcher->_searchH5link)
        {
            if((*tudata.searcher->_searchH5link)(full_name, linfo, tudata.searcher->udata) < 0)
            {
                if(new_name)
                    free(new_name);
                if(full_name)
                    free(full_name);
                return(H5_ITER_ERROR);
            }
        }
    }

    if(new_name)
        free(new_name);
    if(full_name)
        free(full_name);

  /*
   *fprintf(stderr, "Leaving _searchH5byName, in file: %s, at line: %d\n\n", __FILE__, __LINE__);
   */

    return(H5_ITER_CONT);
}

/*
 *************************************************************************
 * Function:    h5_group_name_struct_t _get_parent_group_name(char *name)
 *
 * Purpose:     find parent group name, based on (self)name.
 *
 * Return:      name of the parent group.
 *
 * Programmer:  Wei Huang
 *              April 10, 2012
 *
 *************************************************************************
 */

h5_group_name_struct_t _get_parent_group_name(char *name)
{
    h5_group_name_struct_t h5grplvl;
    char fullname[MAX_NCL_NAME_LENGTH];
    char *cpntr;
    int pos;

  /*
   *fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tname: <%s>\n", name);
   */

    if(name[0] == '/')
    {
        strcpy(fullname, name);
    }
    else
    {
        strcpy(fullname, "/");
        strcat(fullname, name);
    }

    h5grplvl.size = 0;

    while(1 < strlen(fullname))
    {
        cpntr = rindex(fullname, '/');
        strcpy(h5grplvl.short_name[h5grplvl.size], cpntr+1);

        pos = strlen(fullname) - strlen(cpntr);
        fullname[pos+1] = '\0';
        strcpy(h5grplvl.parent_name[h5grplvl.size], fullname);
        fullname[pos] = '\0';

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\th5grplvl.size = %d\n", h5grplvl.size);
       *fprintf(stderr, "\th5grplvl.parent_name: <%s>\n", h5grplvl.parent_name[h5grplvl.size]);
       *fprintf(stderr, "\th5grplvl.short_name: <%s>\n", h5grplvl.short_name[h5grplvl.size]);
       *fprintf(stderr, "\tfullname: <%s>\n\n", fullname);
       */

        ++h5grplvl.size;
    }

    return h5grplvl;
}

/*
 ***********************************************************************
 * Function:    _getGrpNodeByName
 *
 * Purpose:     Get group node by name.
 *
 * Return:      NclFileGrpNode *.
 *
 * Programmer:  Wei Huang
 * Created:     April 11, 2012
 *
 ***********************************************************************
 */

NclFileGrpNode *_getGrpNodeByName(NclFileGrpNode *rootgrp, NclQuark gn)
{
    NclFileGrpNode *curgrpnode;
    NclFileGrpNode *thegrpnode;
    int n;

    if((gn == rootgrp->name) || (gn == rootgrp->real_name))
        return rootgrp;

    if(NULL ==  rootgrp->grp_rec)
        return NULL;

    for(n = 0; n < rootgrp->grp_rec->n_grps; ++n)
    {
        curgrpnode = rootgrp->grp_rec->grp_node[n];
        if((gn == rootgrp->name) || (gn == rootgrp->real_name))
        {
            return curgrpnode;
        }

        thegrpnode = _getGrpNodeByName(curgrpnode, gn);
        if(NULL != thegrpnode)
        {
            return thegrpnode ;
        }
    }

    return NULL;
}

/*
 *************************************************************************
 * Function:    _addGroup(NclFileGrpNode **rootgrp, char *name)
 *
 * Purpose:     add name to rootgrp
 *
 * Return:      the newly added groupnode.
 *
 * Programmer:  Wei Huang
 *      April 11, 2012
 *
 *************************************************************************
 */

NclFileGrpNode *_addGroup(NclFileGrpNode **rootgrp, char *name)
{
    NclFileGrpNode *grpnode = *rootgrp;
    NclFileGrpNode *parentgrpnode = NULL;
    NclFileGrpNode *curgrpnode = NULL;
    char longname[MAX_NCL_NAME_LENGTH];

    NrmQuark qsn = -1;
    NrmQuark qpn = -1;

    int ngrp = 0;

  /*
   *int n = h5grplvl.size;
   */

    h5_group_name_struct_t h5grplvl = _get_parent_group_name(name);

    qsn = _string2quark(h5grplvl.short_name[0]);
    qpn = NrmStringToQuark(h5grplvl.parent_name[0]);

  /*
   *fprintf(stderr, "\nin file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tname: <%s>\n", name);
   *fprintf(stderr, "\tgrpnode->name: <%s>\n", NrmQuarkToString(grpnode->name));
   *fprintf(stderr, "\tgrpnode->real_name: <%s>\n", NrmQuarkToString(grpnode->real_name));
   *fprintf(stderr, "\tparent_group_name: <%s>\n", h5grplvl.parent_name[0]);
   *fprintf(stderr, "\tgroup_name: <%s>\n", h5grplvl.short_name[0]);
   */

    if(qpn == grpnode->real_name)
    {
        if(NULL == grpnode->grp_rec)
        {
            grpnode->grp_rec = _NclFileGrpAlloc(2);
            grpnode->grp_rec->n_grps = 0;
            ngrp = 0;
        }   
        else
        {
            ngrp = grpnode->grp_rec->n_grps;
        }   

        if(grpnode->grp_rec->n_grps >= grpnode->grp_rec->max_grps)
        {
            _NclFileGrpRealloc(grpnode->grp_rec);
        }
        curgrpnode = grpnode->grp_rec->grp_node[ngrp];
        curgrpnode->name = qsn;
        curgrpnode->pname = qpn;
        strcpy(longname, name);
        strcat(longname, "/");
        curgrpnode->real_name = NrmStringToQuark(longname);
        ++grpnode->grp_rec->n_grps;
        return curgrpnode;
    }

    parentgrpnode = _getGrpNodeByName(grpnode, qpn);
    if(NULL == parentgrpnode)
        parentgrpnode = _addGroup(rootgrp, h5grplvl.parent_name[0]);

    return _addGroup(&parentgrpnode, name);
}

NclBasicDataTypes string2NclType(char* name)
{
    if(0 == strcmp("char", name))
        return(NCL_char);
    else if(0 == strcmp("byte", name))
        return(NCL_byte);
    else if(0 == strcmp("ubyte", name))
        return(NCL_ubyte);
    else if(0 == strcmp("short", name))
        return(NCL_short);
    else if(0 == strcmp("ushort", name))
        return(NCL_ushort);
    else if(0 == strcmp("integer", name))
        return(NCL_int);
    else if(0 == strcmp("int", name))
        return(NCL_int);
    else if(0 == strcmp("uint", name))
        return(NCL_uint);
    else if(0 == strcmp("long", name))
            return(NCL_long);
    else if(0 == strcmp("ulong", name))
            return(NCL_ulong);
    else if(0 == strcmp("int64", name))
        return(NCL_int64);
    else if(0 == strcmp("uint64", name))
        return(NCL_uint64);
    else if(0 == strcmp("float", name))
        return(NCL_float);
    else if(0 == strcmp("double", name))
        return(NCL_double);
    else if(0 == strcmp("string", name))
        return(NCL_string);
    else if(0 == strcmp("list", name))
        return(NCL_list);
    else if(0 == strcmp("vlen", name))
        return(NCL_vlen);
    else if(0 == strcmp("compound", name))
        return(NCL_compound);
    else if(0 == strcmp("opaque", name))
        return(NCL_opaque);
    else if(0 == strcmp("enum", name))
        return(NCL_enum);
    else if(0 == strcmp("object reference", name))
        return(NCL_reference);
    else if(0 == strcmp("dataset region reference", name))
        return(NCL_reference);
    else
    {
      /*
       */
        fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tUnknown type: <%s>\n", name);
        return(NCL_none);
    }
}

static hid_t _get_native_type(hid_t type)
{
    hid_t p_type;
    H5T_class_t type_class = H5Tget_class(type);

    if(H5T_BITFIELD == type_class)
        p_type=H5Tcopy(type);
    else
        p_type = H5Tget_native_type(type,H5T_DIR_DEFAULT);

    return (p_type);
}

static void _block2string(char* str, hid_t region)
{
    hssize_t nblocks;
    hsize_t  alloc_size;
    hsize_t *ptdata;
    int      ndims = H5Sget_simple_extent_ndims(region);
    int      i, j;
    char     buffer[1024];

    nblocks = H5Sget_select_hyper_nblocks(region);

  /*
   *fprintf(stderr, "\nfile: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tnblocks = %ld, ndims = %d\n", (long)nblocks, ndims);
   */

  /*Print block information*/
    if(0 >= nblocks)
        return;

    alloc_size = nblocks * ndims * 2 * sizeof(ptdata[0]);
    assert(alloc_size == (hsize_t) ((size_t) alloc_size));
    ptdata = (hsize_t *)malloc((size_t) alloc_size);

    H5Sget_select_hyper_blocklist(region, (hsize_t)0, (hsize_t)nblocks, ptdata);

    for(i = 0; i < nblocks; ++i)
    {
        if(i)
        {
            sprintf(buffer, "%lu, ", (unsigned long)i);
            strcat(str, buffer);
        }
        else
            strcat(str, " ");

      /*Start coordinates and opposite corner*/
        for(j = 0; j < ndims; ++j)
        {
            if(j)
                sprintf(buffer, ", %ld", (long)ptdata[i * 2 * ndims + j]);
            else
                sprintf(buffer, "(%ld", (long)ptdata[i * 2 * ndims + j]);

            strcat(str, buffer);
        }

        strcat(str, ") ");

        for(j = 0; j < ndims; ++j)
        {
            if(j)
                sprintf(buffer, ", %ld", (long)ptdata[i * 2 * ndims + j + ndims]);
            else
                sprintf(buffer, "(%ld", (long)ptdata[i * 2 * ndims + j + ndims]);

            strcat(str, buffer);
        }

        strcat(str, ")");
    }

    free(ptdata);
}

static void _point2string(char* str, hid_t region)
{
    hssize_t   npoints;
    hsize_t    alloc_size;
    hsize_t   *ptdata;
    int        ndims = H5Sget_simple_extent_ndims(region);
    int i;
    int j;
    char      buffer[1024];

    npoints = H5Sget_select_elem_npoints(region);

  /*
   */
    fprintf(stderr, "\nfile: %s, line: %d\n\n", __FILE__, __LINE__);
    fprintf(stderr, "\tnpoints = %ld, ndims = %d\n", (long)npoints, ndims);

    /* Print point information */
    if(0 < npoints)
    {
        alloc_size = npoints * ndims * sizeof(ptdata[0]);
        assert(alloc_size == (hsize_t) ((size_t) alloc_size));
        ptdata = (hsize_t *)malloc((size_t) alloc_size);
        H5Sget_select_elem_pointlist(region, (hsize_t)0, (hsize_t)npoints, ptdata);

        for(i = 0; i < npoints; ++i)
        {
            sprintf(buffer, " %lu ", (unsigned long)i);
            strcat(str, buffer);

            for(j = 0; j < ndims; j++)
            {
                if(j)
                    sprintf(buffer, ", %ld", (long)(ptdata[i * ndims + j]));
                else
                    sprintf(buffer, "(%ld", (long)(ptdata[i * ndims + j]));
                strcat(str, buffer);
            }

            strcat(str, ")");
        }

        free(ptdata);
    }
}

static void _region2string(char* str, hid_t container, void *vp)
{
    hid_t   obj, region;
    char    ref_name[1024];
    H5S_sel_type region_type;

    obj = H5Rdereference(container, H5R_DATASET_REGION, vp);

    if(0 <= obj)
    {
        region = H5Rget_region(container, H5R_DATASET_REGION, vp);
        if(0 <= region)
        {
            H5Rget_name(obj, H5R_DATASET_REGION, vp, (char*) ref_name, 1024);

            strcpy(str, ref_name);

            region_type = H5Sget_select_type(region);
            if(H5S_SEL_POINTS == region_type)
                _point2string(str, region);
            else
                _block2string(str, region);

            H5Sclose(region);
        }
        H5Dclose(obj);
    }
}

const char* lookup_ref_path(haddr_t ref)
{
#if 0
    ref_path_node_t *node;

  /*Create ref path table, if it hasn't already been created*/
    if(ref_path_table == NULL)
        init_ref_path_table();

    node = H5SL_search(ref_path_table, &ref);

    return(node ? node->path : NULL);
#else
    return NULL;
#endif
}

static void _mem2string(char* buffer, hid_t container, hid_t type, void *vp)
{
    size_t         nsize;
    hid_t          obj;
    H5T_class_t    type_class;

  /*
   *fprintf(stderr, "\nEnter _mem2string. file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    nsize = H5Tget_size(type);

    type_class = H5Tget_class(type);

    if(0 > type_class)
        return;

    switch (type_class)
    {
        case H5T_REFERENCE:
             if(nsize == H5R_DSET_REG_REF_BUF_SIZE)
             {
               /*
                *fprintf(stderr, "\tnsize = %ld, H5R_DSET_REG_REF_BUF_SIZE = %ld\n",
                *                   nsize, H5R_DSET_REG_REF_BUF_SIZE);
                */
 
                 _region2string(buffer, container, vp);

               /*
                *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
                *fprintf(stderr, "\tbuffer: <%s>\n", buffer);
                */
             }
             else if (nsize == H5R_OBJ_REF_BUF_SIZE)
             {
                 H5O_info_t oi;

               /*
                *fprintf(stderr, "\tnsize = %ld, H5R_OBJ_REF_BUF_SIZE = %ld\n",
                *                   nsize, H5R_OBJ_REF_BUF_SIZE);
                *Object references
                *    -- show the type and OID of the referenced
                */

                 obj = H5Rdereference(container, H5R_OBJECT, vp);

                 H5Oget_info(obj, &oi);

#if 0
               /*Print object type and close object*/
                 switch (oi.type)
                 {
                     case H5O_TYPE_GROUP:
                        /*h5tools_str_append(str, H5_TOOLS_GROUP);*/
                          fprintf(stderr, "\ttype: H5O_TYPE_GROUP\n");
                          break;

                     case H5O_TYPE_DATASET:
                        /*h5tools_str_append(str, H5_TOOLS_DATASET);*/
                          fprintf(stderr, "\ttype: H5O_TYPE_DATASET\n");
                          break;
                     case H5O_TYPE_NAMED_DATATYPE:
                        /*h5tools_str_append(str, H5_TOOLS_DATATYPE);*/
                          fprintf(stderr, "\ttype: H5O_TYPE_NAMED_DATATYPE\n");
                          break;
                     default:
                        /*h5tools_str_append(str, "%u-", (unsigned) oi.type);*/
                          fprintf(stderr, "\ttype: %u-\n", (unsigned) oi.type);
                          break;
                 }
#endif

                 H5Oclose(obj);

                 strcpy(buffer, _visitedH5address(tudata.seen, oi.addr));

               /*
                *fprintf(stderr, "\tfileno: %ld, addr: <%ld>\n", (long)oi.fileno, (long)(oi.addr));
                *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
                *fprintf(stderr, "\tbuffer: <%s>\n", buffer);
                */
             }
             else
                 fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);

             break;
        default:
             fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
             fprintf(stderr, "\tShould not get here.\n");
             break;
    }

  /*
   *fprintf(stderr, "Leave _mem2string. file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}


static int _mem2quark(hid_t container, hsize_t nelmts,
                      hid_t type, void *_mem, NrmQuark* refquarks)
{
    int            ret_value = 0; /*no need to LEAVE() on ERROR: HERR_INIT(int, SUCCEED) */
    unsigned char *mem = (unsigned char*) _mem;
    hsize_t        i;         /*element counter  */
    size_t         size;      /*size of each datum  */
    char           buffer[4096];

    size = H5Tget_size(type);

    for (i = 0; i < nelmts; ++i)
    {
        void* memref = mem + i * size;

        _mem2string(buffer, container, type, memref);

        refquarks[i] = NrmStringToQuark(buffer);

      /*
       *fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tbuffer: <%s>\n", buffer);
       *fprintf(stderr, "\trefquarks[%ld]: <%s>\n", (long)i, NrmQuarkToString(refquarks[i]));
       */
    }

    return ret_value;
}

#ifndef NELMTS
#define NELMTS(X)    (sizeof(X)/sizeof(X[0]))
#endif

#ifndef H5TOOLS_BUFSIZE
#define H5TOOLS_BUFSIZE	(32 * 1024 * 1024)
#endif

NrmQuark* _get_refquarks(hid_t dset, hid_t p_type, int *nref)
{
    herr_t              ret_value = SUCCEED;
    hid_t               f_space = -1;                  /* file data space */
    hsize_t             elmtno;                   /* counter  */
    size_t              i;                        /* counter  */
    int                 carry;                    /* counter carry value */
    hsize_t             zero[8];                  /* vector of zeros */
    hsize_t             total_size[H5S_MAX_RANK]; /* total size of dataset*/

    /* Print info */
    size_t              p_type_nbytes;            /* size of memory type */
    hsize_t             p_nelmts;                 /* total selected elmts */

    /* Stripmine info */
    hsize_t             sm_size[H5S_MAX_RANK];    /* stripmine size */
    hsize_t             sm_nbytes;                /* bytes per stripmine */
    hsize_t             sm_nelmts;                /* elements per stripmine*/
    unsigned char      *sm_buf = NULL;            /* buffer for raw data */
    hid_t               sm_space = -1;                 /* stripmine data space */

    /* Hyperslab info */
    hsize_t             hs_offset[H5S_MAX_RANK];  /* starting offset */
    hsize_t             hs_size[H5S_MAX_RANK];    /* size this pass */
    hsize_t             hs_nelmts;                /* elements in request */
    hsize_t             ndims = -1;
    hsize_t             p_min_idx[H5S_MAX_RANK];
    hsize_t             p_max_idx[H5S_MAX_RANK];

    NrmQuark*           refquarks = NULL;

    f_space = H5Dget_space(dset);

    if (f_space == FAILED)
    {
        fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tH5Dget_space failed.\n");
        return NULL;
    }


    ndims = H5Sget_simple_extent_ndims(f_space);

    if(ndims > NELMTS(sm_size))
    {
        fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tH5H5Sget_simple_extent_ndims failed.\n");
        return NULL;
    }

    /* Assume entire data space to be printed */
    if(ndims > 0)
    {
        for(i = 0; i < (size_t)ndims; i++)
            p_min_idx[i] = 0;
    }

    H5Sget_simple_extent_dims(f_space, total_size, NULL);

    p_nelmts = 1;

    if(0 < ndims)
    {
        for (i = 0; i < ndims; i++)
            p_nelmts *= total_size[i];
    }

    if(0 == p_nelmts)
        return NULL;

  /*
   *Determine the strip mine size and allocate a buffer.
   *The strip mine is a hyperslab whose size is manageable.
   */
    sm_nbytes = p_type_nbytes = H5Tget_size(p_type);

    if(ndims > 0)
    {
        for (i = ndims; i > 0; --i)
        {
            hsize_t size = H5TOOLS_BUFSIZE / sm_nbytes;
            if ( size == 0) /* datum size > H5TOOLS_BUFSIZE */
                size = 1;
            sm_size[i - 1] = MIN(total_size[i - 1], size);
            sm_nbytes *= sm_size[i - 1];
            assert(sm_nbytes > 0);
        }
    }

    if(!sm_nbytes)
        return NULL;

    refquarks = (NrmQuark*) NclMalloc(p_nelmts * sizeof(NrmQuark));

    assert(sm_nbytes == (hsize_t)((size_t)sm_nbytes));
    sm_buf = (unsigned char *)malloc((size_t)sm_nbytes);

    sm_nelmts = sm_nbytes / p_type_nbytes;
    sm_space = H5Screate_simple(1, &sm_nelmts, NULL);

  /*The stripmine loop*/
    memset(hs_offset, 0, sizeof hs_offset);
    memset(zero, 0, sizeof zero);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tp_nelmts = %ld, hs_nelmts = %ld\n", (long)p_nelmts, (long)hs_nelmts);
   */

    for(elmtno = 0; elmtno < p_nelmts; elmtno += hs_nelmts)
    {
      /*Calculate the hyperslab size*/
        if(ndims > 0)
        {
            for(i = 0, hs_nelmts = 1; i < ndims; ++i)
            {
                hs_size[i] = MIN(total_size[i] - hs_offset[i], sm_size[i]);
                p_max_idx[i] = p_min_idx[i] + hs_size[i];
                hs_nelmts *= hs_size[i];

              /*
               *fprintf(stderr, "\ti = %ld, hs_nelmts = %ld, hs_size[i] = %ld, p_max_idx[i] = %ld\n",
               *                (long)i, (long)hs_nelmts, (long)hs_size[i], (long)p_max_idx[i]);
               */
            }

            H5Sselect_hyperslab(f_space, H5S_SELECT_SET, hs_offset, NULL, hs_size, NULL);
            H5Sselect_hyperslab(sm_space, H5S_SELECT_SET, zero, NULL, &hs_nelmts, NULL);
        }
        else
        {
            H5Sselect_all(f_space);
            H5Sselect_all(sm_space);
            hs_nelmts = 1;
        }

      /*Read the data*/
        ret_value = H5Dread(dset, p_type, sm_space, f_space, H5P_DEFAULT, sm_buf);
        if(0 > ret_value)
        {
            fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\tH5Dread failed.\n");
            return refquarks;
        }

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tp_nelmts = %ld, hs_nelmts = %ld\n", (long)p_nelmts, (long)hs_nelmts);
       */

        ret_value = _mem2quark(dset, hs_nelmts, p_type, sm_buf, refquarks);

        if(0 > ret_value)
        {
            fprintf(stderr, "\nfile: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\t_mem2quark failed.\n");
            return refquarks;
        }

        /* Calculate the next hyperslab offset */
        for(i = ndims, carry = 1; i > 0 && carry; --i)
        {
            p_min_idx[i - 1] = p_max_idx[i - 1];
            hs_offset[i - 1] += hs_size[i - 1];

            if (hs_offset[i - 1] == total_size[i - 1])
                hs_offset[i - 1] = 0;
            else
                carry = 0;
        }
    }

    if(sm_buf)
        free(sm_buf);

    if(sm_space >= 0)
        H5Sclose(sm_space);
    if(f_space >= 0)
         H5Sclose(f_space);

    return refquarks;
}

/*
 ***********************************************************************
 * Function:	_readH5dataInfo(hid_t dset, char *name, NclFileVarNode **node)
 *
 * Purpose:	find information about this dataset.
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 * Created:	April 12, 2012
 *
 ***********************************************************************
 */

herr_t _readH5dataInfo(hid_t dset, char *name, NclFileVarNode **node)
{
    hsize_t     cur_size[H5S_MAX_RANK];   /* current dataset dimensions */
    hsize_t     max_size[H5S_MAX_RANK];   /* maximum dataset dimensions */
    hid_t       space;          /* data space                 */
    hid_t       type;           /* data type                  */
    int         ndims;          /* dimensionality             */
  /*H5S_class_t space_type;*/   /* type of dataspace          */
    int   i;

    char *typename;
    NclFileVarNode *varnode = *node;

    hsize_t     var_size = 1;

  /*
   *fprintf(stderr, "\nEntering _readH5dataInfo, at file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tid: %d, name: <%s>\n", dset, name);
   */

    space = H5Dget_space(dset);
    ndims = H5Sget_simple_extent_dims(space, cur_size, max_size);

    if(ndims > 0)
    {
        varnode->dim_rec = _NclFileDimAlloc(ndims);
        varnode->dim_rec->n_dims = ndims;
    }
    else
    {
        varnode->dim_rec = _NclFileDimAlloc(1);
        varnode->dim_rec->n_dims = 0;
    }

    varnode->dim_rec->gid = dset;

  /*
   *fprintf(stderr, "\tat file: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tndims: %d\n", ndims);
   */

    for(i = 0; i < ndims; ++i)
    {
      /*
       *fprintf(stderr, "\tdim[%d] = %d\n", i, cur_size[i]);
       */
        varnode->dim_rec->dim_node[i].id = i;
        varnode->dim_rec->dim_node[i].size = cur_size[i];
        varnode->dim_rec->dim_node[i].name = -1;
        varnode->dim_rec->dim_node[i].description = -1;
        varnode->dim_rec->dim_node[i].dataset_name = -1;
        varnode->dim_rec->dim_node[i].is_unlimited = 0;
        varnode->dim_rec->dim_node[i].is_dataset = 0;

        var_size *= cur_size[i];
    }

    type = H5Dget_type(dset);

  /*Data type name*/
    typename = _getH5typeName(type, 15);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\ttype name: <%s>\n", typename);
   */

    varnode->type = string2NclType(typename);
    free(typename);

    if(NCL_compound == varnode->type)
    {
        char    *mname=NULL;     /* member name */
        hid_t    subtype;        /* member data type */
        unsigned nmembs;         /* number of members */
        unsigned i;              /* miscellaneous counters */

        NclFileCompoundRecord *comprec;
        NclFileCompoundNode   *compnode;

        NrmQuark *componentnames;
        NrmQuark compatt = NrmStringToQuark("component_names");

        nmembs=H5Tget_nmembers(type);

        componentnames = (NrmQuark*)NclMalloc(nmembs * sizeof(NrmQuark));

      /*
       *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tid: %d, name: <%s>\n", dset, name);
       *fprintf(stderr, "\tfound compound data, nmembs = %d\n", nmembs);
       */

        comprec = _NclFileCompoundAlloc((int) nmembs);

        comprec->n_comps = nmembs;
        comprec->type = type;
        comprec->size = H5Tget_size(type);
        comprec->name = NrmStringToQuark(name);

        i = nmembs;
        while(0 < i)
        {
            --i;
            compnode = &(comprec->compnode[i]);
          /*Name and offset*/
            mname = H5Tget_member_name(type, i);
            compnode->name = NrmStringToQuark(mname);
            compnode->offset = (unsigned long) H5Tget_member_offset(type, i);
            free(mname);

            componentnames[i] = compnode->name;

          /*Member's type*/
            subtype = H5Tget_member_type(type, i);
            typename = _getH5typeName(subtype, i+4);
            compnode->type = string2NclType(typename);
            if(0 == strcmp("string", typename))
                compnode->nvals = (int) H5Tget_size(subtype);
            else
                compnode->nvals = (int) (H5Tget_size(subtype) / _NclSizeOf(compnode->type));
            compnode->rank = i;
            compnode->sides = NULL;
            compnode->value = NULL;
            free(typename);
            H5Tclose(subtype);

          /*
           *fprintf(stderr, "\tcomponent no %d name: <%s>, offset = %d, type: <%s>, nvals = %d\n",
           *                   i, name, compnode->offset, typename, compnode->nvals);
           */
        }

        varnode->comprec = comprec;
        varnode->is_compound = 1;

        _addNclAttNode(&(varnode->att_rec), compatt, NCL_string, nmembs, (void*)componentnames);
        NclFree(componentnames);
    }
    else if(NCL_enum == varnode->type)
    {
        char         *membname=NULL;    /* member names */
        unsigned char *membvalue=NULL;  /* value array */
        void          *values = NULL;
        unsigned    nmembs;         /* number of members */
        unsigned    n;              /* miscellaneous counters */
        size_t      size;
        NclFileEnumRecord *enumrec;

        nmembs = H5Tget_nmembers(type);
        assert(nmembs>0);

      /*
       *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tid: %d, name: <%s>\n", dset, name);
       *fprintf(stderr, "\tfound enum data, nmembs = %d\n", nmembs);
       */

      /*Let add two more attributes to varnode here.
       *1. an array of the enum names
       *2. an array of the enum value
       */

        enumrec = _NclFileEnumAlloc(nmembs);
        enumrec->n_enums = nmembs;
        enumrec->name = NrmStringToQuark(name);
        enumrec->size = nmembs;

        size = H5Tget_size(type);

        switch(size)
        {
            case 1:
                 enumrec->type = NCL_ubyte;
                 break;
            case 2:
                 enumrec->type = NCL_ushort;
                 break;
            case 4:
                 enumrec->type = NCL_uint;
                 break;
            default:
                 enumrec->type = NCL_uint64;
                 break;
        }

        membname = NclCalloc(256, sizeof(char));
        membvalue = NclCalloc(1, size);

        size = sizeof(NclQuark);
        values = (void *)NclCalloc(enumrec->size, size);
        assert(values);

        for(n=0; n<nmembs; ++n)
        {
            membname = H5Tget_member_name(type, n);
            enumrec->enum_node[n].name = NrmStringToQuark(membname);

            memcpy(values + n * size, &(enumrec->enum_node[n].name), size);

          /*
           *fprintf(stderr, "\tmember %d, name: <%s>\n", n, membname);
           */
        }

        _addNclAttNode(&(varnode->att_rec), NrmStringToQuark("enum_name"),
                       NCL_string, enumrec->size, values);

        NclFree(values);

        values = (void *)NclCalloc(enumrec->size, _NclSizeOf(enumrec->type));
        assert(values);

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tenumrec->name: <%s>, enumrec->type = %d, enumrec->size = %ld\n",
       *                 NrmQuarkToString(enumrec->name), enumrec->type, enumrec->size);
       */

        switch(enumrec->type)
        {
            case NCL_char:
            case NCL_ubyte:
                 {
                 unsigned char *iptr = (unsigned char *) membvalue;
                 for(n = 0; n < enumrec->size; n++)
                 {
                     H5Tget_member_value(type, n, membvalue);
                     enumrec->enum_node[n].value = iptr[0];
                     memcpy(values + n, membvalue, 1);
                 }
                 }
                 break;
            case NCL_short:
            case NCL_ushort:
                 {
                 unsigned short *iptr = (unsigned short *) membvalue;
                 for(n = 0; n < enumrec->size; n++)
                 {
                     H5Tget_member_value(type, n, membvalue);
                     enumrec->enum_node[n].value = iptr[0];
                     memcpy(values + 2 * n, membvalue, 2);
                 }
                 }
                 break;
            case NCL_int:
            case NCL_uint:
                 {
                 unsigned int *iptr = (unsigned int *) membvalue;
                 for(n = 0; n < enumrec->size; n++)
                 {
                     H5Tget_member_value(type, n, membvalue);
                     enumrec->enum_node[n].value = iptr[0];
                     memcpy(values + 4 * n, membvalue, 4);
                 }
                 }
                 break;
            default:
                 {
                 uint64 *iptr = (uint64 *) membvalue;
                 for(n = 0; n < enumrec->size; n++)
                 {
                     H5Tget_member_value(type, n, membvalue);
                     enumrec->enum_node[n].value = iptr[0];
                     memcpy(values + 8 * n, membvalue, 8);
                 }
                 }
        }
    
        _addNclAttNode(&(varnode->att_rec), NrmStringToQuark("enum_value"),
                       enumrec->type, enumrec->size, values);

        free(membname);
        free(membvalue);

        NclFree(values);

        varnode->udt = (void *) enumrec;
    }

    H5Sclose (space);

  /*
   *fprintf(stderr, "Leaving _readH5dataInfo, at file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return SUCCEED;
}

static herr_t _get_status(hid_t id, hid_t obj_id, H5O_type_t obj_type, const char *name, const char* flnm, int ln)
{
    herr_t status = SUCCEED;

    if((obj_type >= 0) && (obj_id < 0))
    {
        fprintf(stderr, "\n\n\n");
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "FAILED in file: %s, line: %d\n", flnm, ln);
        fprintf(stderr, "\tname: <%s>\n", name);
        fprintf(stderr, "\tid: %d\n", id);
        fprintf(stderr, "\tobj_type: %d\n", obj_type);
        fprintf(stderr, "\tobj_id: %d\n", obj_id);
        fprintf(stderr, "FAILED in file: %s, line: %d\n", flnm, ln);
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "\n\n\n");

        status =  FAILED;
    }

    return status;
}
/*
 *************************************************************************
 * Function:	_searchH5obj
 *
 * Purpose:	Search information about an object
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 *              July 1, 2009
 *
 *************************************************************************
 */

herr_t _searchH5obj(char *name, H5O_info_t *oinfo,
                    void *_grppntr, char *already_seen)
{
    NclFileGrpNode **rootgrp = (NclFileGrpNode **) _grppntr;
    NclFileGrpNode *grpnode = *rootgrp;
    NclFileGrpNode *curgrpnode = NULL;

    H5O_type_t obj_type = oinfo->type;          /* Type of the object */
    hid_t obj_id = -1;                          /* ID of object opened */
    hid_t id = grpnode->fid;
    herr_t status = SUCCEED;
    int it_is_root = 0;
    char comment[1024];

  /*
   *fprintf(stderr, "\nEntering _NclHDF5search_obj, int file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tname = <%s>, id: %d\n", name, id);
   */

    if(already_seen)
    {
        fprintf(stderr, "\n\n\n");
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "ALREADY-SEEN in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tname: <%s>\n", name);
        fprintf(stderr, "ALREADY-SEEN in file: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "**************************************************************\n");
        fprintf(stderr, "\n\n\n");
      /*
       *fprintf(stderr, "Leaving _NclHDF5search_obj, at file: %s, line: %d\n\n", __FILE__, __LINE__);
       */
        return SUCCEED;
    }

#if 0
    /* Open the object.  Not all objects can be opened.  If this is the case
     * then return right away.
     */
    obj_id = H5Oopen(id, name, H5P_DEFAULT);
    status = _get_status(id, obj_id, obj_type, name, __FILE__, __LINE__);
    if(FAILED == status)
        return FAILED;
#endif

    if(0 == strcmp(name, "/"))
        it_is_root = 1;
    else
        it_is_root = 0;

  /*
   *fprintf(stderr, "\nin file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tname: <%s>\n", name);
   *fprintf(stderr, "\tobj_type: %d\n", obj_type);
   *fprintf(stderr, "\tH5O_TYPE_GROUP: %d\n", H5O_TYPE_GROUP);
   *fprintf(stderr, "\tH5O_TYPE_DATASET: %d\n", H5O_TYPE_DATASET);
   */

    /* Check object information */
    switch (obj_type)
    {
        case H5O_TYPE_GROUP:
          /*
           *Open the object.
           *Not all objects can be opened.
           *If this is the case then return right away.
           */
            obj_id = H5Oopen(id, name, H5P_DEFAULT);
            status = _get_status(id, obj_id, obj_type, name, __FILE__, __LINE__);
            if(FAILED == status)
                return FAILED;

            if(it_is_root)
            {
                grpnode->format = obj_type;
                grpnode->name = NrmStringToQuark("/");
                grpnode->pname = NrmStringToQuark("-");
                grpnode->real_name = NrmStringToQuark("/");

              /*
               *fprintf(stderr, "\nin file: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tname: <%s>\n", name);
               */

                H5Aiterate(obj_id, H5_INDEX_NAME, H5_ITER_INC, NULL, _checkH5attribute, &grpnode->att_rec);
            }
            else
            {
                curgrpnode = _addGroup(&grpnode, name);
                H5Aiterate(obj_id, H5_INDEX_NAME, H5_ITER_INC, NULL, _checkH5attribute, &curgrpnode->att_rec);
            }

            grpnode->gid = -1;

          /*Close the object*/
            H5Oclose(obj_id);

            return SUCCEED;

            break;
        case H5O_TYPE_DATASET:
            {
                NclFileVarNode *curvarnode = NULL;

                NclQuark qdn, qpn;
                int nvar = 0;

                h5_group_name_struct_t h5grplvl = _get_parent_group_name(name);
            
              /*
               *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tname: <%s>\n", name);
               *fprintf(stderr, "\tobj_type: %d\n", obj_type);
               *fprintf(stderr, "\tH5O_TYPE_DATASET: %d\n", H5O_TYPE_DATASET);
               *fprintf(stderr, "\tqdn: <%s>, qpn: <%s>\n", h5grplvl.short_name[0], h5grplvl.parent_name[0]);
               */

                qdn = _string2quark(h5grplvl.short_name[0]);
                qpn = NrmStringToQuark(h5grplvl.parent_name[0]);

                if(h5grplvl.size)
                    curgrpnode = _getGrpNodeByName(grpnode, qpn);
                else
                    curgrpnode = grpnode;

                if(NULL == curgrpnode)
                {
                    curgrpnode = _addGroup(&grpnode, h5grplvl.parent_name[0]);
                }

                if(NULL == curgrpnode)
                {
                  /*
                   */
                    fprintf(stderr, "\nin file: %s, line: %d\n", __FILE__, __LINE__);
                    fprintf(stderr, "\tcan not find parent group for dataset: <%s>\n", name);
                    fprintf(stderr, "\tcan not find parent group: <%s>\n", h5grplvl.parent_name[0]);
                    exit(-1);
                }

              /*
               *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tnew dataset: <%s>\n", name);
               */

                if(NULL == curgrpnode->var_rec)
                {
                    curgrpnode->var_rec = _NclFileVarAlloc(4);
                    curgrpnode->var_rec->n_vars = 0;
                }
                else if(curgrpnode->var_rec->n_vars >= curgrpnode->var_rec->max_vars)
                {
                    _NclFileVarRealloc(curgrpnode->var_rec);
                }

                nvar = curgrpnode->var_rec->n_vars;
                curvarnode = &(curgrpnode->var_rec->var_node[nvar]);
                ++curgrpnode->var_rec->n_vars;
            
              /*
               *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tcurgrpnode->var_rec->n_vars = %d\n", curgrpnode->var_rec->n_vars);
               */
                curvarnode->gid = id;
                curvarnode->name = qdn;
                curvarnode->short_name = qdn;
                curvarnode->real_name = NrmStringToQuark(name);
                curvarnode->full_name = _string2quark(name);

              /*
               *Open the object.
               *Not all objects can be opened.
               *If this is the case then return right away.
               */
                obj_id = H5Oopen(id, name, H5P_DEFAULT);
                curvarnode->id = obj_id;

                status = _get_status(id, obj_id, obj_type, name, __FILE__, __LINE__);
                if(FAILED == status)
                    return FAILED;

                _readH5dataInfo(obj_id, name, &curvarnode);

                H5Aiterate(obj_id, H5_INDEX_NAME, H5_ITER_INC, NULL, _checkH5attribute, &curvarnode->att_rec);

                curvarnode->id = -1;
              /*Close the object*/
                H5Oclose(obj_id);
            }

            return SUCCEED;

            break;
        case H5O_TYPE_NAMED_DATATYPE:
          /*
           *Open the object.
           *Not all objects can be opened.
           *If this is the case then return right away.
           */
            obj_id = H5Oopen(id, name, H5P_DEFAULT);
            status = _get_status(id, obj_id, obj_type, name, __FILE__, __LINE__);
            if(FAILED == status)
                return FAILED;

            fprintf(stderr, "\nin file: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\tH5O_TYPE_NAMED_DATATYPE\n");
            fprintf(stderr, "\ttype obj_id   = %d\n", obj_id);
            fprintf(stderr, "\ttype obj_type = %d\n", obj_type);
#if 0
            strcpy(NclHDF5group_list->group_node->type_name, "Type");
            H5Aiterate(obj_id, H5_INDEX_NAME, H5_ITER_INC, NULL, _checkH5attribute, &curvarnode->att_rec);
#endif
            H5Oclose(obj_id);
            return FAILED;
            break;
        default:
          /*
           *Open the object.
           *Not all objects can be opened.
           *If this is the case then return right away.
           */
            obj_id = H5Oopen(id, name, H5P_DEFAULT);
            status = _get_status(id, obj_id, obj_type, name, __FILE__, __LINE__);
            if(FAILED == status)
                return FAILED;

            grpnode->real_name = NrmStringToQuark("unknown");
            grpnode->format = H5O_TYPE_UNKNOWN;

            fprintf(stderr, "obj_type: %d, grpnode->format: %d\n",
                    obj_type, grpnode->format);
            fprintf(stderr, "Unknown obj_type in _searchH5obj. return FAILED.\n");
            H5Oclose(obj_id);
            return FAILED;
    }

#if 1
    /* Object comment */
    comment[0] = '\0';
    H5Oget_comment(obj_id, comment, sizeof(comment));
    strcpy(comment + sizeof(comment) - 4, "...");
    if(comment[0])
    {
        printf("    %-10s %s\"", "Comment:", comment);
        puts("\"");
    }
#endif

  /*
   *fprintf(stderr, "Leaving _searchH5obj, at file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return SUCCEED;
}

/*
 *************************************************************************
 * Function:	_searchH5link
 *
 * Purpose:	Search information about an link
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 *              July 1, 2009
 *
 *************************************************************************
 */

herr_t _searchH5link(char *name, H5O_info_t *oinfo, void *_h5grp)
{
/*
*/
    fprintf(stderr, "\nEntering _searchH5link, name = <%s>, at file: %s, line: %d\n", name, __FILE__, __LINE__);

/*
*/
    fprintf(stderr, "Leaving _searchH5link, at file: %s, line: %d\n\n", __FILE__, __LINE__);
    return SUCCEED;
}

/*
 ***********************************************************************
 * Function:	_recursiveH5check
 *
 * Purpose:	Recursively check a group
 *
 * Return:	Success: SUCCEED
 *		Failure: FAILED
 *
 * Programmer:	Wei Huang
 * Created:	July 1, 2009
 * Modified:	April 27, 2012
 *
 ***********************************************************************
 */

static herr_t _recursiveH5check(NclFileGrpNode **rootgrp,
                               _searchH5obj_func_t  _searchH5obj,
                               _searchH5link_func_t _searchH5link)
{
    H5O_info_t  oinfo;          /* Object info for starting group */
    H5searcher_t searcher;
    char *grp_name = NrmQuarkToString((*rootgrp)->name);
    herr_t ret_status;

  /*
   *fprintf(stderr, "\nEntering _recursiveH5check, in file: %s, at line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgrp_name: <%s>\n", grp_name);
   */

    /* Init searcher structure */
    searcher._searchH5obj  = _searchH5obj;
    searcher._searchH5link = _searchH5link;
    searcher.udata         = (void *)rootgrp;

    /* Get info for starting object */
    if(H5Oget_info_by_name((*rootgrp)->fid, grp_name, &oinfo, H5P_DEFAULT) < 0)
        return FAILED;

    /* Searching the object */
    if(_searchH5obj)
    {
        (_searchH5obj)(grp_name, &oinfo, searcher.udata, NULL);
    }

    /* if the object is a group */
    if(oinfo.type == H5O_TYPE_GROUP)
    {
        /* Check for multiple links to top group */
        if(oinfo.rc > 1)
            _addH5address(tudata.seen, oinfo.addr, grp_name);

      /*Set up user data structure*/
        tudata.searcher = &searcher;
        tudata.is_absolute = (*grp_name == '/');
        tudata.base_grp_name = grp_name;

      /*Visit all links in group, recursively*/
        ret_status = H5Lvisit_by_name((*rootgrp)->fid, grp_name, H5_INDEX_NAME, H5_ITER_INC, _searchH5byName, &tudata, H5P_DEFAULT);
        if(0 > ret_status)
        {
            fprintf(stderr, "\n\n\n");
            fprintf(stderr, "**************************************************************\n");
            fprintf(stderr, "FAILED in file: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\tgrp_name: <%s>\n", grp_name);
            fprintf(stderr, "\tfid: %ld\n", (*rootgrp)->fid);
            fprintf(stderr, "FAILED in file: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "**************************************************************\n");
            fprintf(stderr, "\n\n\n");

          /*
           *fprintf(stderr, "Leaving _NclHDF5recursive_check, at file: %s, line: %d\n\n", __FILE__, __LINE__);
           */

            return FAILED;
        }

#if 0
        /* Free visited addresses table */
        if(tudata.seen.objs)
        {
            free(tudata.seen.objs);
        }
#endif
    }

  /*
   *fprintf(stderr, "Leaving _recursiveH5check, in file: %s, at line: %d\n\n", __FILE__, __LINE__);
   */

    return SUCCEED;
}

static int H5InitializeOptions(NclFileGrpNode *grpnode)
{
    NCLOptions *options;

    grpnode->n_options = H5_NUM_OPTIONS;

    options = NclMalloc(grpnode->n_options * sizeof(NCLOptions));
    if (! options)
    {
        NHLPERROR((NhlFATAL,ENOMEM,NULL));
        return 0;
    }

    options[H5_USE_CACHE_OPT].name = NrmStringToQuark("usecache");
    options[H5_USE_CACHE_OPT].type = NCL_int;
    options[H5_USE_CACHE_OPT].size = 1;
    options[H5_USE_CACHE_OPT].values = (void *) 0;

    options[H5_COMPRESSION_LEVEL_OPT].name = NrmStringToQuark("compressionlevel");
    options[H5_COMPRESSION_LEVEL_OPT].type = NCL_int;
    options[H5_COMPRESSION_LEVEL_OPT].size = 1;
    options[H5_COMPRESSION_LEVEL_OPT].values = (void *) -1;

    options[H5_CACHE_SIZE_OPT].name = NrmStringToQuark("cachesize");
    options[H5_CACHE_SIZE_OPT].type = NCL_int;
    options[H5_CACHE_SIZE_OPT].size = 1;
    options[H5_CACHE_SIZE_OPT].values = (void *) 3200000;

    options[H5_CACHE_NELEMS_OPT].name = NrmStringToQuark("cachenelems");
    options[H5_CACHE_NELEMS_OPT].type = NCL_int;
    options[H5_CACHE_NELEMS_OPT].size = 1;
    options[H5_CACHE_NELEMS_OPT].values = (void *) 1009;

    options[H5_CACHE_PREEMPTION_OPT].name = NrmStringToQuark("cachepreemption");
    options[H5_CACHE_PREEMPTION_OPT].type = NCL_float;
    options[H5_CACHE_PREEMPTION_OPT].size = 1;
    options[H5_CACHE_PREEMPTION_OPT].values = (void *) 0;

    grpnode->options = options;
    return 0;
}

static void *H5InitializeFileRec(NclFileFormat *format)
{
    static int first = True;
    NclFileGrpNode *grpnode = NULL;

    if (first)
    {
        Qmissing_val = NrmStringToQuark("missing_value");
        Qfill_val = NrmStringToQuark("_FillValue");
        first = False;
    }

    /*nc_set_log_level(3);*/
    nc_set_log_level(3);

    grpnode = (NclFileGrpNode *)NclCalloc(1, sizeof(NclFileGrpNode));
    assert(grpnode);

    grpnode->gid = -1;
    grpnode->fid = -1;
    grpnode->pid = -1;
    grpnode->name = NrmStringToQuark("/");
    grpnode->pname = NrmStringToQuark("-");
    grpnode->real_name = NrmStringToQuark("/");
    grpnode->path = -1;
    grpnode->extension = -1;

    grpnode->open = 0;
    grpnode->header_reserve_space = 0;
    grpnode->define_mode = 0;
    grpnode->other_src = NULL;
    grpnode->parent = NULL;
    if(H5InitializeOptions(grpnode))
    {
        NclFree(grpnode);
        return NULL;
    }
    *format = _NclHDF5;
    setvbuf(stderr,NULL,_IONBF,0);
    return (void *) grpnode;
}

/*-------------------------------------------------------------------------
 * Function:	_readH5info
 *
 * Purpose:	read information about this h5 file.
 *
 * Return:	Success: 0
 *		Failure: -1
 *
 * Programmer:	Wei Huang
 *		April 10, 2012
 *
 *-------------------------------------------------------------------------
 */

static herr_t _readH5info(NclFileGrpNode **rootgrp)
{
    H5O_info_t oi;              /* Information for object */
    herr_t status = FAILED;

    static char root_name[] = "/";

    H5_addr_t        seen;     /* List of addresses seen */

    memset(&tudata, 0, sizeof(H5_ud_traverse_t));

  /*Init addresses seen*/
    seen.nalloc = 1024;
    seen.nused = 0;
    seen.objs = (H5obj_t*) NclCalloc(seen.nalloc, sizeof(H5obj_t));;

  /*Set up user data structure*/
    tudata.seen = &seen;

    /* Retrieve info for object */
    status = H5Oget_info_by_name((*rootgrp)->fid, root_name, &oi, H5P_DEFAULT);

    (*rootgrp)->name = NrmStringToQuark(root_name);

    if(status == FAILED)
    {
        fprintf(stderr, "Failed to get info for fid: %ld, root_name: <%s>, in file: %s, at line: %d\n",
            (*rootgrp)->fid, root_name, __FILE__, __LINE__);
        return FAILED;
    }

    /* Check for group */
    if(H5O_TYPE_GROUP == oi.type)
    {
        hid_t      gid;		/* Object id */
      /*
       *Need to Check if it is external link.
       *if(is_elink())
       *    return SUCCEED;
       */

        /* Get ID for group */
        gid = H5Gopen((*rootgrp)->fid, root_name, H5P_DEFAULT);
        if(gid < 0)
        {
            fprintf(stderr, "Unable to open '%s' as group\n", root_name);
            return FAILED;
        }

      /*
       *fprintf(stderr, "\nin file: %s, at line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\t(*rootgrp)->gid = %ld, (*rootgrp)->real_name: <%s>\n",
       *                   (*rootgrp)->gid, NrmQuarkToString((*rootgrp)->real_name));
       */

      /*Specified name is a group. Search the complete contents of the group. */
        _recursiveH5check(rootgrp, _searchH5obj, _searchH5link);

       (*rootgrp)->gid = -1;

      /*Close group*/
        H5Gclose(gid);
    }
    else
    {
        fprintf(stderr, "<%s> is a non-group object, in file: %s, at line: %d\n",
                root_name, __FILE__, __LINE__);
    }

    return SUCCEED;
}

NhlErrorTypes _addH5dim(NclFileDimRecord **grpdimrec, NclQuark dimname,
                        ng_size_t dimsize, int is_unlimited)
{
    NclFileDimRecord *dimrec = *grpdimrec;
    NclFileDimNode   *dimnode = NULL;
    NhlErrorTypes ret = NhlNOERROR;
    int n;

  /*
   *fprintf(stderr, "\nEnter _addH5dim, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tdimname: <%s>, dimsize: %d\n", NrmQuarkToString(dimname), dimsize);
   */

    if(NULL == dimrec)
    {
        dimrec = _NclFileDimAlloc(2);
        grpdimrec = &dimrec;
        dimrec->n_dims = 0;
    }
    else
    {
        for(n = 0; n < dimrec->n_dims; ++n)
        {
            dimnode = &(dimrec->dim_node[n]);
            if(dimname == dimnode->name)
            {
                return ret;
            }
        }
    }

    n = dimrec->n_dims;

    if(dimrec->n_dims >= dimrec->max_dims)
    {
        _NclFileDimRealloc(dimrec);
        grpdimrec = &dimrec;
    }

    dimnode = &(dimrec->dim_node[n]);
    ++dimrec->n_dims;

    dimnode->id   = n;
    dimnode->name = dimname;
    dimnode->size = dimsize;
    dimnode->is_unlimited = is_unlimited;
    dimnode->is_dataset   = 0;

  /*
   *fprintf(stderr, "Leave _addH5dim, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

static int _buildH5dimlist(NclFileGrpNode **rootgrp)
{
    int ndims = 0;
    NclFileGrpNode *grpnode = *rootgrp;

    NclFileVarRecord *varrec = NULL;
    NclFileDimRecord *grpdimrec = NULL;
    NclFileDimRecord *vardimrec = NULL;

    NclFileVarNode   *varnode = NULL;
    NclFileDimNode   *vardimnode = NULL;

    int i, j, n;
    char tmp_name[MAX_NCL_NAME_LENGTH];

  /*
   *fprintf(stderr, "\nEnter _buildH5dimlist. file: %s, line: %d\n", __FILE__, __LINE__);
   */

    if(NULL != grpnode->grp_rec)
    {
        for(n = 0; n < grpnode->grp_rec->n_grps; ++n)
        {
            ndims = _buildH5dimlist(&(grpnode->grp_rec->grp_node[n]));
        }
    }

    varrec = grpnode->var_rec;

    if(NULL == varrec)
        return 0;

    for(n = 0; n < varrec->n_vars; ++n)
    {
        varnode = &(varrec->var_node[n]);

        vardimrec = varnode->dim_rec;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tvar name: <%s>, ndims = %d\n",
       *                   NrmQuarkToString(varnode->name),
       *                   vardimrec->n_dims);
       */

        for(i = 0; i < vardimrec->n_dims; ++i)
        {
            vardimnode = &(vardimrec->dim_node[i]);
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tDim %d, name: <%s>, length: %d\n",
           *                   n, NrmQuarkToString(vardimnode->name), vardimnode->size);
           */

            if(NULL == grpnode->dim_rec)
            {
                if(0 > vardimnode->name)
                {
                    sprintf(tmp_name, "phony_dim_%4.4d", i);
                    vardimnode->name = NrmStringToQuark(tmp_name);
                }
                _addH5dim(&grpdimrec, vardimnode->name,
                          vardimnode->size, 0);
                grpnode->dim_rec = grpdimrec;
            }
            else
            {
                int new_dim = 1;

                for(j = 0; j < grpdimrec->n_dims; ++j)
                {
                    if(vardimnode->name == grpdimrec->dim_node[j].name)
                    {
                        new_dim = 0;
                        break;
                    }
                }

                if(new_dim)
                {
                    if(0 > vardimnode->name)
                    {
                        j = grpdimrec->n_dims;
                        sprintf(tmp_name, "phony_dim_%4.4d", j);
                        vardimnode->name = NrmStringToQuark(tmp_name);
                    }
                    _addH5dim(&grpdimrec, vardimnode->name, vardimnode->size, 0);
                }
            }
        }
    }

  /*
   *fprintf(stderr, "Leave _buildH5dimlist. file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return ndims;
}

void *H5OpenFile(void *rec, NclQuark path, int status)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *) rec;

    hid_t fid = -1;

    if(NULL == grpnode)
    {
        return(NULL);
    }

    grpnode->path = path;
    grpnode->status = status;
    grpnode->compress_level = 0;

  /*
   *fprintf(stderr,"\nEnter H5OpenFile, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr,"\tpath: <%s>\n", (char *)NrmQuarkToString(path));
   */

    fid = grpnode->fid;

    if(fid < 0)
    {
        if(status > 0)
        {
          /*
           *fprintf(stderr,"\nEnter H5OpenFile, file: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr,"\tpath: <%s>\n", (char *)NrmQuarkToString(path));
           */
            fid = H5Fopen(NrmQuarkToString(path), H5F_ACC_RDONLY, H5P_DEFAULT);
        }
        else
        {
            fid = H5Fcreate(NrmQuarkToString(path), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        }

        grpnode->open = 1;
        grpnode->define_mode = 0;
        grpnode->fid = fid;
        grpnode->gid = fid;
        grpnode->pid = -1;
        grpnode->parent = NULL;
    }

    if(fid < 0)
    { 
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,"%s: Cannot open file: <%s>\n", __FILE__, NrmQuarkToString(path)));
        H5close();
        NclFree(grpnode);
        return(NULL);
    }

    _readH5info(&grpnode);

    _buildH5dimlist(&grpnode);

  /*
   *fprintf(stderr,"Leave H5OpenFile, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return((void*)grpnode);
}

/*
 ***********************************************************************
 * Function:	_readH5dataset
 *
 * Purpose:	read a dataset
 *
 * Programmer:	Wei Huang
 * Created:	April 30, 2012
 *
 ***********************************************************************
 */

void _readH5dataset(hid_t dset, hid_t d_type,
                    ng_size_t *start, ng_size_t *finish,
                    ng_size_t *stride, ng_size_t *count,
                    void *value)
{
    hid_t               d_space;                  /* data space */
    hid_t               m_space;                  /* memory space */
    size_t              i;

    /* Hyperslab info */
    hsize_t            d_start[H5S_MAX_RANK];
    hsize_t            d_stride[H5S_MAX_RANK];
    hsize_t            d_count[H5S_MAX_RANK];
    hsize_t            d_block[H5S_MAX_RANK];

    hsize_t            m_start[H5S_MAX_RANK];
    hsize_t            m_stride[H5S_MAX_RANK];
    hsize_t            m_count[H5S_MAX_RANK];
    hsize_t            m_block[H5S_MAX_RANK];

    hsize_t            ndims;
    int                read_slab = 0;

    herr_t             status;

  /*
   *fprintf(stderr, "\nEnter _readH5dataset, file: %s, line: %d\n", __FILE__, __LINE__);
   */

    d_space = H5Dget_space(dset);
    if (d_space == FAILED)
        return;

    ndims = H5Sget_simple_extent_ndims(d_space);

    if (ndims > H5S_MAX_RANK)
    {
        H5Sclose(d_space);
        return;
    }

    if (ndims < 0)
    {
        NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                  "\nCannot have negative ndims: %d, file: %s, line: %d\n",
                   ndims, __FILE__, __LINE__));
        H5Sclose(d_space);
        return;
    }

    status = H5Sget_simple_extent_dims(d_space, m_count, NULL);

    for(i = 0; i < ndims; ++i)
    {
        if(count[i] != m_count[i])
            ++ read_slab;
      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tcount[%d] = %d, m_count[%d] = %d\n",
       *                 i, count[i], i, m_count[i]);
       */
         m_count[i] = count[i];
    }

    m_space = H5Screate_simple(ndims, m_count, NULL);

    /* Calculate the hyperslab size */
    if(read_slab)
    {
        for (i = 0; i < ndims; i++)
        {
            d_start[i] = start[i];
            d_stride[i] = stride[i];
            d_count[i] = count[i];
            d_block[i] = 1;

            m_start[i] = 0;
            m_stride[i] = 1;
            m_block[i] = 1;
        }

        H5Sselect_hyperslab(d_space, H5S_SELECT_SET, d_start, d_stride,
                            d_count, d_block);

      /*
       *fprintf(stderr, "\tselect slab in file: %s, line: %d\n", __FILE__, __LINE__);
       */

        H5Sselect_hyperslab(m_space, H5S_SELECT_SET, m_start, m_stride,
                            m_count, m_block);
    }
    else
    {
      /*
       *fprintf(stderr, "\tselect all in file: %s, line: %d\n", __FILE__, __LINE__);
       */
        H5Sselect_all(d_space);
        H5Sselect_all(m_space);
    }

    /* Read the data */
    status = H5Dread(dset, d_type, m_space, d_space, H5P_DEFAULT, value);

    if(0 != status)
    {
        fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
    }

    H5Sclose(m_space);
    H5Sclose(d_space);

  /*
   *fprintf(stderr, "Leave _readH5dataset, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

/*
 ***********************************************************************
 * Function:	_getH5data
 *
 * Purpose:	get the dataset
 *
 * Return:	pointer to unsigned char
 *
 * Programmer:	Wei Huang
 *		April 27, 2012
 *
 ***********************************************************************
 */

void _getH5data(hid_t fid, NclFileVarNode *varnode,
                ng_size_t *start, ng_size_t *finish,
                ng_size_t *stride, ng_size_t *count,
                void *storage)
{
    hid_t       did;
    H5S_class_t space_type;
    hid_t       d_space;
    hid_t       d_type;
    hid_t       p_type;
    char	*type_name = _NclBasicDataTypeToName(varnode->type);

  /*
   *fprintf(stderr, "\nEnter _getH5data, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tfid = %ld, varname: <%s>\n", fid, NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->real_name), H5P_DEFAULT);

  /*
   * Get datatype and dataspace handles and then query
   * dataset class, order, size, rank and dimensions.
   */
    d_type = H5Dget_type(did);
    p_type = toH5type(type_name);

  /*Check the data space*/
    d_space = H5Dget_space(did);

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
             _readH5dataset(did, p_type, start, finish, stride, count, storage);
             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

  /*
   *fprintf(stderr, "Leave _getH5data, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

/*
 ***********************************************************************
 * Function:	*_getH5compoundAsList
 *
 * Purpose:	get the compound data as list.
 *
 * Programmer:	Wei Huang
 *		July 27, 2012
 *
 ***********************************************************************
 */

void *_getH5compoundAsList(hid_t fid, NclFileVarNode *varnode)
{
    hid_t       did = -1;
    herr_t      status = 0;
    hsize_t     size  = 1;
    hsize_t     ndims = 0;

    char *component_name;

    NclList complist;

    NclVar compvar;

    ng_size_t one = 1;

    ng_size_t dimsizes[H5S_MAX_RANK];
    ng_size_t dimnames[H5S_MAX_RANK];

    NclMultiDValData comp_md;
    int *id = (int *)NclMalloc(sizeof(int));
    int   n = 0;

    NclFileCompoundNode *compnode = NULL;

    hid_t datatype_id = -1;
    hid_t component_datasize = 1;
    hid_t str_type = 0;

    int i = 0;

  /*
   *fprintf(stderr, "\nEnter _getH5compoundAsList, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    if(NULL == varnode->comprec)
        return NULL;

    did = H5Dopen(fid, NrmQuarkToString(varnode->real_name), H5P_DEFAULT);

  /*
   *complist = (NclList)_NclListCreate(NULL, NULL, Ncl_List, 0, NCL_FIFO);
   */
    complist = (NclList)_NclListCreate(NULL, NULL, 0, 0, NCL_FIFO);
    assert(complist);
    _NclListSetType((NclObj)complist, NCL_FIFO);
    complist->list.list_quark = NrmStringToQuark("compound_components_list");
    complist->list.list_type = Ncl_List;

    *id = complist->obj.id;
    comp_md = _NclMultiDVallistDataCreate(NULL,NULL,Ncl_MultiDVallistData,0,id,
                                          NULL,1,&one,TEMPORARY,NULL);

    ndims = 0;
    size = 0;

    if(NULL != varnode->dim_rec)
    {
        size = 1;
        ndims = varnode->dim_rec->n_dims;
        for(n = 0; n < varnode->dim_rec->n_dims; ++n)
        {
            dimnames[n] = varnode->dim_rec->dim_node[n].name;
            dimsizes[n] = varnode->dim_rec->dim_node[n].size;
            size       *= varnode->dim_rec->dim_node[n].size;
        }
    }

    for(n = 0; n < varnode->comprec->n_comps; ++n)
    {
        compnode = &(varnode->comprec->compnode[n]);

        component_name = NrmQuarkToString(compnode->name);

        if(NCL_string == compnode->type)
        {
            char* cstr = (void *)NclCalloc(1 + compnode->nvals, 1);
            char* carray = (void *)NclCalloc(size * compnode->nvals, 1);
            NrmQuark* qvals = (void*) NclCalloc(size, sizeof(NrmQuark));
            assert(qvals);
            assert(carray);
            assert(cstr);

            str_type = H5Tcopy(H5T_C_S1);
            status += H5Tset_size(str_type, compnode->nvals);
            datatype_id = H5Tcreate( H5T_COMPOUND, compnode->nvals);
            H5Tinsert(datatype_id, component_name, 0, str_type);

            status += H5Dread(did, datatype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, carray);

            for(i = 0; i < size; ++i)
            {
                memcpy(cstr, i+carray, compnode->nvals);
                qvals[i] = NrmStringToQuark(cstr);
            }

            NclFree(cstr);
            NclFree(carray);

            compvar = _NclCreateVlenVar(component_name, qvals,
                                        ndims, dimnames,
                                        dimsizes, compnode->type);

            H5Tclose(str_type);
            H5Tclose(datatype_id);

            _NclListAppend((NclObj)complist, (NclObj)compvar);
        }
        else
        {
            NclVar compvar;
            void *values;

            component_datasize = _NclSizeOf(compnode->type);
            values = (void *)NclCalloc(size, component_datasize);
            assert(values);

            datatype_id = H5Tcreate( H5T_COMPOUND, component_datasize);
            H5Tinsert(datatype_id, component_name, 0,
                      Ncltype2HDF5type(compnode->type));

            status += H5Dread(did, datatype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, values);

            compvar = _NclCreateVlenVar(component_name, values,
                                        ndims, dimnames,
                                        dimsizes, compnode->type);
            H5Tclose(datatype_id);
            _NclListAppend((NclObj)complist, (NclObj)compvar);
        }
    }

    if(SUCCEED != status)
        my_hdf5_error_handler(status, __FILE__, __LINE__);

  /*Close the dataspace*/
    H5Dclose(did);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarnode->name: <%s>, varnode->type: <%s>\n",
   *                 NrmQuarkToString(varnode->name), _NclBasicDataTypeToName(varnode->type));
   *fprintf(stderr, "Leave _getH5compoundAsList, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (void *)comp_md;
}

/*
 ***********************************************************************
 * Function:	_getH5Compounddata
 *
 * Purpose:	read compound data
 *
 * Programmer:	Wei Huang
 *		June 4, 2012
 *
 ***********************************************************************
 */

static void *_getH5CompoundData(hid_t fid, NclFileVarNode *varnode,
                               NclQuark varname, void *storage)
{
    char *component_name;
    char *struct_name;
    char  buffer[NCL_MAX_STRING];
    hid_t did;
    hid_t datatype_id = -1;
    hid_t component_datasize = 1;
    hid_t str_type = 0;

    herr_t status = 0;

    NclFileCompoundNode *compnode = NULL;

  /*
   *fprintf(stderr, "\nEnter _getH5CompoundData, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varname));
   */

    component_name = _getComponentName(NrmQuarkToString(varname), &struct_name);
    if(NULL == component_name)
    {
      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tCan not found component from varname: <%s>\n", NrmQuarkToString(varname));
       */
        storage = (void *) _getH5compoundAsList(fid, varnode);
        return storage;
    }

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varname));
   *fprintf(stderr, "\tcomponent_name: <%s>, struct_name: <%s>\n",
   *                   component_name, struct_name);
   */

    compnode = _getComponentNodeFromVarNode(varnode, component_name);

    did = H5Dopen(fid, NrmQuarkToString(varnode->real_name), H5P_DEFAULT);

    strcpy(buffer, _NclBasicDataTypeToName(compnode->type));

    if(NCL_string == compnode->type)
    {
        char* cstr = NULL;
        NrmQuark qval = -1;

        str_type = H5Tcopy(H5T_C_S1);

        cstr = (char*) NclMalloc(compnode->nvals + 1);

        status = H5Tset_size(str_type, compnode->nvals);

        datatype_id = H5Tcreate( H5T_COMPOUND, compnode->nvals);

        H5Tinsert(datatype_id, component_name, 0, str_type);

        status = H5Dread(did, datatype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, cstr);

        if(0 != status)
        {
            NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                      "\nProblem to read compound: <%s> from: <%s>\n",
                       component_name, NrmQuarkToString(varnode->real_name)));
            H5Tclose(str_type);
            return storage;
        }

        cstr[compnode->nvals] = '\0';

        qval = NrmStringToQuark(cstr);

	if(NULL == storage)
            storage = (void*)NclMalloc(sizeof(NrmQuark));

        memcpy(storage, &qval, sizeof(NrmQuark));

        NclFree(cstr);

        H5Tclose(str_type);
    }
    else
    {
        component_datasize = _NclSizeOf(compnode->type);
        datatype_id = H5Tcreate( H5T_COMPOUND, component_datasize);
        H5Tinsert(datatype_id, component_name, 0, 
                  Ncltype2HDF5type(compnode->type));

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varname));
       *fprintf(stderr, "\tcomponent_name: <%s>, compnode->type: %d\n", 
       *                   component_name, Ncltype2HDF5type(compnode->type));
       *fprintf(stderr, "\tH5T_NATIVE_INT = %d, H5T_NATIVE_FLOAT = %d, H5T_NATIVE_DOUBLE = %d\n",
       *                   H5T_NATIVE_INT, H5T_NATIVE_FLOAT, H5T_NATIVE_DOUBLE);
       */

      /*
       *if(NULL == storage)
       *{
       *    storage = (void *)NclCalloc(varnode->dim_rec->dim_node->size, component_datasize);
       *}
       */

        status = H5Dread(did, datatype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, storage);
    }

    H5Tclose(datatype_id);
    H5Dclose(did);

    free(component_name);
    free(struct_name);

  /*
   *fprintf(stderr, "Leave _getH5CompoundData, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return storage;
}

/*
 ***********************************************************************
 * Function:	_readH5string
 *
 * Purpose:	read a string dataset
 *
 * Programmer:	Wei Huang
 * Created:	April 30, 2012
 *
 ***********************************************************************
 */

void _readH5string(hid_t dset, hid_t d_type,
                   ng_size_t *start, ng_size_t *finish,
                   ng_size_t *stride, ng_size_t *count,
                   void *value)
{
    hid_t               d_space;                  /* data space */
    hid_t               m_space;                  /* memory space */
    size_t              i;

    /* Hyperslab info */
    hsize_t            d_start[H5S_MAX_RANK];
    hsize_t            d_stride[H5S_MAX_RANK];
    hsize_t            d_count[H5S_MAX_RANK];
    hsize_t            d_block[H5S_MAX_RANK];

    hsize_t            m_start[H5S_MAX_RANK];
    hsize_t            m_stride[H5S_MAX_RANK];
    hsize_t            m_count[H5S_MAX_RANK];
    hsize_t            m_block[H5S_MAX_RANK];

    hsize_t            ndims;
    int                read_slab = 0;

    herr_t             status;

    NclQuark          *strquark = (NclQuark *)value;

    char             **strdata;

    size_t             numstr = 0;

  /*
   *fprintf(stderr, "\nEnter _readH5string, file: %s, line: %d\n", __FILE__, __LINE__);
   */

    d_space = H5Dget_space(dset);
    if (d_space == FAILED)
        return;

    ndims = H5Sget_simple_extent_ndims(d_space);

    if (ndims > H5S_MAX_RANK)
    {
        NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                  "\nCannot have huge ndims: %d, file: %s, line: %d\n",
                   ndims, __FILE__, __LINE__));
        H5Sclose(d_space);
        return;
    }

    if (ndims < 0)
    {
        NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                  "\nCannot have negative ndims: %d, file: %s, line: %d\n",
                   ndims, __FILE__, __LINE__));
        H5Sclose(d_space);
        return;
    }

    status = H5Sget_simple_extent_dims(d_space, m_count, NULL);

    numstr = 1;
    for(i = 0; i < ndims; ++i)
    {
        if(count[i] != m_count[i])
            ++ read_slab;
      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tcount[%d] = %ld, m_count[%d] = %ld\n",
       *                 i, count[i], i, m_count[i]);
       */

         m_count[i] = count[i];
         numstr *= count[i];
    }

  /*
   *lenstr = H5Tget_size(d_type);
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tlenstr = %ld\n", lenstr);
   */

    strdata = (char **)NclCalloc(numstr, sizeof(char *));

    m_space = H5Screate_simple(ndims, m_count, NULL);

    /* Calculate the hyperslab size */
    if(read_slab)
    {
        for (i = 0; i < ndims; i++)
        {
            d_start[i] = start[i];
            d_stride[i] = stride[i];
            d_count[i] = count[i];
            d_block[i] = 1;

            m_start[i] = 0;
            m_stride[i] = 1;
            m_block[i] = 1;
        }

        H5Sselect_hyperslab(d_space, H5S_SELECT_SET, d_start, d_stride,
                            d_count, d_block);

      /*
       *fprintf(stderr, "\tselect slab in file: %s, line: %d\n", __FILE__, __LINE__);
       */

        H5Sselect_hyperslab(m_space, H5S_SELECT_SET, m_start, m_stride,
                            m_count, m_block);
    }
    else
    {
      /*
       *fprintf(stderr, "\tselect all in file: %s, line: %d\n", __FILE__, __LINE__);
       */
        H5Sselect_all(d_space);
        H5Sselect_all(m_space);
    }

    /* Read the data */
    
  /*
    m_space = H5Screate(H5S_SCALAR);
    d_type = H5Tcopy(H5T_C_S1);
    d_space = H5Screate(H5S_SCALAR);
   */
    status = H5Dread(dset, d_type, m_space, d_space, H5P_DEFAULT, strdata);

    if(0 != status)
    {
        fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
    }

    for(i = 0; i < numstr; ++i)
    {
        if(NULL != strdata[i])
        {
          /*
           *fprintf(stderr, "\tin file: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tstrdata[%ld]: <%s>\n", i, strdata[i]);
           */
            strquark[i] = NrmStringToQuark(strdata[i]);
        }
        else
        {
            strquark[i] = -1;
        }
    }

    H5Sclose(m_space);
    H5Sclose(d_space);

    NclFree(strdata);
  /*
   *fprintf(stderr, "Leave _readH5string, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

/*
 ***********************************************************************
 * Function:	_readH5reference
 *
 * Purpose:	read a reference dataset
 *
 * Programmer:	Wei Huang
 * Created:	Feb 6, 2014
 *
 ***********************************************************************
 */

NrmQuark* _readH5reference(hid_t dset, hid_t gid, hid_t type, const char* varname,
                      int *nrefs)
{
    herr_t     link_ret = -1;
    H5L_info_t link_info;
    NrmQuark*  refquarks = NULL;

  /*
   *fprintf(stderr, "\nEnter _readH5reference, file: %s, line: %d\n", __FILE__, __LINE__);
   */

    *nrefs = 0;

    link_ret = H5Lget_info(dset, varname, &link_info, H5P_DEFAULT);

    if(0 > link_ret)
    {
        fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
        fprintf(stderr, "\tCheck link info failed. link_ret = %d\n", link_ret);
        return NULL;
    }

    if(H5L_TYPE_HARD == link_info.type)
    {
        H5O_info_t  oinfo;

      /*Stat the object*/
        link_ret = H5Oget_info_by_name(gid, varname, &oinfo, H5P_DEFAULT);

        if(0 > link_ret)
        {
            fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\tH5Oget_info_by_name failed. link_ret = %d\n", link_ret);
            return NULL;
        }

        if((H5O_TYPE_DATASET == oinfo.type) && (H5T_REFERENCE == H5Tget_class(type)))
        {
            hid_t       f_space;
            hid_t       p_type;
            H5S_class_t space_type;
            hid_t       f_type = H5Dget_type(dset);

          /*Check the data space*/
            f_space = H5Dget_space(dset);
            space_type = H5Sget_simple_extent_type(f_space);

            if((H5S_SIMPLE == space_type) || (H5S_SCALAR == space_type))
            {
                p_type = _get_native_type(f_type);
                refquarks = _get_refquarks(dset, p_type, nrefs);
            }

            H5Sclose(f_space);

          /*
           *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tH5Oget_info_by_name failed. link_ret = %d\n", link_ret);
           */
        }
        else
        {
            fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
            fprintf(stderr, "\tUnknown oinfo.type = %ld\n", (long)oinfo.type);
        }
    }

  /*
   *fprintf(stderr, "Leave _readH5reference, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return refquarks;
}

/*
 ***********************************************************************
 * Function:	_getH5reference
 *
 * Purpose:	get the string dataset
 *
 * Programmer:	Wei Huang
 *		June 8, 2012
 *
 ***********************************************************************
 */

void _getH5reference(hid_t fid, NclFileVarNode *varnode,
                  ng_size_t *start, ng_size_t *finish,
                  ng_size_t *stride, ng_size_t *count,
                  void *storage)
{
    hid_t       did;
    H5S_class_t space_type;
    hid_t       d_space;
    hid_t       d_type;

  /*
   *fprintf(stderr, "\nEnter _getH5reference, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->real_name), H5P_DEFAULT);

    d_type = H5Dget_type(did);
    d_space = H5Dget_space(did);

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
             {
                 ng_size_t dimsize[H5S_MAX_RANK];
                 ng_size_t i, n, position, start_position, totalsize;
                 int       nref = 0;
                 NrmQuark* quarkptr = (NrmQuark*) storage;
                 NrmQuark* refquarks = _readH5reference(did, varnode->gid, d_type,
                                                        NrmQuarkToString(varnode->real_name), &nref);

                 dimsize[0] = 0;
                 totalsize = 1;
                 for(n = 0; n < varnode->dim_rec->n_dims; ++n)
                 {
                     totalsize *= varnode->dim_rec->dim_node[n].size;
                     dimsize[n+1] = totalsize;
                 }

                 position = 0;
                 for(n = varnode->dim_rec->n_dims - 1; n >= 0; --n)
                 {
                   /*
                    *fprintf(stderr, "\tstart[%ld] = %ld, finish[%ld] = %ld, stride[%ld] = %ld\n",
                    *                  (long)n, (long)start[n], (long)n, (long)finish[n], (long)n, (long)stride[n]);
                    */

                     start_position = dimsize[n];
                     for(i = start[n]; i <= finish[n]; i += stride[n])
                     {
                         quarkptr[position] = refquarks[start_position + i];
                       /*
                        *fprintf(stderr, "\tquarkptr[%ld]: <%s>\n", 
                        *              (long)position, NrmQuarkToString(quarkptr[position]));
                        */
                         ++position;
                     }
                 }
                 
                 if(NULL != refquarks)
                     NclFree(refquarks);
             }
             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

  /*
   *fprintf(stderr, "Leave _getH5reference, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

/*
 ***********************************************************************
 * Function:	_getH5string
 *
 * Purpose:	get the string dataset
 *
 * Programmer:	Wei Huang
 *		June 8, 2012
 *
 ***********************************************************************
 */

void _getH5string(hid_t fid, NclFileVarNode *varnode,
                  ng_size_t *start, ng_size_t *finish,
                  ng_size_t *stride, ng_size_t *count,
                  void *storage)
{
    hid_t       did;
    H5S_class_t space_type;
    hid_t       d_space;
    hid_t       d_type;

  /*
   *fprintf(stderr, "\nEnter _getH5string, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->name), H5P_DEFAULT);

  /*
   * Get datatype and dataspace handles and then query
   * dataset class, order, size, rank and dimensions.
   */
    d_type = H5Dget_type(did);

    /* Check the data space */
    d_space = H5Dget_space(did);

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
             _readH5string(did, d_type, start, finish, stride, count, storage);
             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

  /*
   *fprintf(stderr, "Leave _getH5string, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

/*
 ***********************************************************************
 * Function:	*_getH5vlen
 *
 * Purpose:	get the Vlen dataset
 *
 * Programmer:	Wei Huang
 *		June 12, 2012
 *
 ***********************************************************************
 */

void *_getH5vlen(hid_t fid, NclFileVarNode *varnode)
{
    hid_t       did = -1;
    H5S_class_t space_type = -1;
    hid_t       d_space = -1;
    hid_t       d_type = -1;
    hid_t       super = -1;
    herr_t      status = -1;
    hsize_t     size  = 1;
    hsize_t     vlnum = 1;
    hsize_t     ndims = 0;
    hsize_t     dims[H5S_MAX_RANK];

    hvl_t       *h5vl = NULL;
    int   n = 0;

    char *typename = NULL;

    NclList vlenlist;

    void *vlenvalues;

    NclVar vlenvar;
    NclBasicDataTypes vlentype;

    ng_size_t one = 1;

    ng_size_t dimsizes[H5S_MAX_RANK];
    ng_size_t dimnames[H5S_MAX_RANK];
    char      buffer[MAX_NCL_NAME_LENGTH];

    NclMultiDValData vlen_md;
    int *id = (int *)NclMalloc(sizeof(int));

  /*
   *fprintf(stderr, "\nEnter _getH5vlen, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->name), H5P_DEFAULT);

  /*
   *Get datatype and dataspace handles
   */
    d_type  = H5Dget_type(did);
    d_space = H5Dget_space(did);

    ndims = H5Sget_simple_extent_dims(d_space, dims, NULL);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tndims = %d\n", ndims);
   */

    for(n = 0; n < ndims; ++n)
    {
        vlnum *= dims[n];
        dimsizes[n] = dims[n];
        sprintf(buffer, "%s_%3.3d", NrmQuarkToString(varnode->name), n);
        dimnames[n] = NrmStringToQuark(buffer);
    }

    h5vl = (hvl_t *) NclMalloc(vlnum * sizeof(hvl_t));

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
            /*Read the data*/
             status = H5Dread(did, d_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, h5vl);

             if(0 != status)
             {
                 fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
             }

             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Data type name*/
    super = H5Tget_super(d_type);
    typename = _getH5typeName(super, 15);

    vlentype = string2NclType(typename);
    vlenlist = (NclList)_NclListCreate(NULL, NULL, Ncl_List, 0, NCL_FIFO);
    assert(vlenlist);
    _NclListSetType((NclObj)vlenlist, NCL_FIFO);
    vlenlist->obj.obj_type = Ncl_List;

    *id = vlenlist->obj.id;
    vlen_md = _NclMultiDVallistDataCreate(NULL,NULL,Ncl_MultiDVallistData,0,id,
                                          NULL,1,&one,TEMPORARY,NULL);

    ndims = 1;
    size = H5Tget_size(super);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\ttype name: <%s>\n", typename);
   *fprintf(stderr, "\td_type size: %d\n", H5Tget_size(d_type));
   *fprintf(stderr, "\tsuper  size: %d\n", H5Tget_size(super));
   *fprintf(stderr, "\tvlentype   : <%s>\n", _NclBasicDataTypeToName(vlentype));
   *fprintf(stderr, "\tvlentype sz: %d\n", _NclSizeOf(vlentype));
   *fprintf(stderr, "\tsize: %d\n", size);
   */

    for(n = 0; n < vlnum; ++n)
    {
        vlenvalues = (void *)NclCalloc(h5vl[n].len, size);
        assert(vlenvalues);
        memcpy(vlenvalues, h5vl[n].p, h5vl[n].len * size);

      /*
       *fprintf(stderr, "\n\tVLEN No %d, length: %d\n", n, h5vl[n].len);
       *iptr = (int *) vlenvalues;
       *for(i = 0; i < h5vl[n].len; ++i)
       *    fprintf(stderr, "\tNo %d, value: %d\n", i, iptr[i]);
       */

        sprintf(buffer, "VLEN_%3.3d", n);
        dimnames[0] = NrmStringToQuark(buffer);
        dimsizes[0] = h5vl[n].len;

        sprintf(buffer, "%s_%3.3d", NrmQuarkToString(varnode->name), n);
        vlenvar = _NclCreateVlenVar(buffer, vlenvalues,
                                    ndims, dimnames,
                                    dimsizes, vlentype);
        _NclListAppend((NclObj)vlenlist, (NclObj)vlenvar);
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

    NclFree(typename);
    NclFree(h5vl);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarnode->name: <%s>, varnode->type: <%s>\n",
   *                 NrmQuarkToString(varnode->name), _NclBasicDataTypeToName(varnode->type));
   *fprintf(stderr, "Leave _getH5vlen, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (void *)vlen_md;
}

/*
 ***********************************************************************
 * Function:	*_getH5enum
 *
 * Purpose:	get the Enum dataset
 *
 * Programmer:	Wei Huang
 *		June 12, 2012
 *
 ***********************************************************************
 */

void *_getH5enum(hid_t fid, NclFileVarNode *varnode)
{
    hid_t       did = -1;
    H5S_class_t space_type = -1;
    hid_t       d_space = -1;
    hid_t       d_type = -1;
    herr_t      status = -1;
    hsize_t     size = 1;
    hsize_t     ndims = 0;
    hsize_t     dims[H5S_MAX_RANK];

    NclFileEnumRecord *enumrec = (NclFileEnumRecord *) varnode->udt;
    int   n = 0;

  /*
   *fprintf(stderr, "\nEnter _getH5enum, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->name), H5P_DEFAULT);

  /*
   * Get datatype and dataspace handles and then query
   * dataset class, order, size, rank and dimensions.
   */
    d_type = H5Dget_type(did);

  /*Check the data space*/
    d_space = H5Dget_space(did);

    ndims = H5Sget_simple_extent_dims(d_space, dims, NULL);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tndims = %d\n", ndims);
   */

    for(n = 0; n < ndims; ++n)
    {
        size *= dims[n];
    }

    enumrec->size = size;
    enumrec->values = NclCalloc(size, _NclSizeOf(enumrec->type));

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
            /*Read the data*/
             status = H5Dread(did, d_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, enumrec->values);

             if(0 != status)
             {
                 fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
             }

             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

  /*
   *switch(enumrec->type)
   *{
   *    case NCL_ubyte:
   *         {
   *              unsigned char *iptr = (unsigned int *) enumrec->values;
   *              for(n = 0; n < enumrec->size; n++)
   *              {
   *                  fprintf(stderr, "No %d: %6d\n", n, iptr[n]);
   *              }
   *              break;
   *         }
   *    case NCL_ushort:
   *         {
   *              unsigned short *iptr = (unsigned int *) enumrec->values;
   *              for(n = 0; n < enumrec->size; n++)
   *              {
   *                  fprintf(stderr, "No %d: %6d\n", n, iptr[n]);
   *              }
   *              break;
   *         }
   *    case NCL_uint:
   *         {
   *              unsigned int *iptr = (unsigned int *) enumrec->values;
   *              for(n = 0; n < enumrec->size; n++)
   *              {
   *                  fprintf(stderr, "No %d: %6d\n", n, iptr[n]);
   *              }
   *              break;
   *         }
   *    default:
   *         {
   *              uint64 *iptr = (uint64 *) enumrec->values;
   *              for(n = 0; n < enumrec->size; n++)
   *              {
   *                  fprintf(stderr, "No %d: %6lld\n", n, iptr[n]);
   *              }
   *         }
   *}

   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarnode->name: <%s>, varnode->type: <%s>\n",
   *                 NrmQuarkToString(varnode->name), _NclBasicDataTypeToName(varnode->type));
   *fprintf(stderr, "Leave _getH5enum, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (void *) enumrec;
}

/*
 ***********************************************************************
 * Function:	*_getH5opaque
 *
 * Purpose:	get the Opaque dataset
 *
 * Programmer:	Wei Huang
 *		June 14, 2012
 *
 ***********************************************************************
 */

void *_getH5opaque(hid_t fid, NclFileVarNode *varnode)
{
    hid_t       did = -1;
    H5S_class_t space_type = -1;
    hid_t       d_space = -1;
    hid_t       d_type = -1;
    herr_t      status = -1;
    hsize_t     size = 1;
    hsize_t     ndims = 0;
    hsize_t     dims[H5S_MAX_RANK];

    int   n = 0;

    char *tag = NULL;

    NclFileOpaqueRecord *opaquerec = (NclFileOpaqueRecord *) NclMalloc(sizeof(NclFileOpaqueRecord));

  /*
   *fprintf(stderr, "\nEnter _getH5opaque, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarname: <%s>\n", NrmQuarkToString(varnode->name));
   */

    did = H5Dopen(fid, NrmQuarkToString(varnode->name), H5P_DEFAULT);

  /*
   * Get datatype and dataspace handles and then query
   * dataset class, order, size, rank and dimensions.
   */
    d_type = H5Dget_type(did);
    size   = H5Tget_size(d_type);
    tag    = H5Tget_tag(d_type);

    opaquerec->name = NrmStringToQuark(tag);
    opaquerec->max_opaques = 1;
    opaquerec->n_opaques = 1;
    opaquerec->type = NCL_ubyte;
    opaquerec->size = size;

  /*Check the data space*/
    d_space = H5Dget_space(did);

    ndims = H5Sget_simple_extent_dims(d_space, dims, NULL);

    for(n = 0; n < ndims; ++n)
    {
        size *= dims[n];
    }

    opaquerec->values = NclMalloc(size);

    space_type = H5Sget_simple_extent_type(d_space);

    switch(space_type)
    {
        case H5S_SCALAR:
        case H5S_SIMPLE:
            /*Read the data*/
             status = H5Dread(did, d_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, opaquerec->values);

             if(0 != status)
             {
                 fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
             }
 
             break;
        default:
             NHLPERROR((NhlFATAL, NhlEUNKNOWN,
                       "\nUnknown space_type: %ld, file: %s, line: %d\n",
                       (long)space_type, __FILE__, __LINE__));
            break;
    }

  /*Close the dataspace*/
    H5Sclose(d_space);
    H5Tclose(d_type);
    H5Dclose(did);

    free(tag);

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvarnode->name: <%s>, varnode->type: <%s>\n",
   *                 NrmQuarkToString(varnode->name), _NclBasicDataTypeToName(varnode->type));
   *fprintf(stderr, "Leave _getH5opaque, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (void *) opaquerec;
}

static void *H5ReadVar(void *therec, NclQuark thevar,
                       ng_size_t *start, ng_size_t *finish,
                       ng_size_t *stride, void *storage)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *) therec;
    NclFileVarNode *varnode;
    ng_size_t n_elem = 1;
    int fid = -1;
    int i;
    ng_size_t count[MAX_NC_DIMS];

  /*
   *fprintf(stderr, "\nEnter H5ReadVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>\n", NrmQuarkToString(thevar));
   *fprintf(stderr, "\tgrpnode->gid = %d\n", grpnode->gid);
   */

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);

    if(NULL == varnode)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: Variable (%s) is not a variable of file (%s)",
            NrmQuarkToString(thevar),NrmQuarkToString(grpnode->path)));

        return(NULL);
    }

  /*
   *for(i = 0; i < varnode->dim_rec->n_dims; i++)
   *{
   *    fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *    fprintf(stderr, "\tstart[%d] = %d, finish[%d] = %d, stride[%d] = %d\n",
   *                     i, start[i], i, finish[i], i, stride[i]);
   *}
   */

    if(NCL_compound != varnode->type)
    {
        if(varnode->value != NULL && varnode->dim_rec->n_dims == 1)
        {
            return GetCachedValue(varnode,start[0],finish[0],stride[0],storage);
        }
    }

    for(i = 0; i < varnode->dim_rec->n_dims; i++)
    {
        count[i] = (ng_size_t)floor((finish[i] - start[i])/(double)stride[i]) + 1;
        n_elem *= count[i];
      /*
       *dimnode = &(varnode->dim_rec->dim_node[i]);
       *if(stride[i] != 1)
       *{
       *    no_stride = 0;
       *}
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tcount[%d] = %d, n_elem = %d\n", i, count[i], n_elem);
       */
    }

    fid = (hid_t)_getH5grpID(grpnode);
            
  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgrpnode->gid: %d, varnode->id: %d\n", grpnode->gid, varnode->id);
   *fprintf(stderr, "\tfid = %d\n", fid);
   */

    switch(varnode->type)
    {
        case NCL_reference:
             _getH5reference(fid, varnode, start, finish, stride, count, storage);
             break;
        case NCL_compound:
             storage = _getH5CompoundData(fid, varnode, thevar, storage);
             break;
        case NCL_list:
        case NCL_vlen:
             storage = _getH5vlen(fid, varnode);
             break;
        case NCL_enum:
             storage = _getH5enum(fid, varnode);
             break;
        case NCL_opaque:
             storage = _getH5opaque(fid, varnode);
             break;
        case NCL_string:
             _getH5string(fid, varnode, start, finish, stride, count, storage);
             break;
        default:
             _getH5data(fid, varnode, start, finish, stride, count, storage);
             break;
    }

  /*
   *fprintf(stderr, "\tthevar: <%s>\n", NrmQuarkToString(thevar));
   *fprintf(stderr, "Leave H5ReadVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return (storage);
}

static void *H5ReadCoord(void* therec, NclQuark thevar,
                          ng_size_t *start, ng_size_t *finish,
                          ng_size_t *stride,void* storage)
{
    return(H5ReadVar(therec,thevar,start,finish,stride,storage));
}

static void *H5ReadAtt(void *therec, NclQuark theatt, void *storage)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
  /*
    NclFileAttNode *attnode;
    int i,fid,ret ;
    char *tmp;

    fprintf(stderr, "\nEnter H5ReadAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   */

    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
        "NclNewHDF5: (%s) is not a global attribute of file (%s)",
        NrmQuarkToString(theatt),NrmQuarkToString(grpnode->path)));

  /*
   *fprintf(stderr, "Leave H5ReadAtt, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return(NULL);
}

static void *H5ReadVarAtt(void *therec, NclQuark thevar, NclQuark theatt, void *storage)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode;
    NclFileAttNode *attnode;

  /*
   *fprintf(stderr, "\nEnter H5ReadVarAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   */

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);

    if(NULL != varnode)
    {
        attnode = _getAttNodeFromNclFileVarNode(varnode, theatt);

        if(NULL != attnode)
        {
            if(NULL != attnode->value)
            {
                if(attnode->the_nc_type == NC_CHAR && !(theatt == Qfill_val || theatt == Qmissing_val))
                {
                    *(NclQuark *)storage = *(NclQuark *)(attnode->value);
                }
                else
                {
                    memcpy(storage, attnode->value,
                           nctypelen(attnode->the_nc_type) * attnode->n_elem);
                }
                return(storage);
            }

            if(attnode->is_virtual)
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                    "NclNewHDF5: Error retrieving value for is_virtual attribute (%s) of (%s->%s)",
                    NrmQuarkToString(theatt),NrmQuarkToString(grpnode->name),NrmQuarkToString(thevar)));
                return NULL;
            }

#if 0
            if(attnode->the_nc_type == NC_CHAR && !(theatt == Qfill_val || theatt  == Qmissing_val))
            {
                tmp = (char*)NclMalloc(attnode->n_elem + 1);
                tmp[attnode->n_elem] = '\0';
                ret = ncattget(fid, varnode->att_rec->id,NrmQuarkToString(theatt),tmp);
                *(NclQuark *)storage = NrmStringToQuark(tmp);
                NclFree(tmp);
            }
            else
            {
                ret = ncattget(fid,varnode->id,NrmQuarkToString(theatt),storage);
            }

            if(ret != -1)
                return(storage);
            else
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,"NclNewHDF5: Error retrieving value for Attribute (%s) of (%s->%s)",
                      NrmQuarkToString(theatt),NrmQuarkToString(grpnode->path),NrmQuarkToString(thevar)));
                return NULL;
            }
#endif
        }
    }

    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
        "NclNewHDF5: Attribute (%s) is not a variable attribute of (%s->%s)",
        NrmQuarkToString(theatt),NrmQuarkToString(grpnode->path),NrmQuarkToString(thevar)));

  /*
   *fprintf(stderr, "Leave H5ReadVarAtt, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return(NULL);
}

static NclFVarRec *H5GetCoordInfo(void* therec, NclQuark thevar)
{
    return((NclFVarRec *)GetVarInfo(therec,thevar));
}

static void H5FreeFileRec(void* therec)
{

    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;

    if(grpnode->open)
    {
        if(0 > grpnode->pid)
        {
            H5Fclose(grpnode->fid);
            H5close();
        }
      /*
       *else if(0 <= grpnode->gid)
       *    H5Gclose(grpnode->gid);
       */

        grpnode->open = 0;
        grpnode->fid = -1;
        grpnode->gid = -1;
        grpnode->pid = -1;
    }

    FileDestroyGrpNode(grpnode);

  /*Free visited addresses table*/
  /*FIXME
   *This cause trouble.
   *We need to fix it later. march 26, 2014, Wei
    if(tudata.seen && tudata.seen->objs)
    {
        NclFree(tudata.seen->objs);
    }
   */
}

static void *H5CreateFile(void *rec, NclQuark path)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *) rec;
    int fid = -1;
    int mode = H5P_DEFAULT;

    fid = H5Fcreate(NrmQuarkToString(path), H5F_ACC_TRUNC, mode, H5P_DEFAULT);

  /*
   *fprintf(stderr, "H5CreateFile in file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "H5CreateFile create file: <%s>\n", NrmQuarkToString(path));
   *fprintf(stderr, "H5CreateFile file id: <%d>\n", fid);
   */

    if(fid >= 0)
    {
        grpnode->fid = fid;
        grpnode->gid = fid;
        grpnode->pid = -1;
        grpnode->define_mode = 1;
        grpnode->open = 1;
        grpnode->parent = NULL;

        return ((void*)grpnode);
    }
    else
    {
        return(NULL);
    }
}

static NhlErrorTypes H5SetOption(void *rootgrp, NclQuark option,
                                 NclBasicDataTypes data_type,
                                 int n_items, void * values)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)rootgrp;

  /*
   *fprintf(stderr, "\nEnter H5SetOption, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\toption: <%s>\n", NrmQuarkToString(option));
   */

    if (option == NrmStringToQuark("usecache"))
    {
        grpnode->options[H5_USE_CACHE_OPT].values = values;
    }
    else if (option == NrmStringToQuark("compressionlevel"))
    {
        if (*(int*)values < -1 || *(int*)values > 9)
        {
            NHLPERROR((NhlWARNING,NhlEUNKNOWN,
                  "H5SetOption: option (%s) value cannot be less than -1 or greater than 9",
                   NrmQuarkToString(option)));
            return(NhlWARNING);
        }
        grpnode->options[H5_COMPRESSION_LEVEL_OPT].values = values;
    }
    else if (option == NrmStringToQuark("cachesize"))
    {
        if (*(int*)values < 1)
        {
            NHLPERROR((NhlWARNING,NhlEUNKNOWN,
                  "H5SetOption: option (%s) value cannot be less than 1",
                   NrmQuarkToString(option)));
            return(NhlWARNING);
        }
        grpnode->options[H5_CACHE_SIZE_OPT].values = values;
    }
    else if (option == NrmStringToQuark("cachenelems"))
    {
        if (*(int*)values < 3)
        {
            NHLPERROR((NhlWARNING,NhlEUNKNOWN,
                  "H5SetOption: option (%s) value cannot be less than 3",
                   NrmQuarkToString(option)));
            return(NhlWARNING);
        }
        else
        {
            unsigned int *iv = (unsigned int *)values;
            *iv = _closest_prime(*iv);
            grpnode->options[H5_CACHE_NELEMS_OPT].values = (void*) iv;
        }
    }
    else if (option == NrmStringToQuark("cachepreemption"))
    {
        float *fv = (float *)values;
        grpnode->options[H5_CACHE_PREEMPTION_OPT].values = (void*) fv;
    }

  /*
   *fprintf(stderr, "\toption: <%s>\n", NrmQuarkToString(option));
   *fprintf(stderr, "Leave H5SetOption, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    
    return NhlNOERROR;
}

static NhlErrorTypes H5AddDim(void* therec, NclQuark thedim,
                              ng_size_t size, int is_unlimited)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *) therec;
    int add_scalar = 0;
    int dimidp = 0;

  /*
   *fprintf(stderr, "\nEnter H5AddDim, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthedim: <%s>, size: %d\n", NrmQuarkToString(thedim), size);
   *fprintf(stderr, "\tgrpnode->gid = %d\n", grpnode->gid);
   */

    if(grpnode->status <=  0)
    {
        if(thedim == NrmStringToQuark("ncl_scalar"))
        {
            if (size != 1)
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                    "NclNewHDF5: <ncl_scalar> is a reserved file dimension name in NCL, %s\n",
                    "this name can only represent dimensions of size 1"));
                return(NhlFATAL);
            }
            add_scalar = 1;
        }

        if(add_scalar)
        {
            NclQuark ns_name = NrmStringToQuark("ncl_scalar");
            grpnode->has_scalar_dim = 1;

            _addNclDimNode(&(grpnode->dim_rec), ns_name, dimidp, -5, 1);
        }
        else
        {
            _addNclDimNode(&(grpnode->dim_rec), thedim, dimidp, size, is_unlimited);
        }
      /*
       *n = grpnode->dim_rec->n_dims - 1;
       *fprintf(stderr, "\tthedim: <%s>, dimid: %d\n", NrmQuarkToString(thedim),
       *                 grpnode->dim_rec->dim_node[n].id);
       *fprintf(stderr, "Leave H5AddDim, file: %s, line: %d\n\n", __FILE__, __LINE__);
       */

        return(NhlNOERROR);
    }
    else
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: File (%s) was opened as read only, can not write to it",
             NrmQuarkToString(grpnode->path)));
    }
    return(NhlFATAL);
}

static NhlErrorTypes H5AddChunkDim(void *therec, NclQuark thedim,
                                   ng_size_t size, int is_unlimited)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileDimNode *dimnode = NULL;
    int add_scalar = 0;

  /*
   *fprintf(stderr, "\nEnter H5AddChunkDim, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthedim: <%s>, size: %d\n", NrmQuarkToString(thedim), size);
   */

    if(grpnode->status <=  0)
    {
        grpnode->is_chunked = 1;
        if(thedim == NrmStringToQuark("ncl_scalar"))
        {
            if(size != 1)
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                    "NclNewHDF5: \"ncl_scalar\" is a reserved file dimension name in NCL, %s",
                    "this name can only represent dimensions of size 1"));
                return(NhlFATAL);
            }
            add_scalar = 1;
        }

        if (add_scalar)
        {
            NclQuark ns_name = NrmStringToQuark("ncl_scalar");
            grpnode->has_scalar_dim = 1;

            dimnode = _getDimNodeFromNclFileGrpNode(grpnode, ns_name);
            _addNclDimNode(&(grpnode->chunk_dim_rec), ns_name, dimnode->id, -5, 1);
        }
        else
        {
            dimnode = _getDimNodeFromNclFileGrpNode(grpnode, thedim);
            _addNclDimNode(&(grpnode->chunk_dim_rec), thedim, dimnode->id, size, is_unlimited);
        }
      /*
       *fprintf(stderr, "Leave H5AddChunkDim, file: %s, line: %d\n\n", __FILE__, __LINE__);
       */
        return(NhlNOERROR);
    }
    else
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: File (%s) was opened as read only, can not write to it",
             NrmQuarkToString(grpnode->path)));
    }
    return(NhlFATAL);
}

static NhlErrorTypes H5AddVar(void* therec, NclQuark thevar,
                              NclBasicDataTypes data_type, int n_dims,
                              NclQuark *dim_names, long *dim_sizes)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode = NULL;
    int fid,i,j;
    int dim_ids[MAX_VAR_DIMS];
    int add_scalar_dim = 0;

    hid_t var_id = -1;

  /*
   *fprintf(stderr, "\nEnter H5AddVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, type: <%s>, n_dims = %d\n",
   *                   NrmQuarkToString(thevar),
   *                   _NclBasicDataTypeToName(data_type), n_dims);
   */

    if(grpnode->status > 0)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: File (%s) was opened as a read only file, can not write to it",
             NrmQuarkToString(grpnode->path)));
        return(NhlFATAL);
    }

    fid = _getH5grpID(grpnode);

    _addNclVarNodeToGrpNode(grpnode, thevar, var_id, data_type,
                            n_dims, dim_names, dim_sizes);

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL != varnode)
    {
        varnode->gid = fid;
    }

    dim_ids[0] = -999;
    for(i = 0; i < n_dims; i++)
    {
        varnode->dim_rec->dim_node[i].id = -1;
        for(j = 0; j < grpnode->dim_rec->n_dims; j++)
        {
            if(grpnode->dim_rec->dim_node[j].name == dim_names[i])
            {
                if((n_dims > 1)&&(dim_names[i] == NrmStringToQuark("ncl_scalar")))
                {
                    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                        "NclNewHDF5: the reserved file dimension name <ncl_scalar> was used in a value %d",
                        "with more than one dimension, can not add variable"));
                    return(NhlFATAL);
                }
                dim_ids[i] = grpnode->dim_rec->dim_node[j].id;
                varnode->dim_rec->dim_node[i].id = dim_ids[i];
              /*
               *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tdimname[%d]: <%s>, dimid[%d] = %d\n",
               *                 i, NrmQuarkToString(dim_names[i]), i, dim_ids[i]);
               */
                break;
            }
        }
    } 

    if (dim_ids[0] == -999)
    {
        if (n_dims == 1 && dim_sizes[0] == 1 && dim_names[0] == NrmStringToQuark("ncl_scalar"))
        {
            dim_ids[0] = -5;
            add_scalar_dim = 1;
        }
        else
        {
            NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                "NclNewHDF5: internal error adding variable"));
            return(NhlFATAL);
        }
    }

    if(add_scalar_dim)
    {
        NclQuark dim_name = NrmStringToQuark("ncl_scalar");
        grpnode->has_scalar_dim = 1;

        _addNclDimNode(&(grpnode->dim_rec), dim_name, -999, -5, 1);
    }

  /*
   *fprintf(stderr, "\tthevar: <%s>, id: var_id = %d\n", 
   *                   NrmQuarkToString(thevar), varnode->id);
   *fprintf(stderr, "Leave H5AddVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return(NhlNOERROR);
}

NhlErrorTypes H5AddVarChunk(void* therec, NclQuark thevar,
                             int n_chunk_dims, ng_size_t *chunk_dims)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode;
    NclFileDimNode *dimnode;
    int i,ret = NhlNOERROR;

  /*
   *fprintf(stderr, "\nEnter H5AddVarChunk, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, n_chunk_dims: %d\n", NrmQuarkToString(thevar), n_chunk_dims);
   */

    if(grpnode->status > 0)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                 "File (%s) was opened as read only, can not write to it",
                  NrmQuarkToString(grpnode->path)));
        return (NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL != varnode)
    {
        if(NULL != varnode->chunk_dim_rec)
        {
            if(n_chunk_dims != varnode->chunk_dim_rec->n_dims)
            {    
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                         "Var (%s) has different chunk_dims to its dimensionality.\n",
                          NrmQuarkToString(thevar)));
                ret = NhlFATAL;
                return (ret);
            }
        }

      /*
       *varnode->chunk_dimid = (int *)NclCalloc(n_chunk_dims, sizeof(int));
       *assert(varnode->chunk_dimid);
       */

        for(i = 0 ; i < n_chunk_dims; i++)
        {
            dimnode = &(varnode->dim_rec->dim_node[i]);
            _addNclDimNode(&(varnode->chunk_dim_rec), dimnode->name, dimnode->id,
                           (ng_size_t)chunk_dims[i], dimnode->is_unlimited);
            dimnode = &(varnode->chunk_dim_rec->dim_node[i]);
            dimnode->id = i;
          /*
           *varnode->chunk_dimid[i] = i;
           */
        }
        ret = NhlNOERROR;
    }

    return (ret);
}

static NhlErrorTypes H5AddVarChunkCache(void* therec, NclQuark thevar,
                                         ng_size_t cache_size, ng_size_t cache_nelems,
                                         float cache_preemption)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode;
    int ret = NhlNOERROR;

  /*
   *fprintf(stderr, "\nEnter H5AddVarChunkCache, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, cache_size: %ld\n", NrmQuarkToString(thevar), cache_size);
   */

    if(grpnode->status > 0)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                 "NclNewHDF5: File (%s) was opened as read only, can not write to it",
                  NrmQuarkToString(grpnode->path)));
        return (NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL != varnode)
    {
        if((cache_size > 0) && (cache_nelems > 0))
            varnode->use_cache = 1;
        varnode->cache_size = cache_size;
        varnode->cache_nelems = cache_nelems;
        if(cache_preemption < 0.0)
            varnode->cache_preemption = 0.0;
        else if(cache_preemption > 1.0)
            varnode->cache_preemption = 1.0;
        else
            varnode->cache_preemption = cache_preemption;
        ret = NhlNOERROR;
    }

    return(ret);
}

static NhlErrorTypes H5SetVarCompressLevel(void* therec, NclQuark thevar,
                                           int compress_level)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode;
    int ret = NhlNOERROR;
  /*
   *fprintf(stderr, "\nEnter H5SetVarCompressLevel, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, compress_level: %d\n", NrmQuarkToString(thevar), compress_level);
   */

    if(grpnode->status > 0)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                 "NclNewHDF5: File (%s) was opened as read only, can not write to it",
                  NrmQuarkToString(grpnode->path)));
        return (NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL != varnode)
    {
        varnode->compress_level = compress_level;
    }

    return(ret);
}

static NhlErrorTypes H5AddAtt(void *therec, NclQuark theatt,
                              NclBasicDataTypes data_type,
                              int n_items, void * values)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileAttNode *attnode = NULL;
    herr_t ret_code = -1;
    hid_t fid = -1;
    htri_t attr_exists = 0;
    int i;
    
  /*
   *fprintf(stderr, "\nEnter H5AddAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\ttheatt: <%s>, n_items: %d\n", NrmQuarkToString(theatt), n_items);
   */

    if(grpnode->status > 0)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: File (%s) was opened as a read only file, can not write to it",
             NrmQuarkToString(grpnode->path)));
        return(NhlFATAL);
    }

    _addNclAttNode(&(grpnode->att_rec), theatt, data_type, n_items, values);

    fid = _getH5grpID(grpnode);

    if(fid > -1)
    {
        if(NULL != grpnode->att_rec)
        {
            grpnode->att_rec->gid = fid;

            for(i = 0; i < grpnode->att_rec->n_atts; i++)
            {
                attnode = &(grpnode->att_rec->att_node[i]);
                attr_exists = H5Aexists(fid, NrmQuarkToString(attnode->name));
              /*
               *fprintf(stderr, "\n\tfile: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\twrite att no %d: <%s>\n",
               *                   i, NrmQuarkToString(attnode->name));
               *fprintf(stderr, "\tattr_exists = %d\n", attr_exists);
               */
                if(! attr_exists)
                {
                    ret_code = _writeH5variableAttribute(fid, attnode);

                    if(ret_code)
                    {
                        NHLPERROR((NhlFATAL,NhlEUNKNOWN,"H5AddAtt: Error writing variable attribute\n"));
                        return(NhlFATAL);
                    }
                }
            }
        }
    }

  /*
   *fprintf(stderr, "Leave H5AddAtt, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return(NhlNOERROR);
}

static NhlErrorTypes H5WriteVarAtt(void *therec, NclQuark thevar,
                                   NclQuark theatt, void* data)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileAttNode *attnode;
    NclFileVarNode *varnode;

  /*
   *fprintf(stderr, "\nEnter H5WriteVarAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, theatt: <%s>\n",
   *                   NrmQuarkToString(thevar), NrmQuarkToString(theatt));
   */

    if(grpnode->status > 0)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5:H5WriteVarAtt NOT permitted write to read-only file (%s)",
             NrmQuarkToString(grpnode->path)));
        return(NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL == varnode)
        return(NhlNOERROR);

    attnode = _getAttNodeFromNclFileVarNode(varnode, theatt);
    if(NULL != attnode)
    {
        if (! attnode->is_virtual)
        {
            /* if the value is the same as before don't bother writing it */
            if((theatt != Qfill_val) && (theatt != Qmissing_val))
            {
                memcmp(attnode->value,data,
                         nctypelen(attnode->the_nc_type)*attnode->n_elem);
            }
        }
    }
    return(NhlNOERROR);
}    

static NhlErrorTypes H5AddVarAtt(void *therec, NclQuark thevar, NclQuark theatt,
                                 NclBasicDataTypes data_type, int n_items, void *values)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode = NULL;
    
  /*
   *fprintf(stderr, "\nEnter H5AddVarAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, theatt: <%s>\n",
   *        NrmQuarkToString(thevar), NrmQuarkToString(theatt));
   */

    if(grpnode->status > 0)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5AddVarAtt: File (%s) was opened as a read only file, can not write to it",
             NrmQuarkToString(grpnode->path)));
    
        return(NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
    if(NULL != varnode)
    {
        _addNclAttNode(&(varnode->att_rec), theatt, data_type, n_items, values);
      /*
       *fprintf(stderr, "Leave H5AddVarAtt, file: %s, line: %d\n\n", __FILE__, __LINE__);
       */
        return(NhlNOERROR);
    } 
    else
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5AddVarAtt: CANNOT write to un-defined variable.\n"));

        return(NhlFATAL);
    }
}

static NhlErrorTypes H5DelAtt(void *therec, NclQuark theatt)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileAttNode *attnode;
    int j;
    int ret = NhlNOERROR;

  /*
   *fprintf(stderr, "\nEnter H5DelAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\ttheatt: <%s>\n", NrmQuarkToString(theatt));
   */

    if(grpnode->status <= 0)
    {
        if(NULL == grpnode->att_rec)
            return(NhlNOERROR);

        for(j = 0; j < grpnode->att_rec->n_atts; j++)
        {
            attnode = &(grpnode->att_rec->att_node[j]);
            if(attnode->name != theatt)
                continue;

            ret = _delNclAttNode(&(grpnode->att_rec), theatt);

            if(ret == -1)
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                    "NclNewHDF5: Error to delete attribute (%s) from file (%s)",
                     NrmQuarkToString(theatt),NrmQuarkToString(grpnode->path)));
                return(NhlFATAL);
            }
            return(NhlNOERROR);
        }
    }

    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
        "NclNewHDF5: Not permit to write to read-only file (%s)",
        NrmQuarkToString(grpnode->path)));
    return(NhlFATAL);
}

static NhlErrorTypes H5DelVarAtt(void *therec, NclQuark thevar, NclQuark theatt)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode;
    NclFileAttNode *attnode = NULL;
    int ret = NhlNOERROR;

  /*
   *fprintf(stderr, "\nEnter H5DelVarAtt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, theatt: <%s>\n",
   *        NrmQuarkToString(thevar), NrmQuarkToString(theatt));
   */

    if(grpnode->status <= 0)
    {
        varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);
        if(NULL == varnode)
            return ret;

        attnode = _getAttNodeFromNclFileVarNode(varnode, theatt);
        if(NULL == attnode)
            return ret;

        ret = _delNclAttNode(&(varnode->att_rec), theatt);
        if(ret == -1)
        {
            NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                  "NclNewHDF5: Error to delete attribute (%s) from variable (%s) in file (%s)",
                  NrmQuarkToString(theatt),NrmQuarkToString(thevar),NrmQuarkToString(grpnode->path)));
            ret = NhlFATAL;
        }
    } 
    else
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "NclNewHDF5: NO write to read-only file (%s)",
             NrmQuarkToString(grpnode->path)));
        ret = NhlFATAL;
    }

    return ret;
}

/*
 ***********************************************************************
 * Function:	_writeH5variableAttribute
 *
 * Purpose:	write HDF5 varibale attribute
 *
 * Return:	SUCCEED
 *              FAILED
 *
 * Programmer:	Wei Huang
 * Created:	April 5, 2010
 * Modified:	May 31, 2012
 *
 ***********************************************************************
 */

herr_t _writeH5variableAttribute(hid_t did, NclFileAttNode *attnode)
{
    hid_t   aid;                /* Attribute dataspace identifiers */
    hid_t   atype;              /* Attribute type */
    herr_t  ret;                /* Return value */
    char    buff[MAX_NCL_BUFFER_LENGTH];
    char*   tmpstr = NULL;

    hsize_t anelem = attnode->n_elem;
    
    if(NCL_string == attnode->type)
    {
      /*
       *Create string attribute.
       */
        strcpy(buff, NrmQuarkToString(*(NclQuark *)attnode->value));
        if(attnode->n_elem > 1)
        {
            aid = H5Screate(H5S_SIMPLE);
            ret = H5Sset_extent_simple(aid, 1, &anelem, NULL);
        }
        else
        {
            aid = H5Screate(H5S_SCALAR);
        }
        atype = H5Tcopy(H5T_C_S1);
                H5Tset_size(atype, strlen(buff)+1);
                H5Tset_strpad(atype,H5T_STR_NULLTERM);
    }
    else
    {
      /*
       *Create dataspace for the first attribute.
        atype  = Ncltype2HDF5type(attnode->type);
       */
        atype  = H5Tcopy(Ncltype2HDF5type(attnode->type));
        if(attnode->n_elem > 1)
        {
            aid = H5Screate(H5S_SIMPLE);
            ret = H5Sset_extent_simple(aid, 1, &anelem, NULL);
        }
        else
        {
            aid = H5Screate(H5S_SCALAR);
        }
    }

  /*
   *Create attribute.
   */

    if(0 >= attnode->id)
    {
        tmpstr = NrmQuarkToString(attnode->name);
        attnode->id = H5Acreate2(did, tmpstr, atype, aid, H5P_DEFAULT, H5P_DEFAULT);
    }
    
  /*
   *Write attribute.
   */
    if(0 >= attnode->id)
    {
        if(NCL_string == attnode->type)
            ret = H5Awrite(attnode->id, atype, buff);
        else
            ret = H5Awrite(attnode->id, atype, attnode->value);
    }

  /*
   *Close attribute and datatype.
   */
    ret = H5Sclose(aid);
    ret = H5Tclose(atype);

  /*
   *Close the attributes.
   */
    if(0 >= attnode->id)
        ret = H5Aclose(attnode->id);
    
    return ret;
}

static NhlErrorTypes H5WriteVar(void *therec, NclQuark thevar, void *data,
                                 long *start, long *finish, long *stride)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    hid_t fid = -1;
    NclFileVarNode *varnode;
    NclFileDimNode *dimnode;
    NclFileAttNode *attnode;
    long count[NCL_MAX_DIMENSIONS];
    ng_size_t n_elem = 1;
    int i,j,n;
    int ret_code = 0;

    H5T_order_t h5order;
    herr_t status;

    hid_t did = 0;
    hid_t space = 0;
    hid_t type = 0;

    hid_t plist = H5P_DEFAULT;
    hsize_t rank = 1;
    hsize_t dims[NCL_MAX_DIMENSIONS];
    hsize_t chunk_dims[NCL_MAX_DIMENSIONS];

  /*
   *fprintf(stderr, "\nEnter H5WriteVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>\n", NrmQuarkToString(thevar));
   */

    if(grpnode->status > 0)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5WriteVar: Error to write read-only file (%s)",
            NrmQuarkToString(grpnode->path)));
        return(NhlFATAL);
    }

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, thevar);

    if(NULL == varnode)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5WriteVar: Error to write undefined variable (%s)",
            NrmQuarkToString(thevar)));
        return(NhlFATAL);
    }

    if(NULL != varnode->value)
    {
        NclFree(varnode->value);
        varnode->value = NULL;
    }

    if(grpnode->compress_level > 0)
    {
        if(varnode->compress_level < 1)
            varnode->compress_level = grpnode->compress_level;
    }

    rank = varnode->dim_rec->n_dims;
    dims[0] = 1;

    memset(dims, 0, NCL_MAX_DIMENSIONS * sizeof(hsize_t));

    for(i = 0; i < varnode->dim_rec->n_dims; i++)
    {
        count[i] = (long)floor((finish[i] - start[i])/(double)stride[i]) + 1;
        n_elem *= (ng_size_t)count[i];

        dimnode = &(varnode->dim_rec->dim_node[i]);
        if(dimnode->is_unlimited)
        {
            dimnode->size = MAX(finish[i] + 1, dimnode->size);
        }

        dims[i] = (hsize_t) (varnode->dim_rec->dim_node[i].size);
    }

    fid = (hid_t)_getH5grpID(grpnode);
                    
  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tfid = %d\n", fid);
   */

    if((NCL_list == varnode->type) || (NCL_vlen == varnode->type))
    {
        NclListObjList  *tmp = NULL;
        NclObj           tmpobj;
        NclVar           tmpvar;
        NclMultiDValData tmp_md;
        NclList          vlist    = (NclList)_NclGetObj(*(int *)data);

        hid_t            filetype;
        hid_t            memtype;

        hvl_t            *vlendata = (hvl_t *) NclCalloc(vlist->list.nelem,
                                                          sizeof(hvl_t));
        assert(vlendata);
                    
        tmp = vlist->list.first;
        for(n = 0; n < vlist->list.nelem; n++)
        { 
            tmpobj = (NclObj)_NclGetObj(tmp->obj_id);
            tmpvar = (NclVar)_NclGetObj(tmpobj->obj.id);
            tmp_md = (NclMultiDValData)_NclGetObj(tmpvar->var.thevalue_id);

            vlendata[n].len = 1;
            for(i = 0; i < tmp_md->multidval.n_dims; ++i)
                vlendata[n].len *= tmp_md->multidval.dim_sizes[i];

            i = vlendata[n].len * _NclSizeOf(tmp_md->multidval.data_type);
            vlendata[n].p = (void *)NclMalloc(i);
            memcpy(vlendata[n].p, tmp_md->multidval.val, i);
            tmp = tmp->next;
        }

      /*
       *Create variable-length datatype for file and memory.
       */
        filetype = H5Tvlen_create(h5memtype2filetype(Ncltype2HDF5type(tmp_md->multidval.data_type)));
        memtype  = H5Tvlen_create(Ncltype2HDF5type(tmp_md->multidval.data_type));

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tNcltype2HDF5type(tmp_md->multidval.data_type) = %d\n",
       *                   Ncltype2HDF5type(tmp_md->multidval.data_type));
       *fprintf(stderr, "\tfiletype = %d, memtype  = %d\n", filetype, memtype);
       *fprintf(stderr, "\tH5T_STD_I32LE = %d, h5memtype2filetype(memtype)  = %d\n",
       *                   H5T_STD_I32LE, h5memtype2filetype(Ncltype2HDF5type(tmp_md->multidval.data_type)));
       *fprintf(stderr, "\tH5T_NATIVE_INT = %d, Ncltype2HDF5type(tmp_md->multidval.data_type) = %d\n",
       *                   H5T_NATIVE_INT, Ncltype2HDF5type(tmp_md->multidval.data_type));
       */
      /*
       *Create dataspace.  Setting maximum size to NULL sets the maximum
       *size to be the current size.
       */
        space = H5Screate_simple (rank, dims, NULL);

        if(1 == grpnode->open)
        {
            fid = grpnode->fid;
        }
        else
        {
            fid = H5Fopen(NrmQuarkToString(grpnode->path), H5F_ACC_RDWR, H5P_DEFAULT);
            grpnode->fid = fid;
            grpnode->gid = fid;
            grpnode->open = 1;
        }

        if(0 > varnode->id)
        {
          /*
           *Create the dataset and write the variable-length data to it.
           */
            did = H5Dcreate(fid, NrmQuarkToString(varnode->name), filetype, space,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            varnode->id = did;

          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tdid: %d\n", did);
           */

            status = H5Dwrite(did, memtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, vlendata);

            if(0 != status)
            {
                fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
            }

          /*
           *Close and release resources.  Note the use of H5Dvlen_reclaim
           *removes the need to manually free() the previously malloc'ed
           *data.
           */

            status = H5Dvlen_reclaim(memtype, space, H5P_DEFAULT, vlendata);

          /*
           *status = H5Dclose(did);
           */
        }

        status = H5Sclose(space);
        status = H5Tclose(filetype);
        status = H5Tclose(memtype);

        NclFree(vlendata);
    }
    else if(NCL_enum & varnode->type)
    {
        size_t size = 1;
        unsigned int mv;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tWrite enum.\n");
       */

        NclFileEnumRecord *enumrec  = (NclFileEnumRecord *) varnode->udt;
        NclFileEnumNode   *enumnode = NULL;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tsize = %d\n", size);
       */

        size = sizeof(mv);

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tsize = %d\n", size);
       */

        space = H5Screate_simple(rank, dims, NULL);
        type  = H5Tcreate(H5T_ENUM, size);

        if(varnode->chunk_dim_rec)
        {
            for(j = 0; j < varnode->chunk_dim_rec->n_dims; j++)
            {
                chunk_dims[j] = (hsize_t) (varnode->chunk_dim_rec->dim_node[j].size);
            }
            plist  = H5Pcreate(H5P_DATASET_CREATE);
            status = H5Pset_chunk(plist, rank, chunk_dims);
        }

        if(varnode->compress_level > 0)
            status = H5Pset_deflate(plist, varnode->compress_level);

        for(j = 0; j < enumrec->n_enums; ++j)
        {
            enumnode = &(enumrec->enum_node[j]);
            mv = (unsigned int) enumnode->value;
          /*
           *fprintf(stderr, "\tNo %d: name: <%s>, value: %d\n",
           *                   j, NrmQuarkToString(enumnode->name), mv);
           */
            status = H5Tenum_insert(type, NrmQuarkToString(enumnode->name),
                                          &mv);
        }

        did = H5Dcreate(fid, NrmQuarkToString(varnode->name),
                        type, space, H5P_DEFAULT, plist, H5P_DEFAULT);

        varnode->id = did;

        if(did > 0)
        {
            unsigned int *ip;
            unsigned char *cptr = (unsigned char *)data;

            size = 1;
            for(j = 0; j < varnode->dim_rec->n_dims; ++j)
            {
                size *= varnode->dim_rec->dim_node[j].size;
            }
            ip = (unsigned int *)NclCalloc(size, sizeof(unsigned int));

          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tsize = %d\n", size);
           */

            for(j = 0; j < size; ++j)
            {
                ip[j] = (unsigned int)cptr[j];
            }
            status = H5Dwrite(did, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, ip);
          /*Do not close the did, as we need to write attributes later
           *H5Dclose(did);
           */

            NclFree(ip);
        }
        else
        {
            ret_code = FAILED;
        }

        H5Sclose(space);
        H5Tclose(type);
    }
    else if(NCL_opaque & varnode->type)
    {
        size_t size = 1;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tWrite opaque.\n");
       */

        NclFileUDTRecord *udtrec  = (NclFileUDTRecord *) varnode->udt;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tsize = %d\n", size);
       */

        size = udtrec->udt_node[0].size;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tsize = %d\n", size);
       */

        type   = H5Tcreate(H5T_OPAQUE, size);
        status = H5Tset_tag(type, "Opaque: character array");

        space = H5Screate_simple(rank, dims, NULL);

        if(varnode->chunk_dim_rec)
        {
            for(j = 0; j < varnode->chunk_dim_rec->n_dims; j++)
            {
                chunk_dims[j] = (hsize_t) (varnode->chunk_dim_rec->dim_node[j].size);
            }
            plist  = H5Pcreate(H5P_DATASET_CREATE);
            status = H5Pset_chunk(plist, rank, chunk_dims);
        }

        if(varnode->compress_level > 0)
            status = H5Pset_deflate(plist, varnode->compress_level);

        did = H5Dcreate(fid, NrmQuarkToString(varnode->name),
                        type, space, H5P_DEFAULT, plist, H5P_DEFAULT);

        varnode->id = did;

        if(did > 0)
        {
            char *cptr = (char *)data;

            status = H5Dwrite(did, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, cptr);
          /*
           *H5Dclose(did);
           */
        }
        else
        {
            ret_code = FAILED;
        }

        H5Sclose(space);
        H5Tclose(type);
    }
    else if(NCL_string == varnode->type)
    {
        char **tmpstr = (char **)NclCalloc(n_elem, sizeof(char *));
        NclQuark *qd = (NclQuark *)data;

        rank = varnode->dim_rec->n_dims;
      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
       *fprintf(stderr, "\trank = %d\n", rank);
       */

        n = 0;
        for(i = 0; i < varnode->dim_rec->n_dims; ++i)
        {
            dimnode = &(varnode->dim_rec->dim_node[i]);
            dims[i] = dimnode->size;

            for(j = 0; j < dimnode->size; ++j)
            {
                tmpstr[n] = NrmQuarkToString(qd[n]);

                ++n;
            }
        }

        space = H5Screate_simple(rank, dims, NULL);

        type  = H5Tcopy(H5T_C_S1);
        status = H5Tset_size(type, H5T_VARIABLE);

      /*
       *if(varnode->chunk_dim_rec)
       *{
       *    for(j = 0; j < varnode->chunk_dim_rec->n_dims; j++)
       *    {
       *        chunk_dims[j] = (hsize_t) (varnode->chunk_dim_rec->dim_node[j].size);
       *    }
       *    plist  = H5Pcreate(H5P_DATASET_CREATE);
       *    status = H5Pset_chunk(plist, rank, chunk_dims);
       *}

       *if(varnode->compress_level > 0)
       *    status = H5Pset_deflate(plist, varnode->compress_level);
       *else
       *    status = H5Pset_deflate(plist, varnode->compress_level);
       */

        did = H5Dcreate(fid, NrmQuarkToString(varnode->name),
                        type, space, H5P_DEFAULT, plist, H5P_DEFAULT);

        varnode->id = did;

        if(did > 0)
        {
            status = H5Dwrite(did, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmpstr);

            H5Sclose(space);
            H5Tclose(type);
          /*
           *H5Dclose(did);
           */
        }
        else
        {
            ret_code = FAILED;
        }

        NclFree(tmpstr);
    }
    else if(NCL_compound == varnode->type)
    {
        hid_t  tid = -1;
        size_t data_size = 1;
        void *data_value = NULL;
        int  *obj_id = (int *)data;

        NclMultiDValData theval = NULL;

        NclList comp_list;
        NclListObjList *step = NULL;
        NclVar cur_var = NULL;

        NclFileCompoundRecord *comp_rec = varnode->comprec;
        NclFileCompoundNode   *compnode = NULL;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tfound node for var: <%s>, thevar: <%s>, rank = %d\n",
       *                   NrmQuarkToString(varnode->name), NrmQuarkToString(thevar), rank);
       */

        data_size = 1;
        for(n = 0; n < rank; ++n)
        {
            dimnode = &(varnode->dim_rec->dim_node[n]);
            data_size *= (size_t) dimnode->size;
        }

        if(NULL != comp_rec)
        {
            size_t cur_mem_loc = 0;
            size_t compound_size = 0;
            size_t *mem_len = (size_t *)NclCalloc(comp_rec->n_comps, sizeof(size_t));
            if (! mem_len)
            {
                NHLPERROR((NhlFATAL,ENOMEM,NULL));
                return NhlFATAL;
            }

            if(comp_rec->n_comps)
            {
                for(n = 0; n < comp_rec->n_comps; n++)
                {
                    compnode = &(comp_rec->compnode[n]);

                    mem_len[n] = ((size_t) _NclSizeOf(compnode->type)
                                 *(size_t) compnode->nvals);
                    compound_size += mem_len[n];
                }

                space = H5Screate_simple(rank, dims, NULL);
                tid = H5Tcreate(H5T_COMPOUND, compound_size);

                cur_mem_loc = 0;
                for(n = 0; n < comp_rec->n_comps; ++n)
                {
                    compnode = &(comp_rec->compnode[n]);

                    H5Tinsert(tid, NrmQuarkToString(compnode->name), cur_mem_loc,
                              Ncltype2HDF5type(compnode->type));

                  /*
                   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
                   *fprintf(stderr, "\tComponent %d name: <%s>, type: <%s>, leng = %d\n",
                   *                   n, NrmQuarkToString(compnode->name),
                   *                   _NclBasicDataTypeToName(compnode->type),
                   *                   mem_len[n]);
                   */

                    cur_mem_loc += mem_len[n];
                }
            }
            else
            {
                NHLPERROR((NhlFATAL,ENOMEM,NULL));
                return NhlFATAL;
            }

            data_value = (void *)NclCalloc((ng_usize_t)(data_size*compound_size), sizeof(void));
            assert(data_value);

            cur_mem_loc = 0;
            for(i = 0; i < data_size; ++i)
            {
                comp_list = (NclList)_NclGetObj(obj_id[i]);
                step = comp_list->list.last;
                for(n = 0; n < comp_rec->n_comps; ++n)
                {
                    cur_var = (NclVar)_NclGetObj(step->obj_id);
                    if(Ncl_Var == cur_var->obj.obj_type)
                    {
                        theval = (NclMultiDValData)_NclGetObj(cur_var->var.thevalue_id);
                        memcpy(data_value + cur_mem_loc,
                                   theval->multidval.val, mem_len[n]);

                        cur_mem_loc += mem_len[n];
                    }
                    else
                    {
                        fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
                        fprintf(stderr, "\tUnknown cur_var->obj.obj_type: 0%x\n", cur_var->obj.obj_type);
                    }

                    step = step->prev;
                }
            }

            did = H5Dcreate(fid, NrmQuarkToString(varnode->name), tid, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            varnode->gid = fid;
            varnode->id  = did;

            status = H5Dwrite(did, tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_value);

            H5Tclose(tid);
            H5Sclose(space);
          /*
           *H5Dclose(did);
           */

            NclFree(data_value);
        }
    }
    else
    {
        space = H5Screate_simple(rank, dims, NULL);
        type  = H5Tcopy(Ncltype2HDF5type(varnode->type));

        if(varnode->chunk_dim_rec)
        {
            for(j = 0; j < varnode->chunk_dim_rec->n_dims; j++)
            {
                chunk_dims[j] = (hsize_t) (varnode->chunk_dim_rec->dim_node[j].size);
            }
            plist  = H5Pcreate(H5P_DATASET_CREATE);
            status = H5Pset_chunk(plist, rank, chunk_dims);
        }

        h5order = H5Tget_order(Ncltype2HDF5type(varnode->type));
        status = H5Tset_order(type, h5order);
 
        if(varnode->compress_level > 0)
            status = H5Pset_deflate(plist, varnode->compress_level);
      /*
       *else
       *    status = H5Pset_deflate(plist, varnode->compress_level);
       */

        if(0 > varnode->id)
        {
            did = H5Dcreate(fid, NrmQuarkToString(varnode->name),
                             type, space, H5P_DEFAULT, plist, H5P_DEFAULT);
                    
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tdid = %d\n", did);
           */
    
            varnode->id = did;
        }
        else
            did = varnode->id;

        if(did > 0)
        {
            status = H5Dwrite(did, Ncltype2HDF5type(varnode->type),
                              H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

            H5Sclose(space);
            H5Tclose(type);
          /*
           *H5Dclose(did);
           */
        }
        else
        {
            ret_code = FAILED;
            NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                      "H5WriteVar: Error ocurred while writing as did = %d\n", did));
            return(NhlFATAL);
        }
    }

    if(NULL != varnode->att_rec)
    {
        if(0 > varnode->id)
            did = H5Dopen(fid, NrmQuarkToString(varnode->name), H5P_DEFAULT);
        else
            did = varnode->id;

        varnode->att_rec->gid = fid;
        varnode->att_rec->id  = did;

        for(i = 0; i < varnode->att_rec->n_atts; i++)
        {
            attnode = &(varnode->att_rec->att_node[i]);
          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n\n", __FILE__, __LINE__);
           *fprintf(stderr, "\twrite att no %d: <%s>\n",
           *                   i, NrmQuarkToString(attnode->name));
           */

            ret_code = _writeH5variableAttribute(did, attnode);

            if(ret_code)
            {
                NHLPERROR((NhlFATAL,NhlEUNKNOWN,"H5WriteVar: Error writing variable attribute\n"));
                return(NhlFATAL);
            }
        }

      /*
       *H5Dclose(did);
       */
    }

  /*
   *fprintf(stderr, "Leave H5WriteVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    if(ret_code)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,"H5WriteVar: Error ocurred while writing can't continue"));
        return(NhlFATAL);
    }
    else
    {
        return(NhlNOERROR);
    }
}

NhlErrorTypes H5AddCompound(void *rec, NclQuark compound_name, NclQuark var_name,
                             ng_size_t n_dims, NclQuark *dim_name, ng_size_t n_mems,
                             NclQuark *mem_name, NclQuark *mem_type, int *mem_size)
{
    NclFileGrpNode   *grpnode = (NclFileGrpNode *) rec;
    NclFileDimNode   *dimnode = NULL;
    NclFileVarNode   *varnode = NULL;
    int n = -1;
    NhlErrorTypes ret = NhlNOERROR;

    NclQuark *udt_mem_name = NULL;
    NclBasicDataTypes *udt_mem_type = NULL;

    ng_size_t *dim_size = NULL;
    long *long_dim_size = NULL;

    size_t compound_length = 0;
    size_t component_size  = 4;
    size_t *mem_offset = NULL;

  /*
   *fprintf(stderr, "\nEnter H5AddCompound, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tcompound_name: <%s>, var_name: <%s>, n_dims = %d, dim_name[0]: <%s>\n",
   *                 NrmQuarkToString(compound_name), NrmQuarkToString(var_name),
   *                 n_dims, NrmQuarkToString(dim_name[0]));
   */

    udt_mem_name = (NclQuark *)NclCalloc(n_mems, sizeof(NclQuark));
    assert(udt_mem_name);
    udt_mem_type = (NclBasicDataTypes *)NclCalloc(n_mems, sizeof(NclBasicDataTypes));
    assert(udt_mem_type);

    mem_offset = (size_t *)NclCalloc(n_mems, sizeof(size_t));
    assert(mem_offset);

    dim_size = (ng_size_t *)NclCalloc(n_dims, sizeof(ng_size_t));
    assert(dim_size);

    long_dim_size = (long *)NclCalloc(n_dims, sizeof(long));
    assert(long_dim_size);

    for(n = 0; n < n_mems; n++)
    {
        udt_mem_name[n] = mem_name[n];
        udt_mem_type[n] = _nameToNclBasicDataType(mem_type[n]);

        if(n)
           mem_offset[n] = mem_offset[n-1] + component_size;
        else
           mem_offset[n] = 0;

        if(mem_size[n] < 1)
            mem_size[n] = 1;

        component_size = get_sizeof(mem_size[n], _NclSizeOf(udt_mem_type[n]));
        compound_length += component_size;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tmem_size[%d] = %d\n", n, mem_size[n]);
       *fprintf(stderr, "\tmem: %d, name: <%s>, type: <%s>, size: %ld, offset: %ld\n",
       *                 n, NrmQuarkToString(mem_name[n]), NrmQuarkToString(mem_type[n]),
       *                 (long)component_size, (long)mem_offset[n]);
       */
    }

  /*
   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tcompound_length = %d\n", compound_length);

   *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgrpnode->gid = %d\n", grpnode->gid);
   *fprintf(stderr, "\tcompound_name: <%s>\n", NrmQuarkToString(compound_name));
   */

    _Ncl_add_udt(&(grpnode->udt_rec),
                 grpnode->fid, -1, compound_name,
                 NCL_compound, NCL_compound,
                 compound_length, n_mems, udt_mem_name, udt_mem_type);

    for(n = 0; n < n_dims; n++)
    {
        dimnode = _getDimNodeFromNclFileGrpNode(grpnode, dim_name[n]);
        dim_size[n] = dimnode->size;
        long_dim_size[n] = (long) dimnode->size;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tdim %d, name: <%s>, size: %d\n",
       *                 n, NrmQuarkToString(dim_name[n]), dim_size[n]);
       */
    }

    _addNclVarNodeToGrpNode(grpnode, var_name, -1, NCL_compound,
                                    n_dims, dim_name, dim_size);

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, var_name);

    if(NULL != varnode)
    {
        NclFileCompoundRecord *comp_rec = _NclFileCompoundAlloc(n_mems);
        NclFileCompoundNode   *compnode = NULL;

        comp_rec->name = compound_name;
        comp_rec->size = compound_length;
        comp_rec->type = NCL_compound;
        comp_rec->xtype = -1;

        for(n = 0; n < n_mems; n++)
        {
            compnode = &(comp_rec->compnode[n]);

            compnode->name = udt_mem_name[n];
            compnode->type = udt_mem_type[n];
            compnode->offset = mem_offset[n];
            compnode->rank = 1;
            compnode->nvals = mem_size[n];
            compnode->sides = NULL;
            compnode->value = NULL;
        }

        varnode->comprec = comp_rec;
    }

    NclFree(udt_mem_name);
    NclFree(udt_mem_type);
    NclFree(mem_offset);
    NclFree(dim_size);
    NclFree(long_dim_size);

  /*
   *fprintf(stderr, "Leave H5AddCompound, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

NhlErrorTypes H5WriteCompound(void *rec, NclQuark compound_name, NclQuark var_name,
                               ng_size_t n_mems, NclQuark *mem_name, NclList thelist)
{
    NclFileGrpNode   *grpnode = (NclFileGrpNode *) rec;
    NclFileDimNode   *dimnode = NULL;
    NclFileVarNode   *varnode = NULL;

    NhlErrorTypes ret = NhlNOERROR;

    int  n = -1;
    int  n_dims = 1;

    hid_t  fid = 0;
    hid_t  did;
    hid_t  tid;
    hid_t  space;
    herr_t status;

    size_t  data_size = 1;
    void    *data_value = NULL;
    hsize_t *dim_size = NULL;

  /*
   *fprintf(stderr, "\nEnter H5WriteCompound, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tcompound_name: <%s>, var_name: <%s>, n_mems = %ld, mem_name[0]: <%s>\n",
   *                 NrmQuarkToString(compound_name), NrmQuarkToString(var_name),
   *                 n_mems, NrmQuarkToString(mem_name[0]));
   */

    fid = (hid_t)_getH5grpID(grpnode);

    varnode = _getVarNodeFromNclFileGrpNode(grpnode, var_name);

    if((NULL != varnode) && (NULL != thelist))
    {
        NclListObjList *list_list = thelist->list.first;
        NclList  comp_list;
        NclListObjList *tmp_list;

        NclVar self = NULL;
        NclMultiDValData theval = NULL;

        NclFileCompoundRecord *comp_rec = varnode->comprec;
        NclFileCompoundNode   *compnode = NULL;

        n_dims = varnode->dim_rec->n_dims;

      /*
       *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
       *fprintf(stderr, "\tfound node for var: <%s>, var_name: <%s>, n_dims = %d\n",
       *                   NrmQuarkToString(varnode->name), NrmQuarkToString(var_name), n_dims);
       */

        dim_size = (void *)NclCalloc(n_dims, sizeof(hsize_t));
        assert(dim_size);

        data_size = 1;
        for(n = 0; n < n_dims; n++)
        {
            dimnode = &(varnode->dim_rec->dim_node[n]);
            data_size *= (size_t) dimnode->size;
            dim_size[n] = (size_t) dimnode->size;
        }

        if(NULL != comp_rec)
        {
            size_t cur_mem_loc = 0;
            size_t compound_size = 0;
            int current_component = 0;

            size_t *mem_len = (size_t *)NclCalloc(n_mems, sizeof(size_t));
            if (! mem_len)
            {
                NHLPERROR((NhlFATAL,ENOMEM,NULL));
                return NhlFATAL;
            }

            space = H5Screate_simple(n_dims, dim_size, NULL);

            for(n = 0; n < n_mems; n++)
            {
                compnode = &(comp_rec->compnode[n]);

                mem_len[n] = (size_t) _NclSizeOf(compnode->type)
                           * (size_t) compnode->nvals;

                compound_size += mem_len[n];
            }

            tid = H5Tcreate(H5T_COMPOUND, compound_size);

            cur_mem_loc = 0;
            for(n = 0; n < n_mems; n++)
            {
                compnode = &(comp_rec->compnode[n]);

                H5Tinsert(tid, NrmQuarkToString(compnode->name), cur_mem_loc,
                          Ncltype2HDF5type(compnode->type));

              /*
               *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
               *fprintf(stderr, "\tComponent %d name: <%s>, type: <%s>, leng = %d\n",
               *                   n, NrmQuarkToString(compnode->name),
               *                   _NclBasicDataTypeToName(compnode->type),
               *                   mem_len[n]);
               */

                cur_mem_loc += mem_len[n];
            }

          /*
           *fprintf(stderr, "\tfile: %s, line: %d\n", __FILE__, __LINE__);
           *fprintf(stderr, "\tdata_size = %d, compound_size = %d\n",
           *                   data_size, compound_size);
           */

            data_value = (void *)NclCalloc((ng_usize_t)(data_size*compound_size), sizeof(void));
            assert(data_value);

            cur_mem_loc = 0;
            while(NULL != list_list)
            {
                current_component = 0;

                self = (NclVar)_NclGetObj(list_list->obj_id);
                if(self != NULL)
                {
                    theval = (NclMultiDValData)_NclGetObj(self->var.thevalue_id);
                    comp_list = (NclList)_NclGetObj(*(int*)theval->multidval.val);
                    tmp_list = comp_list->list.last;

                    while(NULL != tmp_list)
                    {
                        self = (NclVar)_NclGetObj(tmp_list->obj_id);
                        if(self != NULL)
                        {
                            theval = (NclMultiDValData)_NclGetObj(self->var.thevalue_id);

                            memcpy(data_value + cur_mem_loc,
                                   theval->multidval.val, mem_len[current_component]);

                            cur_mem_loc += mem_len[current_component];
                        }
                        tmp_list = tmp_list->prev;
                        current_component++;
                    }
                }

                list_list = list_list->next;
            }

            did = H5Dcreate(fid, NrmQuarkToString(varnode->name), tid, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            varnode->gid = fid;
            varnode->id  = did;

            status = H5Dwrite(did, tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_value);

            if(0 != status)
            {
                fprintf(stderr, "\nError in file: %s, line: %d\n", __FILE__, __LINE__);
            }

            H5Tclose(tid);
            H5Sclose(space);

          /*
           *H5Dclose(did);
           */

            NclFree(data_value);
        }
        NclFree(dim_size);
    }

  /*
   *fprintf(stderr, "Leave H5WriteCompound, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

void _H5_add_udt(NclFileUDTRecord **rootudtrec,
                 int gid, int uid, NclQuark name,
                 int ncl_class, size_t base_type,
                 size_t size, size_t nfields,
                 NclQuark *mem_name, NclBasicDataTypes *mem_type)
{
    NclFileUDTRecord *udtrec = *rootudtrec;
    NclFileUDTNode   *udtnode;
    int n = 0;

  /*
   *fprintf(stderr, "\nEnter _H5_add_udt, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgid: %d, uid: %d, name: <%s>\n", gid, uid, NrmQuarkToString(name));
   */

    if(NULL == udtrec)
    {
        udtrec = _NclFileUDTAlloc(1);
        assert(udtrec);
        *rootudtrec = udtrec;

        udtrec->gid = gid;
        udtrec->uid = uid;
        udtrec->n_udts = 0;
    }
  
    if(udtrec->n_udts >= udtrec->max_udts)
    {
        _NclFileUDTRealloc(udtrec);
    }

    udtnode = &(udtrec->udt_node[udtrec->n_udts]);

    udtnode->id = uid;
    udtnode->name = name;
    udtnode->type = base_type;
    udtnode->size = size;
    udtnode->ncl_class = ncl_class;
    udtnode->max_fields = nfields;
    udtnode->n_fields = nfields;

    udtnode->mem_name = (NclQuark *)NclCalloc(nfields, sizeof(NclQuark));
    assert(udtnode->mem_name);
    udtnode->mem_type = (NclBasicDataTypes *)NclCalloc(nfields, sizeof(NclBasicDataTypes));
    assert(udtnode->mem_type);

    for(n = 0; n < nfields; n++)
    {
        udtnode->mem_name[n] = mem_name[n];
        udtnode->mem_type[n] = mem_type[n];
    }

    udtrec->n_udts ++;
  /*
   *fprintf(stderr, "\tudtrec->n_udts = %d\n", udtrec->n_udts);
   *fprintf(stderr, "Leave _H5_add_udt, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

NclFileVarNode *H5AddEnumVar(void* therec, NclQuark thevar,
                                  nc_type enum_type_id, int n_dims,
                                  NclQuark *dim_names, long *dim_sizes,
                                  NclBasicDataTypes ncl_type)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode = NULL;
    int i,j;
    int dim_ids[MAX_NC_DIMS];

  /*
   *fprintf(stderr, "\nEnter H5AddEnumVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, enum_type_id: %d, n_dims = %d\n",
   *                   NrmQuarkToString(thevar),
   *                   enum_type_id, n_dims);
   */
    if(0 < grpnode->status)
    {    
        NhlPError(NhlFATAL,NhlEUNKNOWN,
            "H5AddEnumVar: File (%s) was opened as a read only file, can not write to it",
             NrmQuarkToString(grpnode->path));

        return NULL;
    }

    memset(dim_ids, 0, MAX_NC_DIMS * sizeof(int));

    dim_ids[0] = -999;
    for(i = 0; i < n_dims; i++)
    {
        for(j = 0; j < grpnode->dim_rec->n_dims; j++)
        {
            if(grpnode->dim_rec->dim_node[j].name == dim_names[i])
            {
                if(NrmStringToQuark("ncl_scalar") == dim_names[i])
                {
                    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                        "H5AddEnumVar: the reserved file dimension name \"ncl_scalar\" was used\n%s\n",
                                  "\t\tin a value with more than one dimension, can not add variable"));
                    return (NULL);
                }
                dim_ids[i] = grpnode->dim_rec->dim_node[j].id;
                break;
            }
        }
    } 

    if (dim_ids[0] == -999)
    {
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5AddEnumVar: internal error adding variable"));
        return NULL;
    }

    _addNclVarNodeToGrpNode(grpnode, thevar, -1, ncl_type,
                            n_dims, dim_names, dim_sizes);

    i = grpnode->var_rec->n_vars - 1;
    varnode = &(grpnode->var_rec->var_node[i]);
    varnode->gid  = grpnode->fid;
    varnode->id   =  -1;
    varnode->type = (NCL_enum | ncl_type);
    varnode->udt  = NULL;

    for(i = 0 ; i < n_dims; i++)
    {
        varnode->dim_rec->dim_node[i].id = dim_ids[i];
    }

  /*
   *fprintf(stderr, "\tthevar: <%s>\n", NrmQuarkToString(thevar));
   *fprintf(stderr, "Leave H5AddEnumVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return varnode;
}

NhlErrorTypes H5AddEnum(void *rec, NclQuark enum_name, NclQuark var_name,
                        NclQuark dim_name, NclQuark *mem_name, void *mem_value,
                        ng_size_t n_mems, NclBasicDataTypes val_type)
{
    NclFileGrpNode *rootgrpnode = (NclFileGrpNode *) rec;
    NhlErrorTypes ret = NhlNOERROR;
    NclFileDimNode   *dimnode;
    NclFileVarNode   *varnode;

    NclQuark          udt_mem_name[1];
    NclBasicDataTypes udt_mem_type[1];

    int      n = 0;
    int      n_dims = 1;
    NclQuark dim_names[1];
    long     dim_sizes[1];

    NclFileEnumRecord *enumrec;

    unsigned char *mv = (unsigned char *) mem_value;

  /*
   *fprintf(stderr, "\nEnter H5AddEnum, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tenum_name: <%s>, var_name: <%s>, dim_name: <%s>\n",
   *                 NrmQuarkToString(enum_name), NrmQuarkToString(var_name),
   *                 NrmQuarkToString(dim_name));
   */

    udt_mem_name[0] = enum_name;
    udt_mem_type[0] = val_type;

    _H5_add_udt(&(rootgrpnode->udt_rec),
                  rootgrpnode->fid, -1, enum_name,
                  NCL_enum, NCL_enum,
                  0, 1, udt_mem_name, udt_mem_type);

    dimnode = _getDimNodeFromNclFileGrpNode(rootgrpnode, dim_name);
    dim_names[0] = dim_name;
    dim_sizes[0] = (long) dimnode->size;

    varnode = H5AddEnumVar(rec, var_name, -1, n_dims, dim_names, dim_sizes, val_type);

    enumrec = _NclFileEnumAlloc(n_mems);

    enumrec->type = val_type;
    enumrec->name = enum_name;
    enumrec->size = 0;
    enumrec->xtype = val_type;
    enumrec->values = NULL;

    for(n = 0; n < n_mems; ++n)
    {
      /*
       *fprintf(stderr, "\tadd mem %d. name: <%s>, value = %d\n",
       *                   n, NrmQuarkToString(mem_name[n]), mv[n]);
       */
        _addNclEnumNode(&enumrec, mem_name[n], (long long)mv[n]);
    }

    varnode->udt = (void *)enumrec;

  /*
   *fprintf(stderr, "\tdim_sizes[0] = %ld\n", dim_sizes[0]);
   *fprintf(stderr, "Leave H5AddEnum, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

static NhlErrorTypes H5AddOpaqueVar(void* therec, NclQuark thevar,
                                    int n_dims, NclQuark *dim_names, long *dim_sizes)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode = NULL;
    int i, j;

  /*
   *fprintf(stderr, "\nEnter H5AddOpaqueVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, n_dims = %d\n",
   *                   NrmQuarkToString(thevar), n_dims);
   */

    if(0 < grpnode->status)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5AddOpaqueVar: File (%s) was opened as a read only file, %s\n",
            "can not write to it",
             NrmQuarkToString(grpnode->path)));

        return(NhlFATAL);
    }

    _addNclVarNodeToGrpNode(grpnode, thevar, -1, NCL_ubyte,
                            n_dims, dim_names, dim_sizes);

    i = grpnode->var_rec->n_vars - 1;
    varnode = &(grpnode->var_rec->var_node[i]);
    varnode->gid = grpnode->fid;
    for(i = 0 ; i < n_dims; i++)
    {
        varnode->dim_rec->dim_node[i].id = -999;
        for(j = 0; j < grpnode->dim_rec->n_dims; j++)
        {
            if(grpnode->dim_rec->dim_node[j].name == dim_names[i])
            {
                if(NrmStringToQuark("ncl_scalar") == dim_names[i])
                {
                    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                        "H5AddOpaqueVar: the reserved file dimension name \"ncl_scalar\" was used %s\n",
                        "in a value with more than one dimension, can not add variable"));
                    return(NhlFATAL);
                }

                varnode->dim_rec->dim_node[i].id = grpnode->dim_rec->dim_node[j].id;
                break;
            }
        }
    } 

    varnode->udt = (void *) grpnode->udt_rec;
    varnode->type = (varnode->type | NCL_opaque);

  /*
   *fprintf(stderr, "Leave H5AddOpaqueVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return(NhlNOERROR);
}

NhlErrorTypes H5AddOpaque(void *rec, NclQuark opaque_name, NclQuark var_name,
                           int var_size, NclQuark dim_name)
{
    NclFileGrpNode *rootgrpnode = (NclFileGrpNode *) rec;
    NhlErrorTypes ret = NhlNOERROR;
    NclFileDimNode   *dimnode;
    int n_dims = 1;

    NclQuark          mem_name[1] = {opaque_name};
    NclBasicDataTypes mem_type[1] = {NCL_ubyte};

    NclQuark dim_names[1] = {dim_name};
    long     dim_sizes[1] = {0};

  /*
   *fprintf(stderr, "\nEnter H5AddOpaque, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\topaque_name: <%s>, var_name: <%s>, var_size: %d, dim_name: <%s>\n",
   *                 NrmQuarkToString(opaque_name), NrmQuarkToString(var_name),
   *                 var_size, NrmQuarkToString(dim_name));
   */

    mem_name[0] = opaque_name;
    mem_type[0] = NCL_ubyte;

    _H5_add_udt(&(rootgrpnode->udt_rec),
                  rootgrpnode->fid, -1, opaque_name,
                  NC_OPAQUE, NC_UBYTE,
                  var_size, 1, mem_name, mem_type);

    dimnode = _getDimNodeFromNclFileGrpNode(rootgrpnode, dim_name);
    dim_sizes[0] = (long) dimnode->size;
    ret =  H5AddOpaqueVar(rec, var_name, n_dims, dim_names, dim_sizes);

  /*
   *fprintf(stderr, "\tdim_sizes[0]= %ld\n", dim_sizes[0]);
   *fprintf(stderr, "Leave H5AddOpaque, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

static NhlErrorTypes H5AddVlenVar(void* therec, NclQuark thevar,
                                  ng_size_t n_dims, NclQuark *dim_names, long *dim_sizes)
{
    NclFileGrpNode *grpnode = (NclFileGrpNode *)therec;
    NclFileVarNode *varnode = NULL;
    int i,j;

  /*
   *fprintf(stderr, "\nEnter H5AddVlenVar, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tthevar: <%s>, n_dims = %d\n",
   *                   NrmQuarkToString(thevar), n_dims);
   */

    if(0 < grpnode->status)
    {    
        NHLPERROR((NhlFATAL,NhlEUNKNOWN,
            "H5AddVlenVar: File (%s) was opened as a read only file, can not write to it",
             NrmQuarkToString(grpnode->path)));
        return(NhlFATAL);
    }

    _addNclVarNodeToGrpNode(grpnode, thevar, -1, NCL_vlen,
                            n_dims, dim_names, dim_sizes);

    i = grpnode->var_rec->n_vars - 1;
    varnode = &(grpnode->var_rec->var_node[i]);
    varnode->gid = grpnode->fid;
    for(i = 0 ; i < n_dims; i++)
    {
        for(j = 0; j < grpnode->dim_rec->n_dims; j++)
        {
            if(grpnode->dim_rec->dim_node[j].name == dim_names[i])
            {
                if(NrmStringToQuark("ncl_scalar") == dim_names[i])
                {
                    NHLPERROR((NhlFATAL,NhlEUNKNOWN,
                        "H5AddVlenVar: the reserved file dimension name \"ncl_scalar\" was used %s\n",
                        "in a value with more than one dimension, can not add variable"));
                    return(NhlFATAL);
                }
                varnode->dim_rec->dim_node[i].id = grpnode->dim_rec->dim_node[j].id;
                break;
            }
        }
    }

  /*
   *fprintf(stderr, "\tthevar: <%s>, id: var_id = %d\n", 
   *                   NrmQuarkToString(thevar), varnode->id);
   *fprintf(stderr, "Leave H5AddVlenVar, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return(NhlNOERROR);
}

NhlErrorTypes H5AddVlen(void *rec, NclQuark vlen_name, NclQuark var_name,
                         NclQuark type, NclQuark *dim_names, ng_size_t n_dims)
{
    NclFileGrpNode *rootgrpnode = (NclFileGrpNode *) rec;
    NhlErrorTypes ret = NhlNOERROR;
    NclFileDimNode   *dimnode;

    NclBasicDataTypes ncl_type;

    NclQuark          mem_name[1];
    NclBasicDataTypes mem_type[1];

    long *dim_sizes;
    ng_size_t n = 0;

  /*
   *fprintf(stderr, "\nEnter H5AddVlen, file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tvlen_name: <%s>, var_name: <%s>, type: <%s>, dim_name: <%s>\n",
   *                 NrmQuarkToString(vlen_name), NrmQuarkToString(var_name),
   *                 NrmQuarkToString(type), NrmQuarkToString(dim_name));
   */

    ncl_type = _nameToNclBasicDataType(type);

    mem_name[0] = vlen_name;
    mem_type[0] = ncl_type;

    _H5_add_udt(&(rootgrpnode->udt_rec),
                  rootgrpnode->fid, -1, vlen_name,
                  NCL_vlen, NCL_vlen,
                  0, 1, mem_name, mem_type);

    dim_sizes = (long *)NclCalloc(n_dims, sizeof(long));
    assert(dim_sizes);

    for(n = 0; n < n_dims; ++n)
    {
        dimnode = _getDimNodeFromNclFileGrpNode(rootgrpnode, dim_names[n]);
        dim_sizes[n] = (long) dimnode->size;
    }

    ret = H5AddVlenVar(rec, var_name, n_dims, dim_names, dim_sizes);

  /*
   *fprintf(stderr, "\tdim_sizes[0]= %d\n", dim_sizes[0]);
   *fprintf(stderr, "Leave H5AddVlen, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
    return ret;
}

void _makeGrpName(NclFileGrpNode *rootgrpnode, NclQuark grpname, char *h5grpname)
{
    char rootname[NCL_MAX_STRING];
    char newname[NCL_MAX_STRING];
    int slen;

  /*
   *fprintf(stderr, "\nEnter _makeGrpName, file: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgrpname: <%s>\n", NrmQuarkToString(grpname));
   */
 
    strcpy(rootname, NrmQuarkToString(rootgrpnode->real_name));
    strcpy(newname, NrmQuarkToString(grpname));

  /*
   *fprintf(stderr, "\trootname: <%s>\n", NrmQuarkToString(rootgrpnode->real_name));
   *fprintf(stderr, "\tnewname: <%s>\n", newname);
   */

    slen = strlen(rootname) - 1;
    if((! slen) || strncmp(newname, rootname, slen))
    {
        strcpy(h5grpname, rootname);

        if(('/' != rootname[slen]) && ('/' != newname[0]))
            strcat(h5grpname, "/");

        strcat(h5grpname, newname);
    }
    else
        strcpy(h5grpname, newname);

  /*
   *fprintf(stderr, "\th5grpname: <%s>\n", h5grpname);
   *fprintf(stderr, "Leave _makeGrpName, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */
}

NhlErrorTypes H5AddGrp(void *rec, NclQuark grpname)
{
    NclFileGrpNode *rootgrpnode = (NclFileGrpNode *) rec;
    NclFileGrpNode *grpnode;

    NhlErrorTypes ret = NhlNOERROR;
    char h5grpfullname[NCL_MAX_STRING];
    hid_t fid = -1;
    hid_t gid = -1;

  /*
   *fprintf(stderr, "\nEnter H5AddGrp, file: %s, line: %d\n\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tgrpname: <%s>\n", NrmQuarkToString(grpname));
   */

    ret = AddNewGrp(rec, grpname, -1);

    grpnode = _getGrpNodeFromNclFileGrpNode(rootgrpnode, grpname);

    fid = (hid_t)_getH5grpID(rootgrpnode);

    _makeGrpName(rootgrpnode, grpname, h5grpfullname);

    gid = H5Gcreate(fid, h5grpfullname, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

  /*
   *fprintf(stderr, "\tfid = %d, h5grpfullname: <%s>\n", fid, h5grpfullname);
   *fprintf(stderr, "\tgid = %d, h5grpfullname: <%s>\n", gid, h5grpfullname);
   */

    grpnode->gid = gid;
    grpnode->fid = gid;
    grpnode->pid = fid;
    grpnode->define_mode = 1;
    grpnode->open = 1;
    grpnode->parent = rootgrpnode;
    grpnode->real_name = NrmStringToQuark(h5grpfullname);

  /*
   *fprintf(stderr, "Leave H5AddGrp, file: %s, line: %d\n\n", __FILE__, __LINE__);
   */

    return ret;
}

NclFormatFunctionRec H5Rec =
{
    /* NclInitializeFileRecFunc initialize_file_rec */       H5InitializeFileRec,
    /* NclCreateFileFunc       create_file; */               H5CreateFile,
    /* NclOpenFileFunc         open_file; */                 H5OpenFile,
    /* NclFreeFileRecFunc      free_file_rec; */             H5FreeFileRec,
    /* NclGetVarNamesFunc      get_var_names; */             GetGrpVarNames,
    /* NclGetVarInfoFunc       get_var_info; */              GetVarInfo,
    /* NclGetDimNamesFunc      get_dim_names; */             GetGrpDimNames,
    /* NclGetDimInfoFunc       get_dim_info; */              GetDimInfo,
    /* NclGetAttNamesFunc      get_att_names; */             GetGrpAttNames,
    /* NclGetAttInfoFunc       get_att_info; */              GetAttInfo,
    /* NclGetVarAttNamesFunc   get_var_att_names; */         GetVarAttNamesFromGrp,
    /* NclGetVarAttInfoFunc    get_var_att_info; */          GetVarAttInfo,
    /* NclGetCoordInfoFunc     get_coord_info; */            H5GetCoordInfo,
    /* NclReadCoordFunc        read_coord; */                H5ReadCoord,
    /* NclReadCoordFunc        read_coord; */                NULL,
    /* NclReadVarFunc          read_var; */                  H5ReadVar,
    /* NclReadVarFunc          read_var; */                  NULL,
    /* NclReadAttFunc          read_att; */                  H5ReadAtt,
    /* NclReadVarAttFunc       read_var_att; */              H5ReadVarAtt,
    /* NclWriteCoordFunc       write_coord; */               H5WriteVar,
    /* NclWriteCoordFunc       write_coord; */               NULL,
    /* NclWriteVarFunc         write_var; */                 H5WriteVar,
    /* NclWriteVarFunc         write_var; */                 NULL,
    /* NclWriteAttFunc         write_att; */                 NULL, /* H5WriteAtt, */
    /* NclWriteVarAttFunc      write_var_att; */             H5WriteVarAtt,
    /* NclAddDimFunc           add_dim; */                   H5AddDim,
    /* NclAddChunkDimFunc      add_chunk_dim; */             H5AddChunkDim,
    /* NclRenameDimFunc        rename_dim; */                NULL, /* H5RenameDim, */
    /* NclAddVarFunc           add_var; */                   H5AddVar,
    /* NclAddVarChunkFunc      add_var_chunk; */             H5AddVarChunk,
    /* NclAddVarChunkCacheFunc add_var_chunk_cache; */       H5AddVarChunkCache,
    /* NclSetVarCompressLevelFunc set_var_compress_level; */ H5SetVarCompressLevel,
    /* NclAddVarFunc           add_coord_var; */             NULL,
    /* NclAddAttFunc           add_att; */                   H5AddAtt,
    /* NclAddVarAttFunc        add_var_att; */               H5AddVarAtt,
    /* NclMapFormatTypeToNcl   map_format_type_to_ncl; */    NULL, /* H5MapToNcl, */
    /* NclMapNclTypeToFormat   map_ncl_type_to_format; */    _Ncl2HDF5type,
    /* NclDelAttFunc           del_att; */                   H5DelAtt,
    /* NclDelVarAttFunc        del_var_att; */               H5DelVarAtt,
    /* NclGetGrpNamesFunc      get_grp_names; */             _NclGetGrpNames,
    /* NclGetGrpInfoFunc       get_grp_info; */              NULL,
    /* NclGetGrpAttNamesFunc   get_grp_att_names; */         NULL,
    /* NclGetGrpAttInfoFunc    get_grp_att_info; */          NULL,
    /* NclAddGrpFunc           add_grp; */                   H5AddGrp,
    /* NclAddVlenFunc          add_vlen; */                  H5AddVlen,
    /* NclAddEnumFunc          add_enum; */                  H5AddEnum,
    /* NclAddOpaqueFunc        add_opaque; */                H5AddOpaque,
    /* NclAddCompoundFunc      add_compound; */              H5AddCompound,
    /* NclWriteCompoundFunc    write_compound; */            H5WriteCompound,
    /* NclSetOptionFunc        set_option;  */               H5SetOption
};

NclFormatFunctionRecPtr HDF5AddFileFormat(void)
{
    return(&H5Rec);
}

