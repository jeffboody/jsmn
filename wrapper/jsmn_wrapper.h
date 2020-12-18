/*
 * Copyright (c) 2020 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef jsmn_wrapper_H
#define jsmn_wrapper_H

#include "../../libcc/cc_list.h"

typedef enum
{
	JSMN_TYPE_UNDEFINED = 0,
	JSMN_TYPE_OBJECT    = 1,
	JSMN_TYPE_ARRAY     = 2,
	JSMN_TYPE_STRING    = 3,
	JSMN_TYPE_PRIMITIVE = 4
} jsmn_type_e;

typedef struct
{
	// list of keyval pairs
	cc_list_t* list;
} jsmn_object_t;

typedef struct
{
	// list of vals
	cc_list_t* list;
} jsmn_array_t;

typedef struct
{
	jsmn_type_e type;

	union
	{
		jsmn_object_t* obj;
		jsmn_array_t*  array;
		char*          data;
	};
} jsmn_val_t;

typedef struct
{
	char*       key;
	jsmn_val_t* val;
} jsmn_keyval_t;

jsmn_object_t* jsmn_object_new(const char* str,
                               size_t len);
void           jsmn_object_delete(jsmn_object_t** _self);

#endif
