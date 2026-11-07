// (c) Glape Contributors

#include "value.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

Value value_nil(void)
{
    Value value;

    value.type = VAL_NIL;
    value.as.integer = 0;
    return value;
}

Value value_bool(int boolean)
{
    Value value;

    value.type = VAL_BOOL;
    value.as.boolean = boolean != 0;
    return value;
}

Value value_int(long long integer)
{
    Value value;

    value.type = VAL_INT;
    value.as.integer = integer;
    return value;
}

Value value_float(double floating)
{
    Value value;

    value.type = VAL_FLOAT;
    value.as.floating = floating;
    return value;
}

Value value_string(GcHeap *heap, const char *chars, size_t length)
{
    ObjString *string;
    Value value;

    if (length > (size_t)-1 - sizeof *string - 1)
        return value_nil();

    string = (ObjString *)gc_alloc(heap, sizeof *string + length + 1, GC_STRING);
    if (string == NULL)
        return value_nil();

    string->length = length;
    string->chars = (char *)(string + 1);

    if (length > 0)
        memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    value.type = VAL_STRING;
    value.as.string = string;
    return value;
}

Value value_list(GcHeap *heap)
{
    ObjList *list = (ObjList *)gc_alloc(heap, sizeof *list, GC_LIST);
    Value value;

    if (list == NULL)
        return value_nil();

    value.type = VAL_LIST;
    value.as.list = list;
    return value;
}

Value value_map(GcHeap *heap)
{
    ObjMap *map = (ObjMap *)gc_alloc(heap, sizeof *map, GC_MAP);
    Value value;

    if (map == NULL)
        return value_nil();

    value.type = VAL_MAP;
    value.as.map = map;
    return value;
}

Value value_function(Function *function)
{
    Value value;

    value.type = VAL_FUNCTION;
    value.as.function = function;
    return value;
}

Value value_native(Native *native)
{
    Value value;

    value.type = VAL_NATIVE;
    value.as.native = native;
    return value;
}

static int list_grow(ObjList *object)
{
    size_t capacity = object->capacity == 0 ? 4 : object->capacity * 2;
    Value *items;

    if (capacity < object->capacity || capacity > ((size_t)-1) / sizeof *items)
        return 0;

    items = (Value *)realloc(object->items, capacity * sizeof *items);
    if (items == NULL)
        return 0;

    object->items = items;
    object->capacity = capacity;
    gc_resize(&object->header, sizeof *object + capacity * sizeof *items);
    return 1;
}

int value_list_push(Value list, Value item)
{
    ObjList *object = list.as.list;

    if (object->count == object->capacity && !list_grow(object))
        return 0;

    object->items[object->count++] = item;
    return 1;
}

int value_list_insert(Value list, size_t index, Value item)
{
    ObjList *object = list.as.list;

    if (index > object->count)
        return 0;

    if (object->count == object->capacity && !list_grow(object))
        return 0;

    if (index < object->count)
        memmove(object->items + index + 1, object->items + index,
                (object->count - index) * sizeof *object->items);

    object->items[index] = item;
    object->count++;
    return 1;
}

int value_list_remove(Value list, size_t index, Value *out)
{
    ObjList *object = list.as.list;

    if (index >= object->count)
        return 0;

    *out = object->items[index];

    if (index + 1 < object->count)
        memmove(object->items + index, object->items + index + 1,
                (object->count - index - 1) * sizeof *object->items);

    object->count--;
    return 1;
}

static int map_find(ObjMap *map, const char *key, size_t length)
{
    size_t i;

    for (i = 0; i < map->count; i++) {
        ObjString *entry_key = map->entries[i].key;

        if (entry_key->length == length && memcmp(entry_key->chars, key, length) == 0)
            return (int)i;
    }

    return -1;
}

static int map_grow(ObjMap *object)
{
    size_t capacity = object->capacity == 0 ? 4 : object->capacity * 2;
    MapEntry *entries;

    if (capacity < object->capacity || capacity > ((size_t)-1) / sizeof *entries)
        return 0;

    entries = (MapEntry *)realloc(object->entries, capacity * sizeof *entries);
    if (entries == NULL)
        return 0;

    object->entries = entries;
    object->capacity = capacity;
    gc_resize(&object->header, sizeof *object + capacity * sizeof *entries);
    return 1;
}

