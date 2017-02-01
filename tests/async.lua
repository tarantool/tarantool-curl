#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = 'curl/driver.so'
-- }}
--

box.cfg { }

-- Includes
local curl  = require('curl')
local fiber = require('fiber')
local json  = require('json')
local os    = require('os')

local headers   = { my_header = "1", my_header2 = "2" }
local my_body   = { key="value" }

local http = curl.http()

print(http.VERSION)

local function write(data, ctx)
    ctx.response = ctx.response .. data
    return data:len()
end

local function done(curl_code, http_code, error_msg, ctx)
    ctx.http_code = http_code
    ctx.curl_code = curl_code
    ctx.error_msg = error_msg
    ctx.done = true
end

local contexts = {}

contexts['GET'] = { http_code = 0,
                    curl_code = 0,
                    error_msg = '',
                    done = true,
                    response = '' }

contexts['POST']= {done          = false,
                   http_code     = 0,
                   curl_code     = 0,
                   error_message = '',
                   body          = json.encode(my_body),
                   response      = ''}

-- GET
local ok, msg = http:async_get('http://httpbin.org/get',
                                                {ctx = contexts['GET'],
                                                 headers = headers,
                                                 read = function(cnt, ctx)
                                                    return cnt:len()
                                                 end,
                                                 write = write,
                                                 done = done, } )
assert(ok)

-- POST
local ok, msg = http:async_post('http://httpbin.org/post',
                    {headers=headers,
                     keepalive_idle = 30,
                     keepalive_interval = 60,
                     read = function(cnt, ctx)
                        local res = ctx.body:sub(1, cnt)
                        ctx.body = ctx.body:sub(cnt + 1)
                        return res
                     end,
                     write = write,
                     done = done,
                     ctx = contexts['POST'],
                    })
assert(ok)

-- Join & tests
local ticks = 0
while http:stat().active_requests ~= 0 do
    if ticks > 60 then
        os.exit(1)
    end

    for _, ctx in ipairs(contexts) do
        if ctx.done then
            assert(ctx.curl_code == 0)
            assert(ctx.http_code == 200)
            local obody = json.decode(r.body)
            assert(obody.headers['My-Header'] == headers.my_header)
            assert(obody.headers['My-Header2'] == headers.my_header2)
        end
    end

    ticks = ticks + 2
    fiber.sleep(2)
end

local st = http:stat()
assert(st.socked_added ~= st.socket_deleted)
assert(st.active_requests == 0)
assert(st.loop_calls > 0)

http:free()

print('[+] async OK')

os.exit(0)
