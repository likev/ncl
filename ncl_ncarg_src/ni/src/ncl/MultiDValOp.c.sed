
/*
 *      $Id: MultiDValOp.c.sed,v 1.13.4.1 2008-03-28 20:37:49 grubin Exp $
 */
/************************************************************************
*									*
*			     Copyright (C)  1995			*
*	     University Corporation for Atmospheric Research		*
*			     All Rights Reserved			*
*									*
************************************************************************/
/*
 *	File:		
 *
 *	Author:		Ethan Alpert
 *			National Center for Atmospheric Research
 *			PO 3000, Boulder, Colorado
 *
 *	Date:		Fri Jan 27 18:22:08 MST 1995
 *
 *	Description:	
 */

static NclData MultiDVal_mdmd_FUNCNAME
#if	NhlNeedProto
(NclData self, NclData other, NclData result)
#else
(self,other,result)
NclData	self;
NclData other;
NclData result;
#endif
{
	NclMultiDValData self_md = (NclMultiDValData)self;
	NclMultiDValData other_md = (NclMultiDValData)other;
	NclMultiDValData result_md = (NclMultiDValData)result;
	NclMultiDValData output_md = NULL;
	NclTypeClass the_type = NULL;
	NclTypeClass operand_type = NULL;
	NclMissingRec themissing;
	void *result_val;
	ng_size_t i;
	

	if((other_md == NULL)||(self_md == NULL))
		return(NULL);

        if((other_md->multidval.n_dims != self_md->multidval.n_dims)){
		NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: Number of dimensions do not match, can't continue");
                return(NULL);
        }
	operand_type = self_md->multidval.type;
	for(i = 0; i< self_md->multidval.n_dims; i++) {
                if(self_md->multidval.dim_sizes[i]
                        != other_md->multidval.dim_sizes[i]) {
                        NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: Dimension size, for dimension number %d, of operands does not match, can't continue\n",i);
                        return(NULL);
                }
        }
        if(self_md->multidval.missing_value.has_missing) {
                themissing.value = self_md->multidval.missing_value.value;
                themissing.has_missing = 1;
        } else if(other_md->multidval.missing_value.has_missing) {
                themissing.value = other_md->multidval.missing_value.value;
                themissing.has_missing = 1;
        } else {
                themissing.has_missing = 0;
        }

	if((self_md->multidval.type != NULL)&&(((NclTypeClass)self_md->multidval.type)->type_class.TFUNC != NULL)) {

/*
* the_type is not null since requirement if TFUNC != NULL TFUNC_type != NULL
*/

		the_type = _NclTFUNC_type(self_md->multidval.type);
		if((result_md != NULL)&&(result_md->multidval.data_type== the_type->type_class.data_type)) {
			result_val = result_md->multidval.val;
		} else if((result_md != NULL)&&(result_md->multidval.type->type_class.size >= the_type->type_class.size)) {
			result_val = result_md->multidval.val;
			result_md->multidval.type = the_type;
			result_md->multidval.data_type = the_type->type_class.data_type;
			result_md->multidval.hlu_type_rep[0] = the_type->type_class.hlu_type_rep[0];
			result_md->multidval.hlu_type_rep[1] = the_type->type_class.hlu_type_rep[1];
			result_md->multidval.totalsize = result_md->multidval.totalelements * the_type->type_class.size;
		} else {
			if((result_md != NULL) &&((result_md != self_md)&&(result_md != other_md))) {
				_NclDestroyObj((NclObj)result_md);
			}
			result_md = NULL;
			result_val = (void*)NclMalloc(self_md->multidval.totalelements * the_type->type_class.size);
			if(result_val == NULL) {
				NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: Could not allocate memory for result type, can't continue\n");
				return(NULL);
			}
		}
		if(_NclTFUNC(
			operand_type,
			result_val,
			self_md->multidval.val,
			other_md->multidval.val,
			(self_md->multidval.missing_value.has_missing?(void*)&self_md->multidval.missing_value.value:NULL),
			(other_md->multidval.missing_value.has_missing?(void*)&other_md->multidval.missing_value.value:NULL),
			self_md->multidval.totalelements,
			other_md->multidval.totalelements) != NhlFATAL) {

			if((the_type != operand_type)&&(themissing.has_missing)) {
				if((the_type->type_class.data_type == NCL_logical)||(!_NclScalarCoerce(&themissing.value,operand_type->type_class.data_type,&themissing.value,the_type->type_class.data_type) )) {

					themissing.value = the_type->type_class.default_mis;
				}
			}
			if(result_md == NULL) {
				output_md = _NclCreateMultiDVal(
					(NclObj)result_md,
					NULL,
					Ncl_MultiDValData,
					0,
					result_val,
					(themissing.has_missing? &(themissing.value):NULL),
					self_md->multidval.n_dims,
					self_md->multidval.dim_sizes,
					TEMPORARY,
					NULL,
					the_type
				);
				return((NclData)output_md);
			} else {
				if(themissing.has_missing)
					result_md->multidval.missing_value = themissing;
				return((NclData)result_md);
			}
		} else {
			if(result_md == NULL) {
				NclFree(result_val);
			}
			NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: operator failed, can't continue");
			return(NULL);
		}
			
			
		
	} else {
		NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: operation not supported on type (%s)",_NclBasicDataTypeToName(self_md->multidval.data_type));
		return(NULL);
	}
}

