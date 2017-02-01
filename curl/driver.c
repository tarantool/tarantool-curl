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

#include "driver.h"


static
int
curl_ev_f(va_list ap)
{
    tnt_lib_ctx_t *ctx = va_arg(ap, tnt_lib_ctx_t *);

    fiber_set_cancellable(true);

    for (;;) {
        if (ctx->done)
            break;
        lib_loop(ctx->lib_ctx, 0.0);
        fiber_sleep(0.001);
    }

    /** Finishing all requests
     */
    for (;;) {
        if (ctx->lib_ctx->stat.active_requests == 0)
            break;
        lib_loop(ctx->lib_ctx, 0.0);
        fiber_sleep(0.001);
    }

    return 0;
}


/*
 */
static
int
tnt_lib_async_request(lua_State *L)
{
    const char *reason = "unknown error";

    tnt_lib_ctx_t *ctx = ctx_get(L);
    if (ctx == NULL)
        return luaL_error(L, "can't get lib ctx");

    if (ctx->done)
        return luaL_error(L, "curl stopped");

    conn_t *c = new_conn(ctx->lib_ctx);
    if (c == NULL)
        return luaL_error(L, "can't allocate conn_t");

    conn_start_args_t conn_args;
    conn_start_args_init(&conn_args);

    const char *method = luaL_checkstring(L, 2);
    const char *url    = luaL_checkstring(L, 3);

    /** Set Options {{{
     */
    if (lua_istable(L, 4)) {

        const int top = lua_gettop(L);

        c->lua_ctx.L = L;

        /* Read callback */
        lua_pushstring(L, "read");
        lua_gettable(L, 4);
        if (lua_isfunction(L, top + 1))
            c->lua_ctx.read_fn = luaL_ref(L, LUA_REGISTRYINDEX);
        else
            lua_pop(L, 1);

        /* Write callback */
        lua_pushstring(L, "write");
        lua_gettable(L, 4);
        if (lua_isfunction(L, top + 1))
            c->lua_ctx.write_fn = luaL_ref(L, LUA_REGISTRYINDEX);
        else
            lua_pop(L, 1);

        /* Done callback */
        lua_pushstring(L, "done");
        lua_gettable(L, 4);
        if (lua_isfunction(L, top + 1))
            c->lua_ctx.done_fn = luaL_ref(L, LUA_REGISTRYINDEX);
        else
            lua_pop(L, 1);

        /* callback's context */
        lua_pushstring(L, "ctx");
        lua_gettable(L, 4);
        c->lua_ctx.fn_ctx = luaL_ref(L, LUA_REGISTRYINDEX);

        /** Http headers */
        lua_pushstring(L, "headers");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1)) {
            lua_pushnil(L);
            char header[4096];
            while (lua_next(L, -2) != 0) {
                snprintf(header, sizeof(header) - 1,
                        "%s: %s", lua_tostring(L, -2), lua_tostring(L, -1));
                if (!conn_add_header(c, header)) {
                    reason = "can't allocate memory (conn_add_header)";
                    goto error_exit;
                }
                lua_pop(L, 1);
            } // while
        }
        lua_pop(L, 1);

        /* SSL/TLS cert  {{{ */
        lua_pushstring(L, "ca_path");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
            curl_easy_setopt(c->easy, CURLOPT_CAPATH,
                             lua_tostring(L, top + 1));
        lua_pop(L, 1);

        lua_pushstring(L, "ca_file");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          curl_easy_setopt(c->easy, CURLOPT_CAINFO,
                           lua_tostring(L, top + 1));
        lua_pop(L, 1);
        /* }}} */

        lua_pushstring(L, "max_conns");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.max_conns = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "keepalive_idle");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.keepalive_idle = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "keepalive_interval");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.keepalive_interval = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "low_speed_limit");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.low_speed_limit = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "low_speed_time");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.low_speed_time = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "read_timeout");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.read_timeout = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "connect_timeout");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.connect_timeout = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        lua_pushstring(L, "dns_cache_timeout");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1))
          conn_args.dns_cache_timeout = (long) lua_tointeger(L, top + 1);
        lua_pop(L, 1);

        /* Debug- / Internal- options */
        lua_pushstring(L, "curl_verbose");
        lua_gettable(L, 4);
        if (!lua_isnil(L, top + 1) && lua_isboolean(L, top + 1))
          conn_args.curl_verbose = true;
        lua_pop(L, 1);
    }
    /* }}} */

    curl_easy_setopt(c->easy, CURLOPT_PRIVATE, (void *) c);

    curl_easy_setopt(c->easy, CURLOPT_URL, url);
    curl_easy_setopt(c->easy, CURLOPT_FOLLOWLOCATION, 1);

    curl_easy_setopt(c->easy, CURLOPT_SSL_VERIFYPEER, 1);

    /* Method {{{ */

    if (*method == 'G') {
      curl_easy_setopt(c->easy, CURLOPT_HTTPGET, 1);
    }
    else if (strcmp(method, "POST") == 0) {
        if (!conn_set_post(c)) {
            reason = "can't allocate memory (conn_set_post)";
            goto error_exit;
        }
    }
    else if (strcmp(method, "PUT") == 0) {
        if (!conn_set_put(c)) {
            reason = "can't allocate memory (conn_set_put)";
            goto error_exit;
        }
    } else {
        reason = "method does not supported";
        goto error_exit;
    }
    /* }}} */

    /* Note that the add_handle() will set a
     * time-out to trigger very soon so that
     * the necessary socket_action() call will be
     * called by this app */
    CURLMcode rc = conn_start(ctx->lib_ctx, c, &conn_args);
    if (rc != CURLM_OK)
        goto error_exit;

    return curl_make_result(L, CURL_LAST, rc);
