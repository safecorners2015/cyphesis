#include <stdio.h>
#include <unistd.h>

#include <Python.h>

#include "Python_API.h"
#include "Thing.h"

#include <modules/Location.h>
#include <server/WorldTime.h>
#include <common/const.h>

static void Function_dealloc(FunctionObject * self)
{
    PyMem_DEL(self);
}

static PyObject * log_debug(PyObject * self, PyObject * args, PyObject * foo)
{
    printf("LOG.DEBUG\n");
    Py_INCREF(Py_None);
    return Py_None;
}

PyTypeObject log_debug_type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"Function",
	sizeof(FunctionObject),
	0,
	/* methods */
	(destructor)Function_dealloc,
	0,		/* tp_print */
	0,		/* tp_getattr */
	0,		/* tp_setattr */
	0,		/* tp_compare */
	0,		/* tp_repr */
	0,		/* tp_as_number */
	0,		/* tp_as_sequence */
	0,		/* tp_as_mapping */
	0,		/* tp_hash */
	log_debug,	/* tp_call */
};

void Create_PyThing(Thing * thing, const string & package, const string & type)
{
    PyObject * mod_dict;
    if ((mod_dict = PyImport_ImportModule((char *)package.c_str()))==NULL) {
        cout << "Cld no find python module " << package << endl << flush;
            PyErr_Print();
        return;
    } else {
        cout << "Got python module " << package << endl << flush;
    }
    PyObject * my_class = PyObject_GetAttrString(mod_dict, (char *)type.c_str());
    if (my_class == NULL) {
        cout << "Cld no find class in module " << package << endl << flush;
            PyErr_Print();
        return;
    } else {
        cout << "Got python class " << type << " in " << package << endl << flush;
    }
    if (PyCallable_Check(my_class) == 0) {
            cout << "It does not seem to be a class at all" << endl << flush;
        return;
    }
    ThingObject * pyThing = newThingObject(NULL);
    pyThing->m_thing = thing;
    if (thing->set_object(PyEval_CallFunction(my_class,"(O)", (PyObject *)pyThing)) == -1) {
        if (PyErr_Occurred() == NULL) {
            cout << "Could not get python obj" << endl << flush;
        } else {
            cout << "Reporting python error for " << type << endl << flush;
            PyErr_Print();
        }
    }
}

static PyObject * location_new(PyObject * self, PyObject * args)
{
	LocationObject *o;
	// We need to deal with actual args here
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newLocationObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->location = new Location;
	return (PyObject *)o;
}

static PyObject * vector3d_new(PyObject * self, PyObject * args)
{
	Vector3DObject *o;
	// We need to deal with actual args here
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newVector3DObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	return (PyObject *)o;
}

static PyObject * worldtime_new(PyObject * self, PyObject * args)
{
	WorldTimeObject *o;
        	
        int seconds;
	if (!PyArg_ParseTuple(args, "i", &seconds)) {
                printf("AWWWWWWOOOOOGA");
		return NULL;
	}
	o = newWorldTimeObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->time = new WorldTime(seconds);
	return (PyObject *)o;
}

static PyObject * oplist_new(PyObject * self, PyObject * args)
{
	OplistObject *o;
	
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newOplistObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->ops = new oplist();
	return (PyObject *)o;
}

static PyObject * object_new(PyObject * self, PyObject * args)
{
	AtlasObject *o;
	
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newAtlasObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->m_obj = new Object;
	return (PyObject *)o;
}

static PyObject * cppthing_new(PyObject * self, PyObject * args)
{
	ThingObject *o;
	
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newThingObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	//o->m_thing = new Thing;
	return (PyObject *)o;
}

