// (c) Glape Contributors

#ifndef GLAPE_REGISTRY_H
#define GLAPE_REGISTRY_H

#include "runtime/interp.h"

typedef ExecStatus (*LibInstallFn)(Interpreter *interp, Value *module);

typedef struct {
    const char *name;
    LibInstallFn install;
} LibDef;

ExecStatus libs_load(Interpreter *interp, const char *name, int line, int column);

#endif