int value_map_set(Value map, ObjString *key, Value value)
{
    ObjMap *object = map.as.map;
    int index = map_find(object, key->chars, key->length);

    if (index >= 0) {
        object->entries[index].value = value;
        return 1;
    }

    if (object->count == object->capacity && !map_grow(object))
        return 0;

    object->entries[object->count].key = key;
    object->entries[object->count].value = value;
    object->count++;
    return 1;
}

int value_map_get(Value map, const char *key, size_t length, Value *out)
{
    ObjMap *object = map.as.map;
    int index = map_find(object, key, length);

    if (index < 0)
        return 0;

    *out = object->entries[index].value;
    return 1;
}

int value_map_remove(Value map, const char *key, size_t length, Value *out)
{
    ObjMap *object = map.as.map;
    int index = map_find(object, key, length);

    if (index < 0)
        return 0;

    *out = object->entries[index].value;

    if ((size_t)index + 1 < object->count)
        memmove(object->entries + index, object->entries + index + 1,
                (object->count - (size_t)index - 1) * sizeof *object->entries);

    object->count--;
    return 1;
}

int value_truthy(Value value)
{
    switch (value.type) {
    case VAL_NIL:
        return 0;
    case VAL_BOOL:
        return value.as.boolean;
    case VAL_INT:
        return value.as.integer != 0;
    case VAL_FLOAT:
        return value.as.floating != 0.0;
    case VAL_STRING:
        return value.as.string->length > 0;
    case VAL_LIST:
        return value.as.list->count > 0;
    case VAL_MAP:
        return value.as.map->count > 0;
    default:
        return 1;
    }
}

static int number_value(Value value, double *out)
{
    if (value.type == VAL_INT) {
        *out = (double)value.as.integer;
        return 1;
    }

    if (value.type == VAL_FLOAT) {
        *out = value.as.floating;
        return 1;
    }

    return 0;
}

static int compare_int_float(long long integer, double floating, int *unordered)
{
    double integral;
    double fraction;

    if (floating != floating) {
        *unordered = 1;
        return 0;
    }

    if (floating > DBL_MAX)
        return -1;

    if (floating < -DBL_MAX)
        return 1;

    if (floating >= 9223372036854775808.0)
        return -1;

    if (floating < -9223372036854775808.0)
        return 1;

    fraction = modf(floating, &integral);

    {
        long long truncated = (long long)integral;

        if (integer < truncated)
            return -1;
        if (integer > truncated)
            return 1;
        if (fraction > 0.0)
            return -1;
        if (fraction < 0.0)
            return 1;
    }

    return 0;
}

int value_number_compare(Value left, Value right, int *unordered)
{
    *unordered = 0;

    if (left.type == VAL_INT && right.type == VAL_INT) {
        if (left.as.integer < right.as.integer)
            return -1;
        if (left.as.integer > right.as.integer)
            return 1;
        return 0;
    }

    if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
        if (left.as.floating != left.as.floating || right.as.floating != right.as.floating) {
            *unordered = 1;
            return 0;
        }

        if (left.as.floating < right.as.floating)
            return -1;
        if (left.as.floating > right.as.floating)
            return 1;
        return 0;
    }

    if (left.type == VAL_INT)
        return compare_int_float(left.as.integer, right.as.floating, unordered);

    return -compare_int_float(right.as.integer, left.as.floating, unordered);
}

int value_equals(Value left, Value right)
{
    if ((left.type == VAL_INT || left.type == VAL_FLOAT) &&
        (right.type == VAL_INT || right.type == VAL_FLOAT)) {
        int comparison;
        int unordered;

        comparison = value_number_compare(left, right, &unordered);
        return !unordered && comparison == 0;
    }

    if (left.type != right.type)
        return 0;

    switch (left.type) {
    case VAL_NIL:
        return 1;
    case VAL_BOOL:
        return left.as.boolean == right.as.boolean;
    case VAL_STRING:
        if (left.as.string->length != right.as.string->length)
            return 0;
        return memcmp(left.as.string->chars, right.as.string->chars, left.as.string->length) == 0;
    case VAL_LIST:
        return left.as.list == right.as.list;
    case VAL_MAP:
        return left.as.map == right.as.map;
    case VAL_FUNCTION:
        return left.as.function == right.as.function;
    case VAL_NATIVE:
        return left.as.native == right.as.native;
    }

    return 0;
}


