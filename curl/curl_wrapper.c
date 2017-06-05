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

#include "debug.h"
#include "curl_wrapper.h"

#include <stdlib.h>
#include <string.h>

/** Information associated with a specific socket
 */
typedef struct {
  CURL          *easy;
  curl_ctx_t    *curl_ctx;
  struct ev_io  ev;

  curl_socket_t sockfd;

  int           action;
  long          timeout;
  int           evset;
} sock_t;


#define is_mcode_good(mcode) is_mcode_good_(__FUNCTION__, (mcode))
static void timer_cb(EV_P_ struct ev_timer *w, int revents);


static inline
bool
is_mcode_good_(const char *where __attribute__((unused)),
               CURLMcode code)
{
    if (code == CURLM_OK)
        return true;

#if defined (MY_DEBUG)
    const char *s;

    switch(code) {
    case CURLM_BAD_HANDLE:
      s = "CURLM_BAD_HANDLE";
      break;
    case CURLM_BAD_EASY_HANDLE:
      s = "CURLM_BAD_EASY_HANDLE";
      break;
    case CURLM_OUT_OF_MEMORY:
      s = "CURLM_OUT_OF_MEMORY";
      break;
    case CURLM_INTERNAL_ERROR:
      s = "CURLM_INTERNAL_ERROR";
      break;
    case CURLM_UNKNOWN_OPTION:
      s = "CURLM_UNKNOWN_OPTION";
      break;
    case CURLM_LAST:
      s = "CURLM_LAST";
      break;
    default:
      s = "CURLM_unknown";
      break;
    case CURLM_BAD_SOCKET:
      s = "CURLM_BAD_SOCKET";
      /* ignore this error */
      return true;
    }

    dd("ERROR: %s returns = %s", where, s);
#else /* MY_DEBUG */
    if (code == CURLM_BAD_SOCKET)
      return true;
#endif

    return false;
}


/** Update the event timer after curl_multi library calls
 */
static
int
multi_timer_cb(CURLM *multi __attribute__((unused)),
              long timeout_ms,
              void *ctx)
{
    dd("timeout_ms = %li", timeout_ms);

    curl_ctx_t *l = (curl_ctx_t *) ctx;

    ev_timer_stop(l->loop, &l->timer_event);
    if (timeout_ms > 0) {
        ev_timer_init(&l->timer_event, timer_cb,
                      (double) (timeout_ms / 1000), 0.);
        ev_timer_start(l->loop, &l->timer_event);
    }
    else if (timeout_ms == 0) {
        ev_set_cb(&l->timer_event, timer_cb);
	ev_feed_event(l->loop, &l->timer_event, 0);
    }

    return 0;
}


/** Check for completed transfers, and remove their easy handles
 */
static
void
check_multi_info(curl_ctx_t *l)
{
    char       *eff_url;
    CURLMsg    *msg;
    int        msgs_left;
    request_t  *r;
    long       http_code;

    dd("REMAINING: still_running = %d", l->still_running);

    while ((msg = curl_multi_info_read(l->multi, &msgs_left))) {

        if (msg->msg != CURLMSG_DONE)
            continue;

        CURL     *easy     = msg->easy_handle;
        CURLcode curl_code = msg->data.result;

        curl_easy_getinfo(easy, CURLINFO_PRIVATE, (void *) &r);
        curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http_code);

        dd("DONE: url = %s, curl_code = %d, http_code = %d",
                eff_url, curl_code, (int) http_code);

        if (curl_code != CURLE_OK)
            ++l->stat.failed_requests;

        if (http_code == 200)
            ++l->stat.http_200_responses;
        else
            ++l->stat.http_other_responses;

        if (r->lua_ctx.done_fn != LUA_REFNIL) {
            /*
              Signature:
                function (curl_code, http_code, error_message, ctx)
            */
            lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.done_fn);
            lua_pushinteger(r->lua_ctx.L, (int) curl_code);
            lua_pushinteger(r->lua_ctx.L, (int) http_code);
            lua_pushstring(r->lua_ctx.L, curl_easy_strerror(curl_code));
            lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.fn_ctx);
            lua_pcall(r->lua_ctx.L, 4, 0 ,0);
        }

        free_request(l, r);
    } /* while */
}


