#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = 'curl/driver.so'
-- }}
--
box.cfg{}

-- Includes
local curl  = require('curl')
local yaml  = require('yaml')
local fiber = require('fiber')
local json  = require('json')

local http = curl.http()

print(http.VERSION)

local headers   = { my_header = "1", my_header2 = "2" }
local my_body   = { key="value" }
local json_body = json.encode(my_body)

-- Sync request
local r = http:sync_get_request('https://tarantool.org/this/page/not/exists',
                                {headers=headers} )
assert(r.code == 404)
assert(r.body:len() ~= 0)

-- Sync requests {{{
local r = http:sync_get_request('https://tarantool.org/',
                                {headers=headers} )
assert(r.code == 200)
assert(r.body:len() ~= 0)

local res = http:sync_request('GET', 'mail.ru')
assert(r.code == 200)

local r = http:sync_post_request('http://httpbin.org/post', json_body,
                                 {headers=headers,
                                  keepalive_idle = 30,
                                  keepalive_interval = 60, })
assert(r.code == 200)
local obody = json.decode(r.body)
assert(obody.headers['My-Header'] == headers.my_header)
assert(obody.headers['My-Header2'] == headers.my_header2)
-- }}}


-- Aync request {{{
local my_ctx = {http_code = 0,
                curl_code = 0,
                error_msg = '',
                fiber = fiber.self() }

local ok, msg = http:request('GET', 'tarantool.org', {
    read = function(cnt, ctx) end,
    write = function(data, ctx)
      print(data)
      return data:len()
    end,
    done = function(curl_code, http_code, error_msg, ctx)
      ctx.http_code = 200;
      ctx.curl_code = curl_code
      ctx.error_msg = error_msg
      fiber.wakeup(ctx.fiber)
    end,
    ctx = my_ctx,
    headers = headers,
} )

fiber.sleep(60)

assert(my_ctx.http_code == 200)
--- }}}

local st = http:stat()
assert(st.socked_added ~= st.socket_deleted)
assert(st.active_requests == 0)
assert(st.loop_calls > 0)

http:free()