const char *value_type_name(Value value)
{
    switch (value.type) {
    case VAL_NIL:
        return "nil";
    case VAL_BOOL:
        return "bool";
    case VAL_INT:
        return "int";
    case VAL_FLOAT:
        return "float";
    case VAL_STRING:
        return "string";
    case VAL_LIST:
        return "list";
    case VAL_MAP:
        return "map";
    case VAL_FUNCTION:
        return "function";
    case VAL_NATIVE:
        return "native";
    }

    return "unknown";
}

#define VALUE_FORMAT_MAX_DEPTH 64

static int format_value(Value value, StrBuf *out, const void *stack[], int depth);

static int stack_contains(const void *stack[], int depth, const void *pointer)
{
    int i;

    for (i = 0; i < depth; i++) {
        if (stack[i] == pointer)
            return 1;
    }

    return 0;
}

static int format_value(Value value, StrBuf *out, const void *stack[], int depth)
{
    switch (value.type) {
    case VAL_NIL:
        return strbuf_append_cstr(out, "nil");

    case VAL_BOOL:
        return strbuf_append_cstr(out, value.as.boolean ? "true" : "false");

    case VAL_INT: {
        char buffer[32];

        snprintf(buffer, sizeof buffer, "%lld", value.as.integer);
        return strbuf_append_cstr(out, buffer);
    }

    case VAL_FLOAT: {
        char buffer[64];
        double number = value.as.floating;

        if (number != number)
            return strbuf_append_cstr(out, "nan");

        if (number > DBL_MAX)
            return strbuf_append_cstr(out, "inf");

        if (number < -DBL_MAX)
            return strbuf_append_cstr(out, "-inf");

        snprintf(buffer, sizeof buffer, "%g", number);
        return strbuf_append_cstr(out, buffer);
    }

    case VAL_STRING:
        return strbuf_append(out, value.as.string->chars, value.as.string->length);

    case VAL_LIST: {
        size_t i;

        if (depth >= VALUE_FORMAT_MAX_DEPTH || stack_contains(stack, depth, value.as.list))
            return strbuf_append_cstr(out, "[...]");

        stack[depth] = value.as.list;

        if (!strbuf_append_char(out, '['))
            return 0;

        for (i = 0; i < value.as.list->count; i++) {
            if (i > 0 && !strbuf_append_cstr(out, ", "))
                return 0;
            if (!format_value(value.as.list->items[i], out, stack, depth + 1))
                return 0;
        }

        return strbuf_append_char(out, ']');
    }

    case VAL_MAP: {
        size_t i;

        if (depth >= VALUE_FORMAT_MAX_DEPTH || stack_contains(stack, depth, value.as.map))
            return strbuf_append_cstr(out, "{...}");

        stack[depth] = value.as.map;

        if (!strbuf_append_char(out, '{'))
            return 0;

        for (i = 0; i < value.as.map->count; i++) {
            MapEntry *entry = &value.as.map->entries[i];

            if (i > 0 && !strbuf_append_cstr(out, ", "))
                return 0;
            if (!strbuf_append(out, entry->key->chars, entry->key->length))
                return 0;
            if (!strbuf_append_cstr(out, ": "))
                return 0;
            if (!format_value(entry->value, out, stack, depth + 1))
                return 0;
        }

        return strbuf_append_char(out, '}');
    }

    case VAL_FUNCTION:
        return strbuf_append_cstr(out, "<function ") &&
               strbuf_append_cstr(out, value.as.function->name) && strbuf_append_char(out, '>');

    case VAL_NATIVE:
        return strbuf_append_cstr(out, "<native ") &&
               strbuf_append_cstr(out, value.as.native->name) && strbuf_append_char(out, '>');
    }

    return 0;
}

int value_format(Value value, StrBuf *out)
{
    const void *stack[VALUE_FORMAT_MAX_DEPTH];

    return format_value(value, out, stack, 0);
}


void value_print(Value value, FILE *out)
{
    StrBuf buffer;

    strbuf_init(&buffer);

    if (value_format(value, &buffer) && buffer.data != NULL)
        fputs(buffer.data, out);

    strbuf_free(&buffer);
}
