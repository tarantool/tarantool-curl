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
#include <inttypes.h>
#include <stdbool.h>

#include <ev.h>
#include <curl/curl.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "request_pool.h"

/** curl_ctx information, common to all requestections
 */
typedef struct curl_ctx_s curl_ctx_t;

struct curl_ctx_s {

  struct ev_loop  *loop;
  struct ev_timer timer_event;

  request_pool_t     cpool;

  CURLM           *multi;
  int             still_running;

  /* Various values of statistics, it are used only for all
   * requestection in curl context */
  struct {
    uint64_t      total_requests;
    uint64_t      http_200_responses;
    uint64_t      http_other_responses;
    size_t        failed_requests;
    size_t        active_requests;
    size_t        sockets_added;
    size_t        sockets_deleted;
    size_t        loop_calls;
  } stat;

};


typedef struct {

  /* Max amount of cached alive requestections */
  long max_conns;

  /* Non-universal keepalive knobs (Linux, AIX, HP-UX, more) */
  long keepalive_idle;
  long keepalive_interval;

  /* Set the "low speed limit & time"
     If the download receives less than "low speed limit" bytes/second during
     "low speed time" seconds, the operations is aborted. You could i.e if you
     have a pretty high speed requestection, abort if it is less than 2000
     bytes/sec during 20 seconds;
   */
  long low_speed_time;
  long low_speed_limit;

  /* Time-out the read operation after this amount of seconds */
  long read_timeout;

  /* Time-out connects operations after this amount of seconds, if connects are
     OK within this time, then fine... This only aborts the requestect phase. */
  long connect_timeout;

  /* DNS cache timeout */
  long dns_cache_timeout;

  /* Enable/Disable curl verbose mode */
  bool curl_verbose;
} request_start_args_t;


typedef struct {
  /* Set to true to enable pipelining for this multi handle */
  bool pipeline;

  /* Maximum number of entries in the requestection cache */
  long max_conns;

  size_t pool_size;
} curl_args_t;


/** Curl context API {{{
 */
curl_ctx_t* curl_ctx_new(const curl_args_t *a);
void curl_destroy(curl_ctx_t *l); /* curl_free exists! */
void curl_poll_one(curl_ctx_t *l);
void curl_print_stat(curl_ctx_t *l, FILE* out);

static inline
curl_ctx_t*
curl_ctx_new_easy(void) {
  const curl_args_t a = { .pipeline = false,
                          .max_conns = 5,
                          .pool_size = 1000 };
  return curl_ctx_new(&a);
}
/* }}} */

/** request API {{{
 */
static inline request_t *new_request(curl_ctx_t *ctx) {
  return request_pool_get_request(&ctx->cpool);
}

static inline void free_request(curl_ctx_t *ctx, request_t *r) {
    request_pool_free_request(&ctx->cpool, r);
}

CURLMcode request_start(request_t *c, const request_start_args_t *a);

#if defined (MY_DEBUG)
request_t* new_request_test(curl_ctx_t *l, const char *url);
#endif /* MY_DEBUG */

static inline
bool
request_add_header(request_t *c, const char *http_header)
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
request_add_header_keepaive(request_t *c, const request_start_args_t *a)
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
request_set_post(request_t *c)
{
  assert(c);
  assert(c->easy);
  if (!request_add_header(c, "Accept: */*"))
    return false;
  curl_easy_setopt(c->easy, CURLOPT_POST, 1L);
  return true;
}

static inline
bool
request_set_put(request_t *c)
{
  assert(c);
  assert(c->easy);
  if (!request_add_header(c, "Accept: */*"))
    return false;
  curl_easy_setopt(c->easy, CURLOPT_UPLOAD, 1L);
  return true;
}


static inline
void
request_start_args_init(request_start_args_t *a)
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

void request_start_args_print(const request_start_args_t *a, FILE *out);
/* }}} */

#endif /* CURL_WRAPPER_H_INCLUDED */