/** Called by libevent when we get action on a multi socket
 */
static
void
event_cb(EV_P_ struct ev_io *w, int revents)
{
    (void) loop;

    dd("w = %p, revents = %d", (void *) w, revents);

    curl_ctx_t *l = (curl_ctx_t*) w->data;

    const int action = ( (revents & EV_READ ? CURL_POLL_IN : 0) |
                         (revents & EV_WRITE ? CURL_POLL_OUT : 0) );
    CURLMcode rc = curl_multi_socket_action(l->multi,
                                            w->fd, action, &l->still_running);
    if (!is_mcode_good(rc))
        ++l->stat.failed_requests;

    check_multi_info(l);

    if (l->still_running <= 0) {
        dd("last transfer done, kill timeout");
        ev_timer_stop(l->loop, &l->timer_event);
    }
}


/** Called by libevent when our timeout expires
 */
static
void
timer_cb(EV_P_ struct ev_timer *w, int revents __attribute__((unused)))
{
    (void) loop;

    dd("w = %p, revents = %i", (void *) w, revents);

    curl_ctx_t *l = (curl_ctx_t *) w->data;
    CURLMcode rc = curl_multi_socket_action(l->multi, CURL_SOCKET_TIMEOUT, 0,
                                            &l->still_running);
    if (!is_mcode_good(rc))
        ++l->stat.failed_requests;

    check_multi_info(l);
}

/** Clean up the sock_t structure
 */
static inline
void
remsock(sock_t *f, curl_ctx_t *l)
{
    dd("removing socket");

    if (f == NULL)
        return;

    if (f->evset)
        ev_io_stop(l->loop, &f->ev);

    ++l->stat.sockets_deleted;

    free(f);
}


/** Assign information to a sock_t structure
 */
static inline
void
setsock(sock_t *f,
        curl_socket_t s,
        CURL *e,
        int act,
        curl_ctx_t *l)
{
    dd("set new socket");

    const int kind = ( (act & CURL_POLL_IN ? EV_READ : 0) |
                       (act & CURL_POLL_OUT ? EV_WRITE : 0) );

    f->sockfd  = s;
    f->action  = act;
    f->easy    = e;
    f->ev.data = l;
    f->evset   = 1;

    if (f->evset)
        ev_io_stop(l->loop, &f->ev);

    ev_io_init(&f->ev, event_cb, f->sockfd, kind);
    ev_io_start(l->loop, &f->ev);
}


/** Initialize a new sock_t structure
 */
static
bool
addsock(curl_socket_t s, CURL *easy, int action, curl_ctx_t *l)
{
    sock_t *fdp = (sock_t *) malloc(sizeof(sock_t));
    if (fdp == NULL)
        return false;

    memset(fdp, 0, sizeof(sock_t));

    fdp->curl_ctx = l;

    setsock(fdp, s, easy, action, l);

    curl_multi_assign(l->multi, s, fdp);

    ++fdp->curl_ctx->stat.sockets_added;

    return true;
}


/* CURLMOPT_SOCKETFUNCTION */
static
int
sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
    curl_ctx_t *l = (curl_ctx_t*) cbp;
    sock_t    *fdp = (sock_t*) sockp;

#if defined(MY_DEBUG)
    static const char *whatstr[] = {
        "none", "IN", "OUT", "INOUT", "REMOVE" };
#endif /* MY_DEBUG */

    dd("e = %p, s = %i, what = %s, cbp = %p, sockp = %p",
            e, s, whatstr[what], cbp, sockp);

    if (what == CURL_POLL_REMOVE) {
        remsock(fdp, l);
    } else {
        if (fdp == NULL) {
            if (!addsock(s, e, what, l))
                return 1;
        }
        else {
            dd("Changing action from = %s, to = %s",
                    whatstr[fdp->action], whatstr[what]);
            setsock(fdp, s, e, what, l);
        }
    }

     return 0;
}


/** CURLOPT_WRITEFUNCTION / CURLOPT_READFUNCTION
 */
