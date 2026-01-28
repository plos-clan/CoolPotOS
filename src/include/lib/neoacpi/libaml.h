#pragma once

#include "neotype.h"

typedef enum {
    AML_OBJ_TYPE_UNINITIALIZED = 0,
    AML_OBJ_TYPE_INTEGER,
    AML_OBJ_TYPE_STRING,
    AML_OBJ_TYPE_BUFFER,
    AML_OBJ_TYPE_PACKAGE,
    AML_OBJ_TYPE_DEVICE,
    AML_OBJ_TYPE_METHOD,
    AML_OBJ_TYPE_REGION,
    AML_OBJ_TYPE_SCOPE,
    AML_OBJ_TYPE_UNKNOWN
} aml_obj_type_t;

typedef struct aml_node {
    char             name[5];
    aml_obj_type_t   type;
    struct aml_node *parent;
    struct aml_node *children;
    struct aml_node *next;
} aml_node_t;

typedef struct {
    const uint8_t *curr_ptr;
    const uint8_t *end_ptr;
    aml_node_t    *curr_scope;
} aml_context_t;