static NclData MultiDVal_ss_FUNCNAME
#if	NhlNeedProto
(NclData self, NclData other, NclData result)
#else
(self,other,result)
NclData	self;
NclData other;
NclData result;
#endif
{
	NclMultiDValData self_md = (NclMultiDValData)self;
	NclMultiDValData other_md = (NclMultiDValData)other;
	NclMultiDValData result_md = (NclMultiDValData)result;
	NclMultiDValData output_md;
	NclTypeClass the_type = NULL;
	NclTypeClass operand_type = NULL;
	NclMissingRec themissing;
	void *result_val;
	ng_size_t total;
	ng_size_t *dim_sizes = NULL;
	int n_dims = 1;
	

	if((other_md == NULL)||(self_md == NULL))
		return(NULL);

	operand_type = self_md->multidval.type;
        if(self_md->multidval.missing_value.has_missing) {
                themissing.value = self_md->multidval.missing_value.value;
                themissing.has_missing = 1;
        } else if(other_md->multidval.missing_value.has_missing) {
                themissing.value = other_md->multidval.missing_value.value;
                themissing.has_missing = 1;
        } else {
                themissing.has_missing = 0;
        }
	if(!((self_md->multidval.kind == SCALAR)||(other_md->multidval.kind == SCALAR))) {
		NhlPError(NhlFATAL,NhlEUNKNOWN,"TFUNC for scalar values was called with non scalar value, this should not happen");
		return(NULL);
	}

	total = self_md->multidval.totalelements;
	n_dims = self_md->multidval.n_dims;
	dim_sizes = self_md->multidval.dim_sizes;
	if(total < other_md->multidval.totalelements) {
		total = other_md->multidval.totalelements;
		n_dims = other_md->multidval.n_dims;
		dim_sizes = other_md->multidval.dim_sizes;
	}

	if((self_md->multidval.type != NULL)&&(((NclTypeClass)self_md->multidval.type)->type_class.TFUNC != NULL)) {

/*
* the_type is not null since requirement if TFUNC != NULL TFUNC_type != NULL
*/

		the_type = _NclTFUNC_type(self_md->multidval.type);

                if((result_md != NULL)&&(result_md->multidval.data_type== the_type->type_class.data_type)) {
                        result_val = result_md->multidval.val;
                } else if((result_md != NULL)&&(result_md->multidval.type->type_class.size >= the_type->type_class.size)) {
                        result_val = result_md->multidval.val;
                        result_md->multidval.type = the_type;
                        result_md->multidval.data_type = the_type->type_class.data_type;
                        result_md->multidval.hlu_type_rep[0] = the_type->type_class.hlu_type_rep[0];
                        result_md->multidval.hlu_type_rep[1] = the_type->type_class.hlu_type_rep[1];
                        result_md->multidval.totalsize = result_md->multidval.totalelements * the_type->type_class.size;
                } else {
			if((result_md != NULL) &&((result_md != self_md)&&(result_md != other_md))) {
                                _NclDestroyObj((NclObj)result_md);
                        }
                        result_md = NULL;
                        result_val = (void*)NclMalloc(total * the_type->type_class.size);
                        if(result_val == NULL) {
                                NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: Could not allocate memory for result type, can't continue\n");
                                return(NULL);
                        }
                }
		if((the_type != operand_type)&&(themissing.has_missing)) {
			if((the_type->type_class.data_type == NCL_logical)||(!_NclScalarCoerce(&themissing.value,operand_type->type_class.data_type,&themissing.value,the_type->type_class.data_type) )) {
				themissing.value = the_type->type_class.default_mis;
			}
		}

		if(_NclTFUNC(
			operand_type,
			result_val,
			self_md->multidval.val,
			other_md->multidval.val,
			(self_md->multidval.missing_value.has_missing?(void*)&self_md->multidval.missing_value.value:NULL),
			(other_md->multidval.missing_value.has_missing?(void*)&other_md->multidval.missing_value.value:NULL),
			self_md->multidval.totalelements,
			other_md->multidval.totalelements) != NhlFATAL) {

			if(result_md == NULL) {
				output_md = _NclCreateMultiDVal(
					(NclObj)result_md,
					NULL,
					Ncl_MultiDValData,
					0,
					result_val,
					(themissing.has_missing? &(themissing.value):NULL),
					n_dims,
					dim_sizes,
					TEMPORARY,
					NULL,
					the_type
				);
				return((NclData)output_md);
			} else {
				if(themissing.has_missing)
					result_md->multidval.missing_value = themissing;
				return((NclData)result_md);
			}
		} else {
			if(result_md == NULL) {
				NclFree(result_val);
			}
			NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: operator failed, can't continue");
			return(NULL);
		}
	} else {
		NhlPError(NhlFATAL,NhlEUNKNOWN,"FUNCNAME: operation not supported on type (%s)",_NclBasicDataTypeToName(self_md->multidval.data_type));
		return(NULL);
	}
}