static
size_t
read_cb(void *ptr, size_t size, size_t nmemb, void *ctx)
{
    dd("size = %zu, nmemb = %zu", size, nmemb);

    request_t    *r         = (request_t *) ctx;
    const size_t total_size = size * nmemb;

    if (r->lua_ctx.read_fn == LUA_REFNIL)
        return total_size;

    lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.read_fn);
    lua_pushnumber(r->lua_ctx.L, total_size);
    lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.fn_ctx);
    lua_pcall(r->lua_ctx.L, 2, 1, 0);

    size_t readen;
    const char *data = lua_tolstring(r->lua_ctx.L,
                                     lua_gettop(r->lua_ctx.L), &readen);
    memcpy(ptr, data, readen);
    lua_pop(r->lua_ctx.L, 1);

    return readen;
}


static
size_t
write_cb(void *ptr, size_t size, size_t nmemb, void *ctx)
{
    dd("size = %zu, nmemb = %zu", size, nmemb);

    request_t    *r    = (request_t *) ctx;
    const size_t bytes = size * nmemb;

    if (r->lua_ctx.write_fn == LUA_REFNIL)
        return bytes;

    lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.write_fn);
    lua_pushlstring(r->lua_ctx.L, (const char *) ptr, bytes);
    lua_rawgeti(r->lua_ctx.L, LUA_REGISTRYINDEX, r->lua_ctx.fn_ctx);
    lua_pcall(r->lua_ctx.L, 2, 1, 0);
    const size_t written = lua_tointeger(r->lua_ctx.L,
                                         lua_gettop(r->lua_ctx.L));
    lua_pop(r->lua_ctx.L, 1);

    return written;
}


