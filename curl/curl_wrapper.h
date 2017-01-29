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

#ifndef CURL_WRAPPER_H_INCLUDED
#define CURL_WRAPPER_H_INCLUDED 1

#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include <ev.h>
#include <curl/curl.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/** lib_ctx information, common to all connections
 */
typedef struct {

  struct ev_loop  *loop;
  struct ev_timer timer_event;

  CURLM           *multi;
  int             still_running;

  struct {
    size_t        active_requests;
    size_t        socket_added;
    size_t        socket_deleted;
    size_t        loop_calls;
    size_t        socket_action_failed;
  } stat;

} lib_ctx_t;


/** Information associated with a specific easy handle
 */
typedef struct {
  CURL      *easy;
  lib_ctx_t *lib_ctx;
  char      error[CURL_ERROR_SIZE];

  struct {
    lua_State *L;
    int       read_fn;
    int       write_fn;
    int       done_fn;
    int       fn_ctx;
  } lua_ctx;

  struct curl_slist *headers;
} conn_t;

typedef struct {
  long max_conns;
  long keepalive_idle;
  long keepalive_interval;
  long low_speed_time;
  long low_speed_limit;
  long read_timeout;
  long connect_timeout;
  long dns_cache_timeout;
  bool curl_verbose;
} conn_start_args_t;


lib_ctx_t* lib_new(void);
void lib_free(lib_ctx_t *l);
void lib_loop(lib_ctx_t *l, double timeout);
void lib_print_stat(lib_ctx_t *l, FILE* out);

conn_t* new_conn(lib_ctx_t *l);
void free_conn(conn_t *c);
CURLMcode conn_start(lib_ctx_t *l, conn_t *c, const conn_start_args_t *a);
conn_t* new_conn_test(lib_ctx_t *l, const char *url);

static inline
bool
conn_add_header(conn_t *c, const char *http_header)
{
  assert(c);
  assert(http_header);
  struct curl_slist *l = curl_slist_append(c->headers, http_header);
  if (l == NULL)
    return false;
  c->headers = l;
  return true;
}

static inline
bool
conn_add_header_keepaive(conn_t *c, const conn_start_args_t *a)
{
  static char buf[255];

  assert(c);
  assert(a);

  snprintf(buf, sizeof(buf) - 1, "Keep-Alive: timeout=%d",
           (int) a->keepalive_idle);

  struct curl_slist *l = curl_slist_append(c->headers, buf);
  if (l == NULL)
    return false;

  c->headers = l;
  return true;
}

static inline
bool
conn_set_post(conn_t *c)
{
  assert(c);
  assert(c->easy);
  if (!conn_add_header(c, "Accept: */*"))
    return false;
  curl_easy_setopt(c->easy, CURLOPT_POST, 1);
  return true;
}

static inline
bool
conn_set_put(conn_t *c)
{
  assert(c);
  assert(c->easy);
  if (!conn_add_header(c, "Accept: */*"))
    return false;
  curl_easy_setopt(c->easy, CURLOPT_UPLOAD, 1);
  return true;
}


static inline
void
conn_start_args_init(conn_start_args_t *a)
{
  assert(a);
  a->max_conns = -1;
  a->keepalive_idle = -1;
  a->keepalive_interval = -1;
  a->low_speed_time = -1;
  a->low_speed_limit = -1;
  a->read_timeout = -1;
  a->connect_timeout = -1;
  a->dns_cache_timeout = -1;
  a->curl_verbose = false;
}

void conn_start_args_print(const conn_start_args_t *a, FILE *out);

#endif /* CURL_WRAPPER_H_INCLUDED */