error_exit:
    if (c)
        free_conn(c);
    return luaL_error(L, reason);
}


static
void
add_field_u64(lua_State *L, const char *key, uint64_t value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);  /* 3rd element from the stack top */
}


static
int
tnt_lib_stat(lua_State *L)
{
    tnt_lib_ctx_t *ctx = ctx_get(L);
    if (ctx == NULL)
        return luaL_error(L, "can't get lib ctx");

    lib_ctx_t *l = ctx->lib_ctx;
    if (l == NULL)
        return luaL_error(L, "it doesn't initialized");

    lua_newtable(L);

    add_field_u64(L, "active_requests", (uint64_t) l->stat.active_requests);
    add_field_u64(L, "socket_added", (uint64_t) l->stat.socket_added);
    add_field_u64(L, "socket_deleted", (uint64_t) l->stat.socket_deleted);
    add_field_u64(L, "loop_calls", (uint64_t) l->stat.loop_calls);
    add_field_u64(L, "total_requests", l->stat.total_requests);
    add_field_u64(L, "http_200_responses",  l->stat.http_200_responses);
    add_field_u64(L, "http_other_responses", l->stat.http_other_responses);
    add_field_u64(L, "failed_requests", (uint64_t) l->stat.failed_requests);

    return 1;
}


/** Lib functions {{{
 */
static
int
tnt_lib_version(lua_State *L)
{
  char version[sizeof("tarantool.curl: xxx.xxx.xxx") +
               sizeof("curl: xxx.xxx.xxx,") +
               sizeof("libev: xxx.xxx") ];

  snprintf(version, sizeof(version) - 1,
            "tarantool.curl: %i.%i.%i, curl: %i.%i.%i, libev: %i.%i",
            TNT_CURL_VERSION_MAJOR,
            TNT_CURL_VERSION_MINOR,
            TNT_CURL_VERSION_PATCH,

            LIBCURL_VERSION_MAJOR,
            LIBCURL_VERSION_MINOR,
            LIBCURL_VERSION_PATCH,

            EV_VERSION_MAJOR,
            EV_VERSION_MINOR );

  return make_str_result(L, true, version);
}


/** lib API {{{
 */

static void tnt_lib_free_(tnt_lib_ctx_t *ctx);

static
int
tnt_lib_new(lua_State *L)
{
    const char *reason = "unknown error";

    tnt_lib_ctx_t *ctx = (tnt_lib_ctx_t *)
            lua_newuserdata(L, sizeof(tnt_lib_ctx_t));
    if (ctx == NULL)
        return luaL_error(L, "lua_newuserdata failed: tnt_lib_ctx_t");

    ctx->lib_ctx = NULL;
    ctx->fiber   = NULL;
    ctx->done    = false;

    lib_new_args_t args = {.pipeline = false,
                           .max_conns = 5 };

    /* pipeline: 1 - on, 0 - off */
    args.pipeline  = (bool) luaL_checkint(L, 1);
    args.max_conns = luaL_checklong(L, 2);

    ctx->lib_ctx = lib_new(&args);
    if (ctx->lib_ctx == NULL) {
        reason = "lib_new failed";
        goto error_exit;
    }

    ctx->fiber = fiber_new("__curl_ev_fiber", curl_ev_f);
    if (ctx->fiber == NULL) {
        reason = "can't create new fiber: __curl_ev_fiber";
        goto error_exit;
    }

    /* Run fibers */
    fiber_set_joinable(ctx->fiber, true);
    fiber_start(ctx->fiber, (void *) ctx);

    luaL_getmetatable(L, DRIVER_LUA_UDATA_NAME);
    lua_setmetatable(L, -2);

    return 1;

error_exit:
    tnt_lib_free_(ctx);
    return luaL_error(L, reason);
}


static
void
tnt_lib_free_(tnt_lib_ctx_t *ctx)
{
    assert(ctx);

    ctx->done = true;

    if (ctx->fiber)
        fiber_join(ctx->fiber);

    if (ctx->lib_ctx)
        lib_free(ctx->lib_ctx);
}


static
int
tnt_lib_free(lua_State *L)
{
    tnt_lib_free_(ctx_get(L));

    /* remove all methods operating on ctx */
    lua_newtable(L);
    lua_setmetatable(L, -2);

    return make_int_result(L, true, 0);
}


/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
    {"version", tnt_lib_version},
    {"new",     tnt_lib_new},
    {NULL,      NULL}
};

static const struct luaL_Reg M[] = {
    {"async_request", tnt_lib_async_request},
    {"stat",          tnt_lib_stat},
    {"free",          tnt_lib_free},
    {"__gc",          tnt_lib_free},
    {NULL,            NULL}
};


/*
 * Lib initializer
 */
LUA_API
int
luaopen_curl_driver(lua_State *L)
{
    /*
        Add metatable.__index = metatable
    */
    luaL_newmetatable(L, DRIVER_LUA_UDATA_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, M);
    luaL_register(L, NULL, R);

    return 1;
}