static PyObject * operation_new(PyObject * self, PyObject * args, PyObject * kwds)
{
    printf("New Operation\n");
    RootOperationObject * op;

    char * type;
    PyObject * to = NULL;
    PyObject * from = NULL;
    PyObject * arg1 = NULL;
    PyObject * arg2 = NULL;
    PyObject * arg3 = NULL;

    printf("New Operation: parsing args\n");
    if (!PyArg_ParseTuple(args, "s|OOO", &type, &arg1, &arg2, &arg3)) {
        return NULL;
    }
    printf("New Operation: creating operation\n");
    op = newAtlasRootOperation(args);
    if (op == NULL) {
        return NULL;
    }
    op->operation = new RootOperation;
    if (strcmp(type, "tick") == 0) {
        *op->operation = Tick::Instantiate();
    } else if (strcmp(type, "create") == 0) {
        *op->operation = Create::Instantiate();
    } else if (strcmp(type, "setup") == 0) {
        *op->operation = Setup::Instantiate();
    } else if (strcmp(type, "look") == 0) {
        *op->operation = Look::Instantiate();
    } else if (strcmp(type, "move") == 0) {
        *op->operation = Move::Instantiate();
    } else if (strcmp(type, "talk") == 0) {
        *op->operation = Talk::Instantiate();
    } else {
        printf("ERROR: PYTHON CREATING AN UNHANDLED OPERATION\n");
        *op->operation = RootOperation::Instantiate();
    }
    if (PyMapping_HasKeyString(kwds, "to")) {
        to = PyMapping_GetItemString(kwds, "to");
        printf("Operation creation sets to\n");
    }
    if (PyMapping_HasKeyString(kwds, "from_")) {
        from = PyMapping_GetItemString(kwds, "from_");
        printf("Operation creation sets from\n");
    }
    return (PyObject *)op;
}

static PyObject * set_kw(PyObject * meth_self, PyObject * args)
{
    // Takes self, kw, name, default=None
    PyObject * self;
    PyObject * kw;
    char * name;
    PyObject * def = NULL;

    if (!PyArg_ParseTuple(args, "OOs|O", &self, &kw, &name, &def)) {
        return NULL;
    }

    PyObject * attr = PyObject_GetAttrString(self, "attributes");
    if (attr == NULL) {
        PyErr_SetString(PyExc_TypeError, "SET_KW: No attributes list");
        return NULL;
    }
    int i = PyList_Size(attr);
    char * entry;
    PyObject * item;
    for(i= 0; i < PyList_Size(attr); i++) {
        item = PyList_GetItem(attr, i);
        if (!PyString_Check(item)) {
            continue;
        }
        entry = PyString_AsString(item);
        if (strcmp(entry, name) == 0) {
            goto list_contains_it;
        }
        // Should I free entry at this point?
    }
    PyList_Append(attr, PyString_FromString(name));
list_contains_it:
    if (!PyDict_Check(kw)) {
        PyErr_SetString(PyExc_TypeError, "SET_KW: kw not a dict");
        return NULL;
    }
    PyObject * value = NULL;
    if ((value = PyDict_GetItemString(kw, name)) == NULL) {
        PyObject * copy = PyDict_GetItemString(kw, "copy");
        if ((copy != NULL) && (PyObject_HasAttrString(copy, name))) {
            value = PyObject_GetAttrString(copy, name);
        } else {
            value = def;
        }
    }
    if (value == NULL) {
        value = Py_None;
        Py_INCREF(Py_None);
    }
    PyObject_SetAttrString(self, name, value);
    
    return NULL;
}

static PyMethodDef atlas_methods[] = {
    /* {"system",	spam_system, METH_VARARGS}, */
    {"Operation",  (PyCFunction)operation_new,	METH_VARARGS|METH_KEYWORDS},
    {"Location",   location_new,		METH_VARARGS},
    {"Object",     object_new,			METH_VARARGS},
    {"Message",    oplist_new,			METH_VARARGS},
    {"cppThing",   cppthing_new,		METH_VARARGS},
    {NULL,		NULL}				/* Sentinel */
};

