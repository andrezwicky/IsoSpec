

#define ISOSPEC_BUILDING_PYTHON

#include "platform.h"

#if ISOSPEC_TEST_WE_ARE_ON_WINDOWS

#include <Python.h>

// Provide a dumy PyInit function on Windows/MSVC.
// We're not using it, as we'll load using CFFI - but it's easier
// than fighting with the build system.
PyObject* PyInit_modulename(void) { return nullptr; };

#endif

#include "unity-build.cpp"