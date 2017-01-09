/*
 * Copyright (C) 2016-2017 Tarantool AUTHORS: please see AUTHORS file.
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "driver.h"

//#define MY_DEBUG 1
#include "debug.h"

struct easy_ctx
{
  lua_State *L;
  int read_fn;
  int write_fn;
  int done_fn;
  int fn_ctx;

  struct curl_slist *headers;
};


static
void
check_multi_info(curl_t *ctx)
{
  CURLMsg *msg;
  int msgs_left;

  while ((msg = curl_multi_info_read(ctx->multi, &msgs_left))) {

    if (msg->msg == CURLMSG_DONE) {

      dd("curl has new messages, messagess processing has started");

      CURL *easy = msg->easy_handle;
      CURLcode res = msg->data.result;
      curl_multi_remove_handle(ctx->multi, easy);

      struct easy_ctx *easy_ctx;
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &easy_ctx);

      if (easy_ctx->done_fn != LUA_REFNIL) {

        dd("the done callback is calling");

        lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX,
                    easy_ctx->done_fn);
        long response_code;
        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE,
                          &response_code);
        lua_pushinteger(easy_ctx->L, res);
        if (res)
          lua_pushstring(easy_ctx->L, curl_easy_strerror(res));
        else
          lua_pushinteger(easy_ctx->L, response_code);
        lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->fn_ctx);
        lua_pcall(easy_ctx->L, 3, 0 ,0);
      }

      luaL_unref(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->read_fn);
      luaL_unref(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->write_fn);
      luaL_unref(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->done_fn);
      luaL_unref(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->fn_ctx);

      if (easy_ctx->headers)
        curl_slist_free_all(easy_ctx->headers);

      curl_easy_cleanup(easy);

      free(easy_ctx);
    } /* msg done */
  } /* while */
}


static int
io_f(va_list ap)
{
  static const double sleeping_timeout =
#if defined (MY_DEBUG)
    0.2
#else
    0.01
#endif
    ;
  curl_t *ctx = va_arg(ap, curl_t *);

  fiber_set_cancellable(true);

  for (struct epoll_event events[10]; ctx->need_work;) {

    int rc = epoll_wait(ctx->epollfd, events, 10, 0);
    for (int i = 0; i < rc; ++i) {
      int mask = events[i].events & EPOLLIN ? CURL_POLL_IN : 0 |
                 events[i].events & EPOLLOUT ? CURL_POLL_OUT : 0;
      dd("events are processing on sfd:%d, mask:%d", events[i].data.fd, mask);
      curl_multi_socket_action(ctx->multi, events[i].data.fd, mask,
                               &ctx->still_running);
    }

    curl_multi_socket_action(ctx->multi, CURL_SOCKET_TIMEOUT, 0,
                             &ctx->still_running);
    check_multi_info(ctx);

    if (rc <= 0)
      fiber_sleep(sleeping_timeout);
  }

  return 0;
}


static size_t
read_cb(void *ptr, size_t size, size_t nmemb, struct easy_ctx *easy_ctx)
{
  size_t total_size = size * nmemb;

  dd("read_cb is calling size:%zu, nmemb:%zu", size, nmemb);

  if (!easy_ctx->read_fn)
    return total_size;

  lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->read_fn);
  lua_pushnumber(easy_ctx->L, total_size);
  lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->fn_ctx);
  lua_pcall(easy_ctx->L, 2, 1, 0);
  size_t readen;
  const char *data = lua_tolstring(easy_ctx->L, lua_gettop(easy_ctx->L),
                                   &readen);
  memcpy(ptr, data, readen);
  lua_pop(easy_ctx->L, 1);
  return readen;
}


static size_t
write_cb(void *ptr, size_t size, size_t nmemb, struct easy_ctx *easy_ctx)
{
  dd("write_cb is calling size:%zu, nmemb:%zu", size, nmemb);

  size_t bytes = size * nmemb;

  if (!easy_ctx->write_fn)
    return bytes;

  lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->write_fn);
  lua_pushlstring(easy_ctx->L, ptr, bytes);
  lua_rawgeti(easy_ctx->L, LUA_REGISTRYINDEX, easy_ctx->fn_ctx);
  lua_pcall(easy_ctx->L, 2, 1, 0);
  size_t written = lua_tointeger(easy_ctx->L, lua_gettop(easy_ctx->L));
  lua_pop(easy_ctx->L, 1);
  return written;
}


/*
 */
