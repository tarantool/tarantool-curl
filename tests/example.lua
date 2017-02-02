#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = 'curl/driver.so'
-- }}
--

box.cfg {}

-- Includes
local curl  = require('curl')
local fiber = require('fiber')
local json  = require('json')
local os    = require('os')

local http = curl.http({pool_size=1})

print(http.VERSION)

local headers   = { my_header = "1", my_header2 = "2" }
local my_body   = { key="value" }
local json_body = json.encode(my_body)

-- Sync request
local r = http:get('https://tarantool.org/this/page/not/exists',
                   {headers=headers} )
assert(r.code == 404)
assert(r.body:len() ~= 0)

-- Sync requests {{{
local r = http:get('https://tarantool.org/', {headers=headers})
assert(r.code == 200)
assert(r.body:len() ~= 0)

local res = http:request('GET', 'mail.ru')
assert(r.code == 200)

local r = http:post('http://httpbin.org/post', json_body,
                    {headers=headers,
                     keepalive_idle = 30,
                     keepalive_interval = 60, })
assert(r.code == 200)
local obody = json.decode(r.body)
assert(obody.headers['My-Header'] == headers.my_header)
assert(obody.headers['My-Header2'] == headers.my_header2)
-- }}}

local st = http:stat()
assert(st.sockets_added == st.sockets_deleted)
assert(st.active_requests == 0)
assert(st.loop_calls > 0)

http:free()

print('[+] example OK')
os.exit(0)
