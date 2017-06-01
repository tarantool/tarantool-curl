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

#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED 1

#include "curl_wrapper.h"
#include "utils.h"

/**
 * Unique name for userdata metatables
 */
#define DRIVER_LUA_UDATA_NAME	"__tnt_curl"
#define WORK_TIMEOUT 0.3
#define TNT_CURL_VERSION_MAJOR 2
#define TNT_CURL_VERSION_MINOR 2
#define TNT_CURL_VERSION_PATCH 8

typedef struct  {
    curl_ctx_t   *curl_ctx;
    struct fiber *fiber;
    bool         done;
} lib_ctx_t;


static inline
lib_ctx_t*
ctx_get(lua_State *L)
{
  return (lib_ctx_t *)
      luaL_checkudata(L, 1, DRIVER_LUA_UDATA_NAME);
}

static inline
int
curl_make_result(lua_State *L, CURLcode code, CURLMcode mcode)
{
  const char *emsg = NULL;
  if (code != CURL_LAST)
    emsg = curl_easy_strerror(code);
  else if (mcode != CURLM_LAST)
    emsg = curl_multi_strerror(mcode);
  return make_str_result(L,
        code != CURLE_OK,
        (emsg != NULL ? emsg : "ok"));
}

static inline
void
add_field_u64(lua_State *L, const char *key, uint64_t value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);  /* 3rd element from the stack top */
}

#endif /* DRIVER_H_INCLUDED */