CURLMcode
request_start(request_t *r, const request_start_args_t *a)
{
    assert(r);
    assert(a);
    assert(r->easy);
    assert(r->curl_ctx);

    if (a->max_conns > 0)
        curl_easy_setopt(r->easy, CURLOPT_MAXCONNECTS, a->max_conns);

#if (LIBCURL_VERSION_MAJOR >= 7 && \
     LIBCURL_VERSION_MINOR >= 25 && \
     LIBCURL_VERSION_PATCH >= 0 )

    if (a->keepalive_idle > 0 && a->keepalive_interval > 0) {

        curl_easy_setopt(r->easy, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(r->easy, CURLOPT_TCP_KEEPIDLE, a->keepalive_idle);
        curl_easy_setopt(r->easy, CURLOPT_TCP_KEEPINTVL,
                                  a->keepalive_interval);
        if (!request_add_header(r, "Connection: Keep-Alive") &&
            !request_add_header_keepaive(r, a))
        {
            ++r->curl_ctx->stat.failed_requests;
            return CURLM_OUT_OF_MEMORY;
        }
    } else {
        if (!request_add_header(r, "Connection: close")) {
            ++r->curl_ctx->stat.failed_requests;
            return CURLM_OUT_OF_MEMORY;
        }
    }

#else /* > 7.25.0 */

    if (a->keepalive_idle > 0 && a->keepalive_interval > 0) { }

#endif

    if (a->read_timeout > 0)
        curl_easy_setopt(r->easy, CURLOPT_TIMEOUT_MS, a->read_timeout);

    if (a->connect_timeout > 0)
        curl_easy_setopt(r->easy, CURLOPT_CONNECTTIMEOUT_MS, a->connect_timeout);

    if (a->dns_cache_timeout > 0)
        curl_easy_setopt(r->easy, CURLOPT_DNS_CACHE_TIMEOUT,
                         a->dns_cache_timeout);

    if (a->curl_verbose)
        curl_easy_setopt(r->easy, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(r->easy, CURLOPT_PRIVATE, (void *) r);

    curl_easy_setopt(r->easy, CURLOPT_READFUNCTION, read_cb);
    curl_easy_setopt(r->easy, CURLOPT_READDATA, (void *) r);

    curl_easy_setopt(r->easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(r->easy, CURLOPT_WRITEDATA, (void *) r);

    curl_easy_setopt(r->easy, CURLOPT_NOPROGRESS, 1L);

    curl_easy_setopt(r->easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (a->low_speed_time > 0)
        curl_easy_setopt(r->easy, CURLOPT_LOW_SPEED_TIME, a->low_speed_time);

    if (a->low_speed_limit > 0)
        curl_easy_setopt(r->easy, CURLOPT_LOW_SPEED_LIMIT, a->low_speed_limit);

    /* Headers have to seted right before add_handle() */
    if (r->headers != NULL)
        curl_easy_setopt(r->easy, CURLOPT_HTTPHEADER, r->headers);

    ++r->curl_ctx->stat.total_requests;

    CURLMcode rc = curl_multi_add_handle(r->curl_ctx->multi, r->easy);
    if (!is_mcode_good(rc)) {
        ++r->curl_ctx->stat.failed_requests;
        return rc;
    }

    return rc;
}


#if defined (MY_DEBUG)
request_t*
new_request_test(curl_ctx_t *l, const char *url)
{
    request_t *r = new_request(l);
    if (r == NULL)
        return NULL;

    curl_easy_setopt(r->easy, CURLOPT_URL, url);

    request_start_args_t a;
    request_start_args_init(&a);

    a.keepalive_interval = 60;
    a.keepalive_idle = 120;
    a.read_timeout = 2;

    if (request_start(r, &a) != CURLM_OK)
        goto error_exit;

    return r;

error_exit:
    free_request(l, r);
    return NULL;
}
#endif /* MY_DEBUG */


curl_ctx_t*
curl_ctx_new(const curl_args_t *a)
{
    assert(a);

    curl_ctx_t *l = (curl_ctx_t *) malloc(sizeof(curl_ctx_t));
    if (l == NULL)
        return NULL;

    memset(l, 0, sizeof(curl_ctx_t));

    if (!request_pool_new(&l->cpool, l, a->pool_size))
        goto error_exit;

    l->loop = ev_loop_new(0);
    if (l->loop == NULL)
        goto error_exit;

    l->multi = curl_multi_init();
    if (l->multi == NULL)
        goto error_exit;

    ev_timer_init(&l->timer_event, timer_cb, 0., 0.);
    l->timer_event.data = (void *) l;

    curl_multi_setopt(l->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
    curl_multi_setopt(l->multi, CURLMOPT_SOCKETDATA, (void *) l);

    curl_multi_setopt(l->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(l->multi, CURLMOPT_TIMERDATA, (void *) l);

    if (a->pipeline)
        curl_multi_setopt(l->multi, CURLMOPT_PIPELINING, 1L /* pipline on */);

    if (a->max_conns > 0)
        curl_multi_setopt(l->multi, CURLMOPT_MAXCONNECTS, a->max_conns);

    return l;

error_exit:
    curl_destroy(l);
    return NULL;
}


void
curl_destroy(curl_ctx_t *l)
{
    if (l == NULL)
        return;

    if (l->multi != NULL)
        curl_multi_cleanup(l->multi);

    if (l->loop)
        ev_loop_destroy(l->loop);

    request_pool_free(&l->cpool);

    free(l);
}


void
curl_poll_one(curl_ctx_t *l)
{
    if (l == NULL)
        return;

    assert(l->loop);

    /*
        We don't call any curl_multi_socket*()
        function yet as we have no handles added!
    */

    ev_loop(l->loop, EVRUN_NOWAIT);

    ++l->stat.loop_calls;
}


void
curl_print_stat(curl_ctx_t *l, FILE* out)
{
    if (l == NULL)
        return;

    fprintf(out, "active_requests = %zu, sockets_added = %zu, "
                 "sockets_deleted = %zu, loop_calls = %zu, "
                 "total_requests = %llu, failed_requests = %llu, "
                 "http_200_responses = %llu, http_other_responses = %llu"
                 "\n",
            l->stat.active_requests,
            l->stat.sockets_added,
            l->stat.sockets_deleted,
            l->stat.loop_calls,
            (unsigned long long) l->stat.total_requests,
            (unsigned long long) l->stat.failed_requests,
            (unsigned long long) l->stat.http_200_responses,
            (unsigned long long) l->stat.http_other_responses
            );
}


void
request_start_args_print(const request_start_args_t *a, FILE *out)
{
  fprintf(out, "max_conns = %d, keepalive_idle = %d, keepalive_interval = %d, "
               "low_speed_time = %d, low_speed_limit = %d, curl_verbose = %d"
               "\n",
          (int) a->max_conns,
          (int) a->keepalive_idle,
          (int) a->keepalive_interval,
          (int) a->low_speed_time,
          (int) a->low_speed_limit,
          (int) a->curl_verbose );
}