static int
async_request(lua_State *L)
{
  const char *reason = "unknown error";

  /* FIXME: check number and type of input arguments */
  CURL *easy = NULL;
  struct easy_ctx *easy_ctx = NULL;

  curl_t *ctx = curl_get(L);
  const char *method = luaL_checkstring(L, 2);
  const char *url = luaL_checkstring(L, 3);

  easy = curl_easy_init();
  if (!easy)
    return luaL_error(L, "curl_easy_init failed!");

  easy_ctx = (struct easy_ctx *)malloc(sizeof(struct easy_ctx));
  if (!easy_ctx) {
    reason = "can't allocate memry (easy_ctx)";
    goto err;
  }
  memset(easy_ctx, 0, sizeof(struct easy_ctx));

  easy_ctx->L = L;

  if (lua_istable(L, 4)) {
    int top = lua_gettop(L);
    /* FIXME: push string can fail, use safe version */
    lua_pushstring(L, "read");
    lua_gettable(L, 4);
    if (lua_isfunction(L, top + 1)) {
      easy_ctx->read_fn = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      easy_ctx->read_fn = LUA_REFNIL;
      lua_pop(L, 1);
    }

    lua_pushstring(L, "write");
    lua_gettable(L, 4);
    if (lua_isfunction(L, top + 1)) {
      easy_ctx->write_fn = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      easy_ctx->write_fn = LUA_REFNIL;
      lua_pop(L, 1);
    }

    lua_pushstring(L, "done");
    lua_gettable(L, 4);
    if (lua_isfunction(L, top + 1)) {
      easy_ctx->done_fn = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      easy_ctx->done_fn = LUA_REFNIL;
      lua_pop(L, 1);
    }

    lua_pushstring(L, "ctx");
    lua_gettable(L, 4);
    easy_ctx->fn_ctx = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_pushstring(L, "headers");
    lua_gettable(L, 4);
    if (!lua_isnil(L, top + 1)) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        char header[4096];
        snprintf(header, 4096, "%s: %s", lua_tostring(L, -2),
                                         lua_tostring(L, -1));
        easy_ctx->headers = curl_slist_append(easy_ctx->headers, header);
        if (!easy_ctx->headers) {
          reason = "can't allocate memry (curl_slist_append)";
          goto err;
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    lua_pushstring(L, "ca_path");
    lua_gettable(L, 4);
    if (!lua_isnil(L, top + 1))
      curl_easy_setopt(easy, CURLOPT_CAPATH, lua_tostring(L, top + 1));
    lua_pop(L, 1);

    lua_pushstring(L, "ca_file");
    lua_gettable(L, 4);
    if (!lua_isnil(L, top + 1))
      curl_easy_setopt(easy, CURLOPT_CAINFO, lua_tostring(L, top + 1));
    lua_pop(L, 1);

    lua_pushstring(L, "verbose");
    lua_gettable(L, 4);
    if (!lua_isnil(L, top + 1))
      curl_easy_setopt(easy, CURLOPT_VERBOSE, lua_tonumber(L, top + 1));
    lua_pop(L, 1);
  }

  curl_easy_setopt(easy, CURLOPT_PRIVATE, easy_ctx);

  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(easy, CURLOPT_READFUNCTION, read_cb);
  curl_easy_setopt(easy, CURLOPT_READDATA, easy_ctx);

  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(easy, CURLOPT_WRITEDATA, easy_ctx);

  curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1);

  curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 1);

  if (!strcmp(method, "POST")) {
    easy_ctx->headers = curl_slist_append(easy_ctx->headers, "Accept: */*");
    if (!easy_ctx->headers) {
      reason = "can't allocate memory (curl_slist_append)";
      goto err;
    }
    curl_easy_setopt(easy, CURLOPT_POST, 1);
  } else if (!strcmp(method, "PUT")) {
    easy_ctx->headers = curl_slist_append(easy_ctx->headers, "Accept: */*");
    if (!easy_ctx->headers) {
      reason = "can't allocate memory (curl_slist_append)";
      goto err;
    }
    curl_easy_setopt(easy, CURLOPT_UPLOAD, 1);
  } else if (!strcmp(method, "DELETE")) {
    /* FIXME: Do a custom rquest */
  } else if (!strcmp(method, "GET")) {
    curl_easy_setopt(easy, CURLOPT_HTTPGET, 1);
  } else {
    /* FIXME: */
  }

  if (easy_ctx->headers)
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, easy_ctx->headers);

  return curl_make_result(L,
    CURL_LAST,
    /* Note that the add_handle() will set a
     * time-out to trigger very soon so that
     * the necessary socket_action() call will be called
     * by this app */
    curl_multi_add_handle(ctx->multi, easy));

