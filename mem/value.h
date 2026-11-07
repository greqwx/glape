// (c) Glape Contributors

#ifndef GLAPE_VALUE_H
#define GLAPE_VALUE_H

#include <stddef.h>
#include <stdio.h>

#include "mem/gc.h"
#include "mem/util.h"
#include "parse/ast.h"

typedef struct Env Env;
typedef struct Interpreter Interpreter;

typedef enum {
    EXEC_OK,
    EXEC_RETURN,
    EXEC_BREAK,
    EXEC_CONTINUE,
    EXEC_ERROR
} ExecStatus;

typedef struct Value Value;
typedef ExecStatus (*NativeFn)(Interpreter *interp, Value *args, size_t argc, Value *result);

typedef struct Native {
    const char *name;
    NativeFn fn;
} Native;

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_LIST,
    VAL_MAP,
    VAL_FUNCTION,
    VAL_NATIVE
} ValueType;

typedef struct ObjString ObjString;
typedef struct ObjList ObjList;
typedef struct ObjMap ObjMap;
typedef struct Function Function;

struct Value {
    ValueType type;
    union {
        int boolean;
        long long integer;
        double floating;
        ObjString *string;
        ObjList *list;
        ObjMap *map;
        Function *function;
        Native *native;
    } as;
};

struct ObjString {
    GcObject header;
    size_t length;
    char *chars;
};

struct ObjList {
    GcObject header;
    Value *items;
    size_t count;
    size_t capacity;
};

typedef struct MapEntry {
    ObjString *key;
    Value value;
} MapEntry;

struct ObjMap {
    GcObject header;
    MapEntry *entries;
    size_t count;
    size_t capacity;
};

struct Function {
    GcObject header;
    const char *name;
    const char *file;
    int line;
    int column;
    AstName *params;
    int may_fail;
    AstBlock body;
    Env *closure;
};

Value value_nil(void);
Value value_bool(int boolean);
Value value_int(long long integer);
Value value_float(double floating);
Value value_string(GcHeap *heap, const char *chars, size_t length);
Value value_list(GcHeap *heap);
Value value_map(GcHeap *heap);
Value value_function(Function *function);
Value value_native(Native *native);

int value_list_push(Value list, Value item);
int value_list_insert(Value list, size_t index, Value item);
int value_list_remove(Value list, size_t index, Value *out);
int value_map_set(Value map, ObjString *key, Value value);
int value_map_get(Value map, const char *key, size_t length, Value *out);
int value_map_remove(Value map, const char *key, size_t length, Value *out);

int value_truthy(Value value);
int value_equals(Value left, Value right);

/*
 * Compares two numeric values (int or float) exactly.
 * Returns -1, 0 or 1. Sets *unordered to 1 when either side is NaN.
 */
int value_number_compare(Value left, Value right, int *unordered);

const char *value_type_name(Value value);

int value_format(Value value, StrBuf *out);
void value_print(Value value, FILE *out);

#endif
