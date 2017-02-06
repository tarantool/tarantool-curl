/*
 * Copyright (C) 2016 - 2017 Tarantool AUTHORS: please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef REQUEST_POOL_H_INCLUDED
#define REQUEST_POOL_H_INCLUDED 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <curl/curl.h>

struct curl_ctx_s;

typedef struct {

  /* pool meta info */
  struct {
    size_t idx;
    bool   busy;
  } pool;

  /** Information associated with a specific easy handle */
  CURL       *easy;

  /* Reference to curl context */
  struct curl_ctx_s *curl_ctx;

  /* Callbacks from lua and Lua context */
  struct {
    lua_State *L;
    int       read_fn;
    int       write_fn;
    int       done_fn;
    int       fn_ctx;
  } lua_ctx;

  /* HTTP headers */
  struct curl_slist *headers;
} request_t;

typedef struct {
  request_t  *mem;
  size_t     size;
} request_pool_t;


bool request_pool_new(request_pool_t *p, struct curl_ctx_s *c, size_t s);
void request_pool_free(request_pool_t *p);

request_t* request_pool_get_request(request_pool_t *p);
void request_pool_free_request(request_pool_t *p, request_t *c);
size_t request_pool_get_free_size(request_pool_t *p);

#endif /* REQUEST_POOL_H_INCLUDED */
