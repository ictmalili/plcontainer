#include "pyconversions.h"
#include "common/messages/messages.h"
#include "common/comm_utils.h"
#include <Python.h>

static PyObject *plc_pyobject_from_int1(char *input);
static PyObject *plc_pyobject_from_int2(char *input);
static PyObject *plc_pyobject_from_int4(char *input);
static PyObject *plc_pyobject_from_int8(char *input);
static PyObject *plc_pyobject_from_float4(char *input);
static PyObject *plc_pyobject_from_float8(char *input);
static PyObject *plc_pyobject_from_text(char *input);
static PyObject *plc_pyobject_from_text_ptr(char *input);
static PyObject *plc_pyobject_from_array_dim(plcArray *arr, plcPyInputFunc infunc,
                    int *idx, int *ipos, char **pos, int vallen, int dim);
static PyObject *plc_pyobject_from_array (char *input);

static int plc_pyobject_as_int1(PyObject *input, char **output);
static int plc_pyobject_as_int2(PyObject *input, char **output);
static int plc_pyobject_as_int4(PyObject *input, char **output);
static int plc_pyobject_as_int8(PyObject *input, char **output);
static int plc_pyobject_as_float4(PyObject *input, char **output);
static int plc_pyobject_as_float8(PyObject *input, char **output);
static int plc_pyobject_as_text(PyObject *input, char **output);
static int plc_pyobject_as_array(PyObject *input, char **output);

static void plc_pyobject_iter_free (plcIterator *iter);
static rawdata *plc_pyobject_as_array_next (plcIterator *iter);

static int plc_get_type_length(plcDatatype dt);
static plcPyInputFunc plc_get_input_function(plcDatatype dt);
static plcPyOutputFunc plc_get_output_function(plcDatatype dt);

static PyObject *plc_pyobject_from_int1(char *input) {
    return PyLong_FromLong( (long) *input );
}

static PyObject *plc_pyobject_from_int2(char *input) {
    return PyLong_FromLong( (long) *((short*)input) );
}

static PyObject *plc_pyobject_from_int4(char *input) {
    return PyLong_FromLong( (long) *((int*)input) );
}

static PyObject *plc_pyobject_from_int8(char *input) {
    return PyLong_FromLongLong( (long long) *((long long*)input) );
}

static PyObject *plc_pyobject_from_float4(char *input) {
    return PyFloat_FromDouble( (double) *((float*)input) );
}

static PyObject *plc_pyobject_from_float8(char *input) {
    return PyFloat_FromDouble( *((double*)input) );
}

static PyObject *plc_pyobject_from_text(char *input) {
    return PyString_FromString( input );
}

static PyObject *plc_pyobject_from_text_ptr(char *input) {
    return PyString_FromString( *((char**)input) );
}

static PyObject *plc_pyobject_from_array_dim(plcArray *arr,
                                             plcPyInputFunc infunc,
                                             int *idx,
                                             int *ipos,
                                             char **pos,
                                             int vallen,
                                             int dim) {
    PyObject *res = NULL;
    if (dim == arr->meta->ndims) {
        if (arr->nulls[*ipos] != 0) {
            res = Py_None;
            Py_INCREF(Py_None);
        } else {
            res = infunc(*pos);
            *ipos += 1;
            *pos = *pos + vallen;
        }
    } else {
        res = PyList_New(arr->meta->dims[dim]);
        for (idx[dim] = 0; idx[dim] < arr->meta->dims[dim]; idx[dim]++) {
            PyObject *obj;
            obj = plc_pyobject_from_array_dim(arr, infunc, idx, ipos, pos, vallen, dim+1);
            if (obj == NULL) {
                Py_DECREF(res);
                return NULL;
            }
            PyList_SetItem(res, idx[dim], obj);
        }
    }
    return res;
}

static PyObject *plc_pyobject_from_array (char *input) {
    plcArray *arr = (plcArray*)input;
    PyObject *res = NULL;

    if (arr->meta->ndims == 0) {
        res = PyList_New(0);
    } else {
        int  *idx;
        int  ipos;
        char *pos;
        int   vallen = 0;
        plcPyInputFunc infunc;

        idx = malloc(sizeof(int) * arr->meta->ndims);
        memset(idx, 0, sizeof(int) * arr->meta->ndims);
        ipos = 0;
        pos = arr->data;
        vallen = plc_get_type_length(arr->meta->type);
        infunc = plc_get_input_function(arr->meta->type);
        if (arr->meta->type == PLC_DATA_TEXT)
            infunc = plc_pyobject_from_text_ptr;
        res = plc_pyobject_from_array_dim(arr, infunc, idx, &ipos, &pos, vallen, 0);
    }

    return res;
}

