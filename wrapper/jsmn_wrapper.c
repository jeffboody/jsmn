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

#include <stdlib.h>

#define LOG_TAG "jsmn"
#include "../../libcc/cc_log.h"
#include "../../libcc/cc_memory.h"
#include "jsmn_wrapper.h"

// import the jsmn implementation
#define JSMN_STATIC
#include "../jsmn.h"

typedef struct
{
	// arguments
	const char* str;
	size_t      len;

	// parser
	jsmn_parser parser;
	int         pos;
	int         count;
	jsmntok_t*  tokens;
} jsmn_wrapper_t;

// forward declarations
static jsmn_object_t* jsmn_object_wrap(jsmn_wrapper_t* jw);
static void           jsmn_object_delete(jsmn_object_t** _self);
static jsmn_array_t*  jsmn_array_wrap(jsmn_wrapper_t* jw);
static void           jsmn_array_delete(jsmn_array_t** _self);
static jsmn_val_t*    jsmn_val_wrap(jsmn_wrapper_t* jw);
static jsmn_keyval_t* jsmn_keyval_wrap(jsmn_wrapper_t* jw);
static void           jsmn_keyval_delete(jsmn_keyval_t** _self);

/***********************************************************
* private                                                  *
***********************************************************/

static jsmn_wrapper_t*
jsmn_wrapper_new(const char* str, size_t len)
{
	ASSERT(str);

	jsmn_wrapper_t* self;
	self = (jsmn_wrapper_t*)
	       CALLOC(1, sizeof(jsmn_wrapper_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->str = str,
	self->len = len,

	jsmn_init(&self->parser);
	self->count = jsmn_parse(&self->parser, str, len, NULL, 0);
	if(self->count <= 0)
	{
		LOGE("jsmn_parse failed");
		goto fail_parse1;
	}

	self->tokens = (jsmntok_t*)
	               CALLOC(self->count, sizeof(jsmntok_t));
	if(self->tokens == NULL)
	{
		LOGE("CALLOC failed");
		goto fail_tokens;
	}

	jsmn_init(&self->parser);
	int count = jsmn_parse(&(self->parser), str, len,
	                       self->tokens, self->count);
	if(self->count != count)
	{
		LOGE("invalid count1=%i, count2=%i", self->count, count);
		goto fail_parse2;
	}

	// success
	return self;

	// failure
	fail_parse2:
		FREE(self->tokens);
	fail_tokens:
	fail_parse1:
		FREE(self);
	return NULL;
}

static void
jsmn_wrapper_delete(jsmn_wrapper_t** _self)
{
	ASSERT(_self);

	jsmn_wrapper_t* self = (jsmn_wrapper_t*) *_self;
	if(self)
	{
		FREE(self->tokens);
		FREE(self);
		*_self = NULL;
	}
}

static jsmntok_t*
jsmn_wrapper_step(jsmn_wrapper_t* self, char** _data)
{
	ASSERT(self);
	ASSERT(_data);
	ASSERT(*_data == NULL);

	if(self->pos >= self->count)
	{
		return NULL;
	}

	jsmntok_t* tok = &(self->tokens[self->pos]);
	if((tok->type == JSMN_UNDEFINED) ||
	   (tok->type == JSMN_STRING)    ||
	   (tok->type == JSMN_PRIMITIVE))
	{
		size_t len  = tok->end - tok->start;
		char*  data = CALLOC(len + 1, sizeof(char));
		if(data == NULL)
		{
			LOGE("CALLOC failed");
			return NULL;
		}

		int i;
		int start = tok->start;
		for(i = 0; i < len; ++i)
		{
			data[i] = self->str[start + i];
		}
		data[len] = '\0';

		*_data = data;
	}

	++self->pos;

	return tok;
}

static jsmntok_t* jsmn_wrapper_peek(jsmn_wrapper_t* self)
{
	ASSERT(self);

	if(self->pos >= self->count)
	{
		return NULL;
	}

	return &(self->tokens[self->pos]);
}

static jsmn_object_t* jsmn_object_wrap(jsmn_wrapper_t* jw)
{
	ASSERT(jw);

	jsmn_object_t* self;
	self = (jsmn_object_t*)
	       CALLOC(1, sizeof(jsmn_object_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->list = cc_list_new();
	if(self->list == NULL)
	{
		goto fail_list;
	}

	char*      str = NULL;
	jsmntok_t* tok = jsmn_wrapper_step(jw, &str);
	if(tok == NULL)
	{
		goto fail_tok;
	}

	if(tok->type != JSMN_OBJECT)
	{
		goto fail_type;
	}

	// parse kv pairs
	int i;
	jsmn_keyval_t* kv;
	for(i = 0; i < tok->size; ++i)
	{
		kv = jsmn_keyval_wrap(jw);
		if(kv == NULL)
		{
			goto fail_kv;
		}

		if(cc_list_append(self->list, NULL,
		                  (const void*) kv) == NULL)
		{
			goto fail_append;
		}
	}

	// success
	return self;

	// failure
	fail_append:
		jsmn_keyval_delete(&kv);
	fail_kv:
	{
		cc_listIter_t* iter;
		iter = cc_list_head(self->list);
		while(iter)
		{
			kv = (jsmn_keyval_t*)
			     cc_list_remove(self->list, &iter);
			jsmn_keyval_delete(&kv);
		}
	}
	fail_type:
		FREE(str);
	fail_tok:
		cc_list_delete(&self->list);
	fail_list:
		FREE(self);
	return NULL;
}

static void jsmn_object_delete(jsmn_object_t** _self)
{
	ASSERT(_self);

	jsmn_object_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->list);
		while(iter)
		{
			jsmn_keyval_t* kv;
			kv = (jsmn_keyval_t*)
			     cc_list_remove(self->list, &iter);
			jsmn_keyval_delete(&kv);
		}

		cc_list_delete(&self->list);
		FREE(self);
		*_self = NULL;
	}
}

static jsmn_array_t* jsmn_array_wrap(jsmn_wrapper_t* jw)
{
	ASSERT(jw);

	jsmn_array_t* self;
	self = (jsmn_array_t*)
	       CALLOC(1, sizeof(jsmn_array_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->list = cc_list_new();
	if(self->list == NULL)
	{
		goto fail_list;
	}

	char*      str = NULL;
	jsmntok_t* tok = jsmn_wrapper_step(jw, &str);
	if(tok == NULL)
	{
		goto fail_tok;
	}

	if(tok->type != JSMN_ARRAY)
	{
		goto fail_type;
	}

	// parse vals
	int i;
	jsmn_val_t* val;
	for(i = 0; i < tok->size; ++i)
	{
		val = jsmn_val_wrap(jw);
		if(val == NULL)
		{
			goto fail_val;
		}

		if(cc_list_append(self->list, NULL,
		                  (const void*) val) == NULL)
		{
			goto fail_append;
		}
	}

	// success
	return self;

	// failure
	fail_append:
		jsmn_val_delete(&val);
	fail_val:
	{
		cc_listIter_t* iter;
		iter = cc_list_head(self->list);
		while(iter)
		{
			val = (jsmn_val_t*)
			      cc_list_remove(self->list, &iter);
			jsmn_val_delete(&val);
		}
	}
	fail_type:
		FREE(str);
	fail_tok:
		cc_list_delete(&self->list);
	fail_list:
		FREE(self);
	return NULL;
}

static void jsmn_array_delete(jsmn_array_t** _self)
{
	ASSERT(_self);

	jsmn_array_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->list);
		while(iter)
		{
			jsmn_val_t* v;
			v = (jsmn_val_t*)
			    cc_list_remove(self->list, &iter);
			jsmn_val_delete(&v);
		}

		cc_list_delete(&self->list);
		FREE(self);
		*_self = NULL;
	}
}

static jsmn_val_t* jsmn_val_wrap(jsmn_wrapper_t* jw)
{
	ASSERT(jw);

	jsmn_val_t* self;
	self = (jsmn_val_t*)
	       CALLOC(1, sizeof(jsmn_val_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	jsmntok_t* tok = jsmn_wrapper_peek(jw);
	if(tok == NULL)
	{
		goto fail_tok;
	}

	// parse val
	char* str = NULL;
	if(tok->type == JSMN_OBJECT)
	{
		self->obj = jsmn_object_wrap(jw);
		if(self->obj == NULL)
		{
			goto fail_val;
		}
	}
	else if(tok->type == JSMN_ARRAY)
	{
		self->array = jsmn_array_wrap(jw);
		if(self->array == NULL)
		{
			goto fail_val;
		}
	}
	else
	{
		tok = jsmn_wrapper_step(jw, &str);
		if((tok == NULL) || (str == NULL))
		{
			goto fail_val;
		}

		self->data = str;
	}

	self->type = (jsmn_type_e) tok->type;

	// success
	return self;

	// failure
	fail_val:
		FREE(str);
	fail_tok:
		FREE(self);
	return NULL;
}

static jsmn_keyval_t* jsmn_keyval_wrap(jsmn_wrapper_t* jw)
{
	ASSERT(jw);

	jsmn_keyval_t* self;
	self = (jsmn_keyval_t*)
	       CALLOC(1, sizeof(jsmn_keyval_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	jsmntok_t* tok = jsmn_wrapper_step(jw, &self->key);
	if(tok == NULL)
	{
		goto fail_tok;
	}

	if(tok->type != JSMN_STRING)
	{
		goto fail_type;
	}

	// parse val
	self->val = jsmn_val_wrap(jw);
	if(self->val == NULL)
	{
		goto fail_val;
	}

	// success
	return self;

	// failure
	fail_val:
	fail_type:
		FREE(self->key);
	fail_tok:
		FREE(self);
	return NULL;
}

static void jsmn_keyval_delete(jsmn_keyval_t** _self)
{
	ASSERT(_self);

	jsmn_keyval_t* self = *_self;
	if(self)
	{
		jsmn_val_delete(&self->val);
		FREE(self->key);
		FREE(self);
		*_self = NULL;
	}
}

/***********************************************************
* public                                                   *
***********************************************************/

jsmn_val_t* jsmn_val_new(const char* str, size_t len)
{
	ASSERT(str);

	jsmn_wrapper_t* jw = jsmn_wrapper_new(str, len);
	if(jw == NULL)
	{
		return NULL;
	}

	jsmn_val_t* self = jsmn_val_wrap(jw);
	if(self == NULL)
	{
		goto fail_val;
	}

	jsmn_wrapper_delete(&jw);

	// success
	return self;

	// failure
	fail_val:
		jsmn_wrapper_delete(&jw);
	return NULL;
}

void jsmn_val_delete(jsmn_val_t** _self)
{
	ASSERT(_self);

	jsmn_val_t* self = *_self;
	if(self)
	{
		if(self->type == JSMN_TYPE_OBJECT)
		{
			jsmn_object_delete(&self->obj);
		}
		else if(self->type == JSMN_TYPE_ARRAY)
		{
			jsmn_array_delete(&self->array);
		}
		else
		{
			FREE(self->data);
		}

		FREE(self);
		*_self = NULL;
	}
}