static PyMethodDef Vector3D_methods[] = {
	{"Vector3D",	vector3d_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef server_methods[] = {
	{"WorldTime",	worldtime_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef common_methods[] = {
	//{"null",	null_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef misc_methods[] = {
	{"set_kw",	set_kw,		METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

void init_python_api()
{
	char * cwd;

	if ((cwd = getcwd(NULL, 0)) != NULL) {
                size_t len = strlen(cwd) * 2 + 60;
                char * pypath = (char *)malloc(len);
                strcpy(pypath, cwd);
                strcat(pypath, "/rulesets/basic");
                // This should eventually pull in a ruleset name from
                // the commandline args.
                // basic ruleset should always be left on the end
                // strcat(pypath, cwd);
                // strcat(pypath, "/rulesets/acorn");
		setenv("PYTHONPATH", pypath, 1);
	}

	Py_Initialize();

        //PyRun_SimpleString("from hooks import ruleset_import_hooks\n");
        //PyRun_SimpleString("ruleset_import_hooks.install(['basic','acorn'])\n");

	if (Py_InitModule("atlas", atlas_methods) == NULL) {
		printf("Failed to Create atlas thing\n");
		return;
	}
	printf("Created atlas thing\n");

	if (Py_InitModule("Vector3D", Vector3D_methods) == NULL) {
		printf("Failed to Create Vector3D thing\n");
		return;
	}
	printf("Created Vector3D thing\n");

        PyObject * misc;
        if ((misc = Py_InitModule("misc", misc_methods)) == NULL) {
		printf("Failed to Create misc thing\n");
		return;
	}

	PyObject * common;
	PyObject * dict;
	if ((common = Py_InitModule("common", common_methods)) == NULL) {
		printf("Failed to Create common thing\n");
		return;
	}
	printf("Created common thing\n");
	PyObject * _const = PyModule_New("const");
	PyObject * log = PyModule_New("log");
        PyObject * debug = (PyObject *)PyObject_NEW(FunctionObject, &log_debug_type);
	dict = PyModule_GetDict(common);
	PyDict_SetItemString(dict, "const", _const);
	PyDict_SetItemString(dict, "log", log);
	PyObject_SetAttrString(log, "debug", debug);
	//PyDict_SetItemString(dict, "misc", misc);
	PyObject_SetAttrString(_const, "server_python", PyInt_FromLong(0));
	PyObject_SetAttrString(_const, "debug_level",
			PyInt_FromLong(consts::debug_level));
	PyObject_SetAttrString(_const, "debug_thinking",
			PyInt_FromLong(consts::debug_thinking));

	PyObject_SetAttrString(_const, "time_multiplier",
			PyFloat_FromDouble(consts::time_multiplier));
	PyObject_SetAttrString(_const, "base_velocity_coefficient",
			PyFloat_FromDouble(consts::base_velocity_coefficient));
	PyObject_SetAttrString(_const, "base_velocity",
			PyFloat_FromDouble(consts::base_velocity));

	PyObject_SetAttrString(_const, "basic_tick",
			PyFloat_FromDouble(consts::basic_tick));
	PyObject_SetAttrString(_const, "day_in_seconds",
			PyInt_FromLong(consts::day_in_seconds));

	PyObject_SetAttrString(_const, "sight_range",
			PyFloat_FromDouble(consts::sight_range));
	PyObject_SetAttrString(_const, "hearing_range",
			PyFloat_FromDouble(consts::hearing_range));
	PyObject_SetAttrString(_const, "collision_range",
			PyFloat_FromDouble(consts::collision_range));
	PyObject_SetAttrString(_const, "enable_ranges",
			PyInt_FromLong(consts::enable_ranges));

	PyObject * server;
	if ((server = Py_InitModule("server", server_methods)) == NULL) {
		printf("Failed to Create server thing\n");
		return;
	}
	dict = PyModule_GetDict(server);
	PyObject * dictlist = PyModule_New("dictlist");
	PyDict_SetItemString(dict, "dictlist", dictlist);
	if (PyExc_IOError != NULL) {
		printf("Got PyExc_IOError\n");
	}
}
