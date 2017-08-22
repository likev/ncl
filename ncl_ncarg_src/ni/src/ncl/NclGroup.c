
#ifdef NIO_LIB_ONLY
#include "niohlu.h"
#include "nioNresDB.h"
#include "nioCallbacks.h"
#else
#include <ncarg/hlu/hlu.h>
#include <ncarg/hlu/NresDB.h>
#include <ncarg/hlu/Callbacks.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "defs.h"
#include "Symbol.h"
#include <math.h>
#include "NclVar.h"
#include "NclGroup.h"
#include "NclFile.h"
#include "NclFileInterfaces.h"
#include "DataSupport.h"
#include "VarSupport.h"
#include "NclMultiDValData.h"
#include "NclAtt.h"
#include "AttSupport.h"
#include "NclType.h"
#include "TypeSupport.h"
#include "FileSupport.h"
#include "NclMdInc.h"
#include "NclCoordVar.h"
#include "NclCallBacksI.h"

extern int grib_version;

#define NCLGROUP_INC -1
#define NCLGROUP_DEC -2
#define NCLGROUP_VEC 0

#define GROUP_COORD_VAR_ACCESS 0
#define GROUP_VAR_ACCESS 1

NclQuark GroupGetDimName(
#if	NhlNeedProto
NclGroup /* thegroup */,
int /*num*/
#endif
);

#if 0 
/* this is not yet defined */
static int GroupIsVar(
#if	NhlNeedProto
NclGroup /*thegroup */,
NclQuark /* name */
#endif
);
#endif

void copyAttributes(NclFileAttInfoList **out, NclFileAttInfoList *in)
{
    NclFileAttInfoList *att_list;
    NclFileAttInfoList *new_list;

    *out = NULL;
    att_list = in;
    while(att_list)
    {
        new_list = (NclFileAttInfoList *) NclMalloc(sizeof(NclFileAttInfoList));
        new_list->the_att = (struct _NclFAttRec *) NclMalloc(sizeof(struct _NclFAttRec));
        new_list->the_att->att_name_quark = att_list->the_att->att_name_quark;
        new_list->the_att->data_type      = att_list->the_att->data_type;
        new_list->the_att->num_elements   = att_list->the_att->num_elements;

      /*
       *fprintf(stdout, "\tnew_list->the_att->att_name_quark: <%s>\n",
       *    NrmQuarkToString(new_list->the_att->att_name_quark));
       */

        new_list->next = *out;
        *out = new_list;
        att_list = att_list->next;
    }
}

struct _FileCallBackRec *getFileCallBack(struct _FileCallBackRec *in_fcb)
{
    struct _FileCallBackRec *out_fcb = NULL;

    if(in_fcb != NULL)
    {
        out_fcb = (struct _FileCallBackRec *)NclMalloc(sizeof(struct _FileCallBackRec));
        out_fcb->thefileid = in_fcb->thefileid;
        out_fcb->theattid  = in_fcb->theattid;
        out_fcb->thevar    = in_fcb->thevar;
    }
    return (out_fcb);
}

struct _NclFGrpRec *getGrpRec(struct _NclFGrpRec *in_grp_info)
{
    int n = 0;
    struct _NclFGrpRec *out_grp_info;
    out_grp_info = (struct _NclFGrpRec *)NclMalloc(sizeof(struct _NclFGrpRec));

    out_grp_info->grp_name_quark = in_grp_info->grp_name_quark;
    out_grp_info->grp_real_name_quark = in_grp_info->grp_real_name_quark;
    out_grp_info->grp_full_name_quark = in_grp_info->grp_full_name_quark;
    out_grp_info->data_type = in_grp_info->data_type;
    out_grp_info->num_dimensions = in_grp_info->num_dimensions;

    for(n = 0; n < in_grp_info->num_dimensions; n++)
        out_grp_info->file_dim_num[n] = in_grp_info->file_dim_num[n];

    return (out_grp_info);
}

struct _NclFVarRec *getVarRec(struct _NclFVarRec *in_var_info)
{
    int n = 0;
    struct _NclFVarRec *out_var_info;
    out_var_info = (struct _NclFVarRec *)NclMalloc(sizeof(struct _NclFVarRec));