static int plc_pyobject_as_int1(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(1);
    *output = out;
    if (PyLong_Check(input))
        *out = (char)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *out = (char)PyInt_AsLong(input);
    else if (PyFloat_Check(input))
        *out = (char)PyFloat_AsDouble(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_int2(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(2);
    *output = out;
    if (PyLong_Check(input))
        *((short*)out) = (short)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *((short*)out) = (short)PyInt_AsLong(input);
    else if (PyFloat_Check(input))
        *((short*)out) = (short)PyFloat_AsDouble(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_int4(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(4);
    *output = out;
    if (PyLong_Check(input))
        *((int*)out) = (int)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *((int*)out) = (int)PyInt_AsLong(input);
    else if (PyFloat_Check(input))
        *((int*)out) = (int)PyFloat_AsDouble(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_int8(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(8);
    *output = out;
    if (PyLong_Check(input))
        *((long long*)out) = (long long)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *((long long*)out) = (long long)PyInt_AsLong(input);
    else if (PyFloat_Check(input))
        *((long long*)out) = (long long)PyFloat_AsDouble(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_float4(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(4);
    *output = out;
    if (PyFloat_Check(input))
        *((float*)out) = (float)PyFloat_AsDouble(input);
    else if (PyLong_Check(input))
        *((float*)out) = (float)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *((float*)out) = (float)PyInt_AsLong(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_float8(PyObject *input, char **output) {
    int res = 0;
    char *out = (char*)malloc(8);
    *output = out;
    if (PyFloat_Check(input))
        *((double*)out) = (double)PyFloat_AsDouble(input);
    else if (PyLong_Check(input))
        *((double*)out) = (double)PyLong_AsLongLong(input);
    else if (PyInt_Check(input))
        *((double*)out) = (double)PyInt_AsLong(input);
    else
        res = -1;
    return res;
}

static int plc_pyobject_as_text(PyObject *input, char **output) {
    int res = 0;
    PyObject *obj;
    obj = PyObject_Str(input);
    if (obj != NULL) {
        *output = strdup(PyString_AsString(obj));
        Py_DECREF(obj);
    } else {
        res = -1;
    }
    return res;
}

static void plc_pyobject_iter_free (plcIterator *iter) {
    plcPyArrMeta    *meta;
    meta = (plcPyArrMeta*)iter->meta;
    pfree(meta->dims);
    pfree(iter->meta);
    pfree(iter->position);
    pfree(iter);
}

static rawdata *plc_pyobject_as_array_next (plcIterator *iter) {
    plcPyArrMeta    *meta;
    plcPyArrPointer *ptrs;
    rawdata         *res;
    PyObject        *obj;
    int              ptr;

    meta = (plcPyArrMeta*)iter->meta;
    ptrs = (plcPyArrPointer*)iter->position;
    res  = (rawdata*)pmalloc(sizeof(rawdata));

    ptr = meta->ndims - 1;
    if (ptrs[ptr].obj)
    obj = PyList_GetItem(ptrs[ptr].obj, ptrs[ptr].pos);
    if (obj == NULL || obj == Py_None) {
        res->isnull = 1;
        res->value = NULL;
    } else {
        res->isnull = 0;
        meta->outputfunc(obj, &res->value);
    }

    while (ptr > 0) {
        ptrs[ptr].pos += 1;
        /* If we finished up iterating over this dimension */
        if (ptrs[ptr].pos == meta->dims[ptr]) {
            Py_DECREF(ptrs[ptr].obj);
            ptrs[ptr].obj = NULL;
            ptrs[ptr].pos = 0;
            ptr -= 1;
        }
        /* If we found the "next" dimension to iterate over */
        else if (ptrs[ptr].pos < meta->dims[ptr]) {
            ptr += 1;
            while (ptr < meta->ndims) {
                ptrs[ptr].obj = PyList_GetItem(ptrs[ptr-1].obj, ptrs[ptr-1].pos);
                Py_INCREF(ptrs[ptr].obj);
                ptrs[ptr].pos = 0;
                ptr += 1;
            }
            break;
        }
    }

    if (ptr < 0)
        plc_pyobject_iter_free(iter);

    return res;
}

static int plc_pyobject_as_array(PyObject *input, char **output) {
    plcPyArrMeta    *meta;
    PyObject        *obj;
    plcIterator     *iter;
    size_t           dims[PLC_MAX_ARRAY_DIMS];
    PyObject        *stack[PLC_MAX_ARRAY_DIMS];
    int              ndims = 0;
    int              res = 0;
    int              i = 0;
    plcPyArrPointer *ptrs;

    /* We allow only lists to be returned as arrays */
    if (PyList_Check(input)) {
        obj = input;
        while (obj != NULL && PyList_Check(obj)) {
            dims[ndims]  = PyList_Size(obj);
            stack[ndims] = obj;
            Py_INCREF(stack[ndims]);
            ndims += 1;
            if (dims[ndims-1] > 0)
                obj = PyList_GetItem(obj, 0);
            else
                break;
        }

        /* Allocate the iterator */
        iter = (plcIterator*)pmalloc(sizeof(plcIterator));

        /* Initialize meta */
        meta = (plcPyArrMeta*)pmalloc(sizeof(plcPyArrMeta));
        meta->ndims = ndims;
        meta->dims  = (size_t*)pmalloc(ndims * sizeof(size_t));
        for (i = 0; i < ndims; i++)
            meta->dims[i] = dims[i];
        /* TODO: !!!!!!!!!!! */
        meta->outputfunc = plc_pyobject_as_int8;
        iter->meta = (char*)meta;

        /* Initializing initial position */
        ptrs = (plcPyArrPointer*)pmalloc(ndims * sizeof(plcPyArrPointer));
        for (i = 0; i < ndims; i++) {
            ptrs[i].pos = 0;
            ptrs[i].obj = stack[i];
        }
        iter->position = (char*)ptrs;

        /* Initializing "data" */
        *((PyObject**)iter->data) = input;

        /* Initializing "next" function */
        iter->next = plc_pyobject_as_array_next;

        *((plcIterator**)output) = iter;
    } else {
        *((plcIterator**)output) = NULL;
        res = -1;
    }

    return res;
}

static int plc_get_type_length(plcDatatype dt) {
    int res = 0;
    switch (dt) {
        case PLC_DATA_INT1:
            res = 1;
            break;
        case PLC_DATA_INT2:
            res = 2;
            break;
        case PLC_DATA_INT4:
            res = 4;
            break;
        case PLC_DATA_INT8:
            res = 8;
            break;
        case PLC_DATA_FLOAT4:
            res = 4;
            break;
        case PLC_DATA_FLOAT8:
            res = 8;
            break;
        case PLC_DATA_TEXT:
            res = 8;
            break;
        case PLC_DATA_ARRAY:
        case PLC_DATA_RECORD:
        case PLC_DATA_UDT:
        default:
            lprintf(ERROR, "Type %d cannot be passed plc_get_type_length function",
                    (int)dt);
            break;
    }
    return res;
}

static plcPyInputFunc plc_get_input_function(plcDatatype dt) {
    plcPyInputFunc res = NULL;
    switch (dt) {
        case PLC_DATA_INT1:
            res = plc_pyobject_from_int1;
            break;
        case PLC_DATA_INT2:
            res = plc_pyobject_from_int2;
            break;
        case PLC_DATA_INT4:
            res = plc_pyobject_from_int4;
            break;
        case PLC_DATA_INT8:
            res = plc_pyobject_from_int8;
            break;
        case PLC_DATA_FLOAT4:
            res = plc_pyobject_from_float4;
            break;
        case PLC_DATA_FLOAT8:
            res = plc_pyobject_from_float8;
            break;
        case PLC_DATA_TEXT:
            res = plc_pyobject_from_text;
            break;
        case PLC_DATA_ARRAY:
            res = plc_pyobject_from_array;
            break;
        case PLC_DATA_RECORD:
        case PLC_DATA_UDT:
        default:
            lprintf(ERROR, "Type %d cannot be passed plc_get_input_function function",
                    (int)dt);
            break;
    }
    return res;
}

static plcPyOutputFunc plc_get_output_function(plcDatatype dt) {
    plcPyOutputFunc res = NULL;
    switch (dt) {
        case PLC_DATA_INT1:
            res = plc_pyobject_as_int1;
            break;
        case PLC_DATA_INT2:
            res = plc_pyobject_as_int2;
            break;
        case PLC_DATA_INT4:
            res = plc_pyobject_as_int4;
            break;
        case PLC_DATA_INT8:
            res = plc_pyobject_as_int8;
            break;
        case PLC_DATA_FLOAT4:
            res = plc_pyobject_as_float4;
            break;
        case PLC_DATA_FLOAT8:
            res = plc_pyobject_as_float8;
            break;
        case PLC_DATA_TEXT:
            res = plc_pyobject_as_text;
            break;
        case PLC_DATA_ARRAY:
            res = plc_pyobject_as_array;
            break;
        case PLC_DATA_RECORD:
        case PLC_DATA_UDT:
        default:
            lprintf(ERROR, "Type %d cannot be passed plc_get_output_function function",
                    (int)dt);
            break;
    }
    return res;
}

static void plc_parse_type(plcPyType *pytype, plcType *type) {
    int i = 0;
    //pytype->name = strdup(type->name); TODO: implement type name to support UDTs
    pytype->name = strdup("results");
    pytype->type = type->type;
    pytype->nSubTypes = type->nSubTypes;
    pytype->subTypes = (plcPyType*)malloc(pytype->nSubTypes * sizeof(plcPyType));
    pytype->conv.inputfunc  = plc_get_input_function(pytype->type);
    pytype->conv.outputfunc = plc_get_output_function(pytype->type);
    for (i = 0; i < type->nSubTypes; i++) {
        pytype->conv.inputfunc  = plc_get_input_function(pytype->type);
        pytype->conv.outputfunc = plc_get_output_function(pytype->type);
        plc_parse_type(&pytype->subTypes[i], &type->subTypes[i]);
    }
}

plcPyFunction *plc_py_init_function(callreq call) {
    plcPyFunction *res;
    int i;

    res = (plcPyFunction*)malloc(sizeof(plcPyFunction));
    res->call = call;
    res->proc.src  = strdup(call->proc.src);
    res->proc.name = strdup(call->proc.name);
    res->nargs = call->nargs;
    res->args = (plcPyType*)malloc(res->nargs * sizeof(plcPyType));

    for (i = 0; i < res->nargs; i++)
        plc_parse_type(&res->args[i], &call->args[i].type);

    plc_parse_type(&res->res, &call->retType);

    return res;
}

plcPyResult *plc_init_result_conversions(plcontainer_result res) {
    plcPyResult *pyres = NULL;
    int i;

    pyres = (plcPyResult*)malloc(sizeof(plcPyResult));
    pyres->res = res;
    pyres->inconv = (plcPyTypeConv*)malloc(res->cols * sizeof(plcPyTypeConv));

    for (i = 0; i < res->cols; i++)
        pyres->inconv[i].inputfunc = plc_get_input_function(res->types[i].type);

    return pyres;
}

static void plc_py_free_type(plcPyType *type) {
    int i = 0;
    for (i = 0; i < type->nSubTypes; i++)
        plc_py_free_type(&type->subTypes[i]);
    if (type->nSubTypes > 0)
        free(type->subTypes);
}

void plc_py_free_function(plcPyFunction *func) {
    int i = 0;
    for (i = 0; i < func->nargs; i++)
        plc_py_free_type(&func->args[i]);
    free(func->args);
    free(func);
}

void plc_free_result_conversions(plcPyResult *res) {
    free(res->inconv);
    free(res);
}

void plc_py_copy_type(plcType *type, plcPyType *pytype) {
    type->type = pytype->type;
    type->nSubTypes = pytype->nSubTypes;
    if (type->nSubTypes > 0) {
        int i = 0;
        type->subTypes = (plcType*)pmalloc(type->nSubTypes * sizeof(plcType));
        for (i = 0; i < type->nSubTypes; i++)
            plc_py_copy_type(&type->subTypes[i], &pytype->subTypes[i]);
    } else {
        type->subTypes = NULL;
    }
}