#include <Python.h>
#include "fijs.h"

static void destroy_index_capsule(PyObject* capsule) {
    FIJS_Index* index = (FIJS_Index*)PyCapsule_GetPointer(capsule, "fijs._index_t");
    if (index) {
        fijs_index_destroy(index);
    }
}

static PyObject* _fijs_create(PyObject* self, PyObject* args) {
    const char* text;
    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    FIJS_Index* index = fijs_index_create(text);
    if (!index) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create FIJS index.");
        return NULL;
    }

    return PyCapsule_New(index, "fijs._index_t", destroy_index_capsule);
}

static PyObject* _fijs_search(PyObject* self, PyObject* args) {
    PyObject* index_capsule;
    PyObject* patterns_seq;

    if (!PyArg_ParseTuple(args, "OO", &index_capsule, &patterns_seq)) {
        return NULL;
    }

    FIJS_Index* index = (FIJS_Index*)PyCapsule_GetPointer(index_capsule, "fijs._index_t");
    if (!index) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a valid FIJS index object.");
        return NULL;
    }

    if (!PySequence_Check(patterns_seq)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be a sequence of strings.");
        return NULL;
    }

    Py_ssize_t num_patterns = PySequence_Length(patterns_seq);
    if (num_patterns < 0) {
        return NULL;
    }

    const char** patterns = PyMem_New(const char*, num_patterns);
    if (!patterns) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_patterns; ++i) {
        PyObject* item = PySequence_GetItem(patterns_seq, i);
        if (!item) {
            PyMem_Free(patterns);
            return NULL;
        }
        if (!PyUnicode_Check(item)) {
            Py_DECREF(item);
            PyMem_Free(patterns);
            PyErr_SetString(PyExc_TypeError, "Pattern sequence must contain only strings.");
            return NULL;
        }
        patterns[i] = PyUnicode_AsUTF8(item);
        Py_DECREF(item);
        if (!patterns[i]) {
            PyMem_Free(patterns);
            return NULL;
        }
    }

    FIJS_Result* results_head = fijs_search(index, patterns, (int)num_patterns);
    PyMem_Free(patterns);

    PyObject* result_dict = PyDict_New();
    if (!result_dict) {
        if (results_head) fijs_results_free(results_head);
        return PyErr_NoMemory();
    }

    FIJS_Result* current_res = results_head;
    while (current_res) {
        PyObject* locations_list = PyList_New(current_res->count);
        if (!locations_list) {
            Py_DECREF(result_dict);
            fijs_results_free(results_head);
            return PyErr_NoMemory();
        }
        for (size_t i = 0; i < current_res->count; ++i) {
            PyObject* loc = PyLong_FromSize_t(current_res->locations[i]);
            if (!loc) {
                Py_DECREF(locations_list);
                Py_DECREF(result_dict);
                fijs_results_free(results_head);
                return PyErr_NoMemory();
            }
            PyList_SET_ITEM(locations_list, i, loc);
        }
        PyDict_SetItemString(result_dict, current_res->pattern, locations_list);
        Py_DECREF(locations_list);
        current_res = current_res->next;
    }

    if (results_head) {
        fijs_results_free(results_head);
    }

    return result_dict;
}

static PyMethodDef _fijs_methods[] = {
    {"create", _fijs_create, METH_VARARGS, "Create a new FIJS index from text."},
    {"search", _fijs_search, METH_VARARGS, "Search the index for a list of patterns."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef _fijs_module = {
    PyModuleDef_HEAD_INIT,
    "_fijs_ext",
    "Internal C extension for the fijit library.",
    -1,
    _fijs_methods
};

PyMODINIT_FUNC PyInit__fijs_ext(void) {
    return PyModule_Create(&_fijs_module);
}