    out_var_info->var_name_quark = in_var_info->var_name_quark;
    out_var_info->var_real_name_quark = in_var_info->var_real_name_quark;
    out_var_info->var_full_name_quark = in_var_info->var_full_name_quark;
    out_var_info->data_type = in_var_info->data_type;
    out_var_info->num_dimensions = in_var_info->num_dimensions;
    out_var_info->num_compounds = in_var_info->num_compounds;

    for(n = 0; n < in_var_info->num_dimensions; n++)
        out_var_info->file_dim_num[n] = in_var_info->file_dim_num[n];

    for(n = 0; n < in_var_info->num_compounds; n++)
    {
        out_var_info->component_name[n] = in_var_info->component_name[n];
        out_var_info->component_type[n] = in_var_info->component_type[n];
    }
 
    return (out_var_info);
}

void setGroupAttributes(NclFile group_out)
{
    int i, j, num_atts;
    NclQuark group_name;
    NclQuark *name_list;

    if(group_out->file.format_funcs->get_grp_att_names != NULL)
    {
        for(i = 0; i < group_out->file.n_grps; i++)
        {
            group_name = group_out->file.grp_info[i]->grp_full_name_quark;

            name_list = (*group_out->file.format_funcs->get_grp_att_names)
                        (group_out->file.private_rec,group_name,&num_atts);

            for(j = 0; j < num_atts; j++)
            {
                AddAttInfoToList(&(group_out->file.grp_att_info[i]),
                                  (*group_out->file.format_funcs->get_grp_att_info)
                                  (group_out->file.private_rec,group_name,name_list[j]));
            }

            NclFree((void*)name_list);
        }
    }
}

void initializeGroup(NclFile group_out)
{
    _NclInitFilePart(&(group_out->file));
}

void readFileAtt
#if    NhlNeedProto
(NclFile thefile)
#else 
(thefile)
NclFile thefile;
#endif
{
    int att_id = -1;
    int i;
    void *val;
    NclMultiDValData tmp_md;
    NhlArgVal udata;

    if(thefile->file.format_funcs->read_att != NULL)
    {
        att_id = _NclAttCreate(NULL,NULL,Ncl_Att,0,(NclObj)thefile);
        for(i = 0; i < thefile->file.n_file_atts; i++)
        {
            val = NclMalloc(_NclSizeOf(thefile->file.file_atts[i]->data_type)*
                                       thefile->file.file_atts[i]->num_elements );

            (void)(*thefile->file.format_funcs->read_att)
                  (thefile->file.private_rec,
                   thefile->file.file_atts[i]->att_name_quark,
                   val);

            tmp_md = _NclCreateMultiDVal(
                    NULL,
                    NULL,
                    Ncl_MultiDValData,
                    0,
                    val,
                    NULL,
                    1,
                    &thefile->file.file_atts[i]->num_elements,
                    TEMPORARY,
                    NULL,
                    _NclTypeEnumToTypeClass(_NclBasicDataTypeToObjType(thefile->file.file_atts[i]->data_type)));
            if(tmp_md != NULL)
            {
                _NclAddAtt(att_id,NrmQuarkToString(thefile->file.file_atts[i]->att_name_quark),tmp_md,NULL);
            }
        }
        udata.ptrval = (void*)NclMalloc(sizeof(FileCallBackRec));
        ((FileCallBackRec*)udata.ptrval)->thefileid = thefile->obj.id;
        ((FileCallBackRec*)udata.ptrval)->theattid = att_id;
        ((FileCallBackRec*)udata.ptrval)->thevar = -1;
        thefile->file.file_att_cb = _NclAddCallback((NclObj)_NclGetObj(att_id),NULL,FileAttIsBeingDestroyedNotify,ATTDESTROYED,&udata);
        thefile->file.file_att_udata = (FileCallBackRec*)udata.ptrval;
        thefile->file.file_atts_id = att_id;
    }
}