err:
  if (easy_ctx->headers)
    curl_slist_free_all(easy_ctx->headers);
  if (easy_ctx)
    free(easy_ctx);
  if (easy)
    curl_easy_cleanup(easy);
  return luaL_error(L, reason);
}



/**
 * Lib functions
 */

static
int
lib_version(lua_State *L)
{
  char version[sizeof("xxx.xxx.xxx-xx.xx")];

  snprintf(version, sizeof(version),
            "%i.%i.%i-%i.%i",
            LIBCURL_VERSION_MAJOR,
            LIBCURL_VERSION_MINOR,
            LIBCURL_VERSION_PATCH,
            0,
            1);

  return make_str_result(L, true, version);
}


static
int
sock_cb(CURL *easy,
        curl_socket_t sfd,
        int what,
        curl_t *ctx,
        struct epoll_event *event)
{
  (void)easy;
  bool epoll_new = false;

  switch (what) {
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
    if (!event) {
      epoll_new = true;
      event = (struct epoll_event *)calloc(1, sizeof(struct epoll_event));
      if (!event)
        goto err;
      curl_multi_assign(ctx->multi, sfd, event);
    }
    event->data.fd = sfd;
    event->events = what & CURL_POLL_IN ? EPOLLIN : 0 |
                    what & CURL_POLL_OUT ? EPOLLOUT : 0;
    if (-1 == epoll_ctl(ctx->epollfd, epoll_new ? EPOLL_CTL_ADD : EPOLL_CTL_MOD,
                  sfd,
                  event))
    {
      goto err;
    }
    break;
  case CURL_POLL_REMOVE:
    if (epoll_ctl(ctx->epollfd, EPOLL_CTL_DEL, sfd, NULL) == -1)
      goto err;
    curl_multi_assign(ctx->multi, sfd, NULL);
    if (event)
      free(event);
    break;
  }

  return 0;

err:
  curl_multi_assign(ctx->multi, sfd, NULL);
  epoll_ctl(ctx->epollfd, EPOLL_CTL_DEL, sfd, NULL);
  if (event)
      free(event);
  return 1;
}


static
int
lib_new(lua_State *L)
{
  const char *reason = "unknown error";

  curl_t *ctx = (curl_t *) lua_newuserdata(L, sizeof(curl_t));
  if (!ctx)
    return luaL_error(L, "lua_newuserdata failed!");

  memset(ctx, 0, sizeof(curl_t));
  ctx->epollfd = epoll_create(1);

  ctx->multi = curl_multi_init();
  if (!ctx->multi) {
    reason = "can't create 'multi'";
    goto error;
  }

  ctx->io_fiber = fiber_new("__curl_io_fiber", io_f);
  if (!ctx->io_fiber) {
    reason = "can't create new fiber: __curl_io_fiber";
    goto error;
  }

  curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
  curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETDATA, ctx);

  ctx->need_work = true;

  /* Run fibers */
  fiber_set_joinable(ctx->io_fiber, true);
  fiber_start(ctx->io_fiber, ctx);

  luaL_getmetatable(L, DRIVER_LUA_UDATA_NAME);
  lua_setmetatable(L, -2);

  ctx->L = L;

  return 1;

error:
  if (ctx->multi)
    curl_multi_cleanup(ctx->multi);
  if (ctx->io_fiber)
    fiber_cancel(ctx->io_fiber);
  if (ctx->epollfd)
    close(ctx->epollfd);
  return luaL_error(L, reason);
}


static
int
curl_destroy(lua_State *L)
{
  curl_t *ctx = curl_get(L);

  ctx->need_work = false;

  if (ctx->fiber) {
    fiber_cancel(ctx->fiber);
    fiber_join(ctx->fiber);
  }

  if (ctx->multi)
    curl_multi_cleanup(ctx->multi);

  /* remove all methods operating on ctx */
  lua_newtable(L);
  lua_setmetatable(L, -2);

  return make_int_result(L, true, 0);
}



/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
  {"version", lib_version},
  {"new",     lib_new},

  {NULL,      NULL}
};

static const struct luaL_Reg M[] = {

  {"destroy", curl_destroy},
  {"__gc",    curl_destroy},
  {"async_request", async_request},

  {NULL,    NULL}
};

/*
 * ]]
 */


/*
 * Lib initializer
 */
LUA_API
int
luaopen_curl_driver(lua_State *L)
{
  /**
   * Add metatable.__index = metatable
   */
  luaL_newmetatable(L, DRIVER_LUA_UDATA_NAME);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_register(L, NULL, M);
  luaL_register(L, NULL, R);

  /**
   * Add definitions
   */
  //register_defs(L, main_defs);

  return 1;
}