void setVarAtts
#if     NhlNeedProto
(NclFile thefile)
#else
(thefile)
NclFile thefile;
#endif
{
    int index;
    NclFileAttInfoList *step;
    int att_id = -1;
    void *val;
    NclMultiDValData tmp_md;
    NhlArgVal udata;
    NclQuark  var;
    
    for(index = 0; index < thefile->file.n_vars; index++)
    {
        var = thefile->file.var_info[index]->var_full_name_quark;
        step = thefile->file.var_att_info[index];
        att_id = _NclAttCreate(NULL,NULL,Ncl_Att,0,(NclObj)thefile);
        while(step != NULL)
        {
            if (step->the_att->data_type == NCL_none)
                val = NULL;
            else
            {
                val = NclMalloc(_NclSizeOf(step->the_att->data_type)* step->the_att->num_elements );
                (void)(*thefile->file.format_funcs->read_var_att)
                      (thefile->file.private_rec, var,
                       step->the_att->att_name_quark, val);
            }
            tmp_md = _NclCreateMultiDVal(
                    NULL,
                    NULL,
                    Ncl_MultiDValData,
                    0,
                    val,
                    NULL,
                    1,
                    &step->the_att->num_elements,
                    TEMPORARY,
                    NULL,
                    _NclTypeEnumToTypeClass(_NclBasicDataTypeToObjType(step->the_att->data_type))
                    );
            if(tmp_md != NULL)
            {
                _NclAddAtt(att_id,NrmQuarkToString(step->the_att->att_name_quark),tmp_md,NULL);
            }
            step = step->next;
        }
        udata.ptrval = (void*)NclMalloc(sizeof(FileCallBackRec));
        ((FileCallBackRec*)udata.ptrval)->thefileid = thefile->obj.id;
        ((FileCallBackRec*)udata.ptrval)->theattid = att_id;
        ((FileCallBackRec*)udata.ptrval)->thevar = var;
        thefile->file.var_att_cb[index] = _NclAddCallback((NclObj)_NclGetObj(att_id),NULL,
					      FileAttIsBeingDestroyedNotify,ATTDESTROYED,&udata);
        thefile->file.var_att_udata[index] = (FileCallBackRec*)udata.ptrval;
        thefile->file.var_att_ids[index] = att_id;
    }
}

NclQuark *_split_string2quark(NclQuark qn, int *ns)
{
    char name[NCL_MAX_STRING];
    char *tmp_str = NULL;
    char *result = NULL;
    char *delim  = "/";
    NclQuark *sq = NULL;
    int i = 1;
    int n = 1;

    strcpy(name, NrmQuarkToString(qn));

  /*
   *fprintf(stdout, "\n\nEnter _split_string2quark, file: %s, line:%d\n", __FILE__, __LINE__);

   *fprintf(stdout, "\tin name: <%s>\n", name);
   */
   
    while(name[i])
    {
        if('/' == name[i])
           ++n;
        ++i;
    }

    sq = (NclQuark *)NclMalloc(n * sizeof(NclQuark));

    *ns = n;

    tmp_str = name;

    result = strtok(tmp_str, delim);

    n = 0;
    while(result != NULL)
    {
        sq[n] = NrmStringToQuark(result);

      /*
       *fprintf(stdout, "\tsub str %d: <%s>\n", n, result);
       */

        ++n;
        result = strtok(NULL, delim);
    }

  /*
   *fprintf(stdout, "\tname: <%s>\n", name);
   *fprintf(stdout, "Leave _split_string2quark, file: %s, line:%d\n\n", __FILE__, __LINE__);
   */

    return sq;
}

/*
 * Updates the dimension info
 */

NhlErrorTypes UpdateGroupDims(NclGroup *group_out, NclFile thefile)
{
	NclQuark *name_list;
	int n_names;
	int i;
	int index = -1;

	name_list = (*thefile->file.format_funcs->get_dim_names)(thefile->file.private_rec,&n_names);
	group_out->file.n_file_dims = n_names;
	for(i = 0; i < n_names; i++){
		if (group_out->file.file_dim_info[i])
			NclFree(group_out->file.file_dim_info[i]);
		group_out->file.file_dim_info[i] = (thefile->file.format_funcs->get_dim_info)
			(thefile->file.private_rec,name_list[i]);
		if(thefile->file.n_vars)
		    index = _NclFileIsVar(thefile,name_list[i]);
		if(index > -1 && thefile->file.var_info[index]->num_dimensions == 1) {
			group_out->file.coord_vars[i] = thefile->file.var_info[index];
		}
	}
	NclFree((void*)name_list);
	return NhlNOERROR;
}

NclGroup *_NclGroupCreate
#if    NhlNeedProto
(NclObj  inst, NclObjClass theclass, NclObjTypes obj_type, unsigned int obj_type_mask, NclStatus status, NclFile file_in, NclQuark group_name)
#else
(inst, theclass, obj_type, obj_type_mask, status, file_in, group_name)
NclObj  inst;
NclObjClass theclass;
NclObjTypes obj_type;
unsigned int obj_type_mask;
NclStatus status; 
NclFile file_in;
NclQuark group_name;
#endif
{
    NclGroup *group_out = NULL;
    int group_out_free = 0;
    NhlErrorTypes ret= NhlNOERROR;
    NclObjClass class_ptr;

    NclQuark *selected_group;
    int nsg = 0;
    int new_group = 0;
    int i, j, n;
    int n_grps = 0;
    int n_vars = 0;

    NclQuark *inqname;
    int       inqnumb;

    NclQuark *tmpqname;
    int       tmpqnumb;

    int       found = 0;

    if(theclass == NULL)
    {
        class_ptr = nclFileClass;
    }
    else
    {
        class_ptr = theclass;
    }

    if(inst == NULL)
    {
        group_out = (NclFile)NclCalloc(1, sizeof(NclFileRec));
        group_out_free = 1;
    }
    else
    {
        group_out = (NclFile)inst;
    }

    inqname = _split_string2quark(group_name, &inqnumb);

  /*
   *fprintf(stdout, "\n\nhit _NclGroupCreate, file: %s, line:%d\n", __FILE__, __LINE__);
   *fprintf(stdout, "\tgroup_name: <%s>\n", NrmQuarkToString(group_name));
   */

    initializeGroup(group_out);

    group_out->file.fname = file_in->file.fname;
    group_out->file.fpath = NrmStringToQuark(_NGResolvePath(NrmQuarkToString(file_in->file.fpath)));
    group_out->file.file_ext_q = file_in->file.file_ext_q;
    group_out->file.wr_status = file_in->file.wr_status;
    group_out->file.file_format = file_in->file.file_format;
    group_out->file.advanced_file_structure = 0;
    group_out->file.format_funcs = _NclGetFormatFuncs(group_out->file.file_ext_q);
    group_out->file.private_rec = (*group_out->file.format_funcs->initialize_file_rec)(&group_out->file.file_format);

    if(group_out->file.private_rec == NULL)
    {
        NhlPError(NhlFATAL,ENOMEM,NULL);
        if(group_out_free) 
            NclFree((void*)group_out);
        return(NULL);
    }

    group_out->file.private_rec = (*group_out->file.format_funcs->open_file)
                                  (group_out->file.private_rec,
                                   group_out->file.fpath, group_out->file.wr_status);

    group_out->file.n_file_dims = file_in->file.n_file_dims;
    group_out->file.n_file_atts = file_in->file.n_file_atts;

    _NclReallocFilePart(&(group_out->file), -1, -1,
                        group_out->file.n_file_dims, group_out->file.n_file_atts);

    UpdateGroupDims(group_out, file_in);
  /*
   *for(i = 0; i < group_out->file.n_file_dims; i++)
   *{
   *    group_out->file.file_dim_info[i] = file_in->file.file_dim_info[i];
   *    group_out->file.coord_vars[i] = file_in->file.coord_vars[i];
   *}
   */

    for(j = 0; j < file_in->file.n_file_atts; j++)
    {
        group_out->file.file_atts[j] = (struct _NclFAttRec *)NclMalloc(sizeof(struct _NclFAttRec));
        group_out->file.file_atts[j]->att_name_quark = file_in->file.file_atts[j]->att_name_quark;
        group_out->file.file_atts[j]->data_type      = file_in->file.file_atts[j]->data_type;
        group_out->file.file_atts[j]->num_elements   = file_in->file.file_atts[j]->num_elements;
    }

    readFileAtt(group_out);

    selected_group = (NclQuark *)NclMalloc((1 + file_in->file.n_grps) * sizeof(NclQuark));

    nsg = 0;
    for(i = 0; i < file_in->file.n_grps; i++)
    {
        tmpqname = _split_string2quark(file_in->file.grp_info[i]->grp_full_name_quark, &tmpqnumb);

        found = 0;
        for(n = 0; n < tmpqnumb; ++n)
        {
          /*
           *fprintf(stderr, "\ttmpqname[%d]: <%s>\n", n, NrmQuarkToString(tmpqname[n]));
           */

            if(tmpqname[n] == inqname[0])
            {
                found = 1;
                for(j = 0; j < inqnumb; ++j)
                {
                  /*
                   *fprintf(stderr, "\tinqname[%d]: <%s>\n", j, NrmQuarkToString(inqname[j]));
                   */
                    if(tmpqname[n+j] != inqname[j])
                    {
                        found = 0;
                        break;
                    }
                }
            }
            if(found)
                break;
        }

        NclFree(tmpqname);

      /*
       *att_list = file_in->file.grp_att_info[i];
       *j = group_out->file.n_file_atts;
       *while(att_list)
       *{
       *    group_out->file.file_atts[j] = (struct _NclFAttRec *)NclMalloc(sizeof(struct _NclFAttRec));
       *    group_out->file.file_atts[j]->att_name_quark = att_list->the_att->att_name_quark;
       *    group_out->file.file_atts[j]->data_type      = att_list->the_att->data_type;
       *    group_out->file.file_atts[j]->num_elements   = att_list->the_att->num_elements;
       *    j++;
       *    att_list = att_list->next;
       *}
       *group_out->file.n_file_atts = j;
       */

        if(! found)
            continue;

        new_group = 1;
        for(n = 0; n < nsg; n++)
        {
            if(selected_group[n] == file_in->file.grp_info[i]->grp_full_name_quark)
            {
                new_group = 0;
                break;
            }
        }
        if(new_group)
        {
            selected_group[nsg] = file_in->file.grp_info[i]->grp_full_name_quark;
            nsg++;
        }

      /*
       *copyAttributes(&(group_out->file.grp_att_info[n_grps]),
       *               file_in->file.grp_att_info[i]);
       *group_out->file.grp_att_ids[n_grps] = -1;
       *LoadVarAtts(file_in, file_in->file.grp_info[i]->grp_name_quark);
       */

        if(inqnumb != tmpqnumb)
        {
	    if(n_grps >= group_out->file.max_grps)
                _NclReallocFilePart(&(group_out->file), n_grps, -1, -1, -1);
            group_out->file.grp_info[n_grps] = getGrpRec(file_in->file.grp_info[i]);
            n_grps++;
        }
    }

    group_out->file.n_grps = n_grps;
    setGroupAttributes(group_out);

  /*
   *fprintf(stderr, "file: %s, line: %d\n", __FILE__, __LINE__);
   *fprintf(stderr, "\tn_grps = %d\n", n_grps);
   */

    for(i = 0; i < file_in->file.n_vars; i++)
    {
        tmpqname = _split_string2quark(file_in->file.var_info[i]->var_full_name_quark, &tmpqnumb);

        found = 0;
        for(n = 0; n < tmpqnumb - 1; ++n)
        {
          /*
           *fprintf(stderr, "\ttmpqname[%d]: <%s>\n", n, NrmQuarkToString(tmpqname[n]));
           */

            if(tmpqname[n] == inqname[0])
            {
                found = 1;
                for(j = 0; j < inqnumb; ++j)
                {
                  /*
                   *fprintf(stderr, "\tinqname[%d]: <%s>\n", j, NrmQuarkToString(inqname[j]));
                   */
                    if(tmpqname[n+j] != inqname[j])
                    {
                        found = 0;
                        break;
                    }
                }
            }
            if(found)
                break;
        }

        NclFree(tmpqname);

        if(!found)
            continue;

	if(n_vars >= group_out->file.max_vars)
            _NclReallocFilePart(&(group_out->file), -1, n_vars, -1, -1);

        group_out->file.var_info[n_vars] = getVarRec(file_in->file.var_info[i]);
        copyAttributes(&(group_out->file.var_att_info[n_vars]),
                       file_in->file.var_att_info[i]);

      /*
       *UpdateCoordInfo(group_out, group_out->file.var_info[n_vars]->var_name_quark);
       */
        n_vars++;
    }

    group_out->file.n_vars = n_vars;

    _NclReallocFilePart(&(group_out->file), n_grps, n_vars, -1, -1);

    setVarAtts(group_out);

    (void)_NclObjCreate((NclObj)group_out,class_ptr,obj_type,
                        (obj_type_mask | Ncl_File),status);

    if(class_ptr == nclFileClass)
    {
        _NclCallCallBacks((NclObj)group_out,CREATED);
    }

    NclFree(selected_group);

  /*
   *fprintf(stdout, "\tgroup_out->file.n_vars = %d\n", group_out->file.n_vars);
   *fprintf(stdout, "\n\nend _NclGroupCreate, file: %s, line:%d\n", __FILE__, __LINE__);
   */
    return(group_out);
}

