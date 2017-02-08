-- {{
-- Util functions
function print_err(ok, msg, desc, crash)
    if not ok then
        print('[-] ' .. desc)
        error(msg)
        if crash then
            os.exit(1) 
        end
    else
        print('[+]' .. desc)
    end
end


function run(desc, func, param)
  print('Running', desc)
  ok, err = func(param)  
  print_err(ok, err, desc, true)
end
--
--}}


curl = require('curl')
local pool_size = 10
local http = curl.http({pool_size=pool_size})

--{{
-- Testing sync get calls
sync_funcs = {}
sync_funcs["get"] = {f = http.get, param = nil}
sync_funcs["request with GET"] = {f = http.request, param = "GET"}


json = require('json')

right_response_headers_host = "httpbin.org"
host_good = 'https://httpbin.org/get'
host_bad = 'https://httpbin.org/not_get'

function valid_response_get(value)
    local r = {}
    if not value.param then
        r = value.f(http, host_good)
    else
        r = value.f(http, value.param, host_good)
    end
    if r.code ~= 200  then
        return false, 'request is expecting 200, returned: ' .. r.code
    end
    if json.decode(r.body).headers.Host ~= right_response_headers_host then
        return false, "unexpected body answer: " .. r.body
    end
    return true, _
end

function invalid_response_get(value)
    local r = {}
    if not value.param then
        r = value.f(http, host_bad)
    else
        r = value.f(http, value.param, host_bad, '')
    end
    if r.code ~= 404  then
        return false, 'request is expecting 200, returned: ' .. r.code
    end
    return true, _
end

for key, value in pairs(sync_funcs)
    do
       run(key .. " valid request", valid_response_get, value)
       run(key .. " invalid request", invalid_response_get, value)
    end

--}}

--{{
-- Testing async 
function good_done_cb(curl_code, http_code, error_message, ctx)
    ctx.done = true
    if error_message ~= "No error" then 
        ctx.ok = false
        ctx.msg = error_message
    end
    if http_code ~= 200 then
        ctx.ok = false
        ctx.msg = 'request is expecting 200, returned: ' .. code
    end
end

function good_write_cb(data, ctx)
    if json.decode(data).headers.Host ~= right_response_headers_host then
        ctx.ok = false
        ctx.msg = "unexpected body answer: " .. data
    end
    return data:len()
end

function bad_done_cb(curl_code, http_code, error_message, ctx)
    ctx.done = true
    if code ~= 404 then
       ctx.ok = false 
       ctx.msg = 'request is expecting 404, returned: ' .. code
       error(res)
    end
end

async_funcs = {}
async_funcs["async_get"] = {f = http.async_get, param=nil} 
async_funcs["async_request with GET"] = {f = http.async_request, param = "GET"}

fiber = require('fiber')

function async_get(value)
    local wcb = nil
    local dcb = nil
    local rcb = function(cnt, ctx) end
    local host = nil
    if value.good then
        wcb = good_write_cb
        dcb = good_done_cb
        host = host_good
    else
        wcb = function(data, ctx) end
        dcb = bad_done_cb
        host = host_bad
    end
    local ctx = {}
    ctx.ok = true
    ctx.msg = nil
    local response = nil
    if value.param then
        response = value.f(http, value.param, host,
         {done=dcb, write=wcb, read=rcb, ctx=ctx})
    else
         response = value.f(http, host,
         {done=dcb, write=wcb, read=rcb, ctx=ctx}) 
    end
    local max_tick = 100 * 6
    local tick = 0
    while not ctx.done do
        if tick > max_tick then
            return false, "too long wait"
        end
        fiber.sleep(0.01)
        tick = tick + 1
    end
    if response ~= true then
        return false, response
    end
    return ctx.ok, ctx.msg
end

for key, value in pairs(async_funcs)
    do
        local tmp = value
        tmp.good = true
        run(key ..  " valid request", async_get, tmp)
        run(key ..  " invalid request", async_get, value)
    end
--}}

--{{
--Post tests
--
function write1(data, ctx)
    if json.decode(data)["headers"]["Content-Type"] ~= "json/application" then
        ctx.ok = false
        ctx.msg = "Unexpected body answer: " .. data
    end
    return data:len()
end


post_requests = {}
post_requests["post"] = {
    f = http.post, 
    param = nil, 
    host = "https://httpbin.org/post"
} 

post_requests["request with POST"] = {
    f = http.request, 
    param = "POST", 
    host = "https://httpbin.org/post"
} 

post_requests["put"] = {
    f = http.put,
    param = nil,
    host = "https://httpbin.org/put"
} 

post_requests["request with PUT"] = {
    f = http.request,
    param = "PUT",
    host = "https://httpbin.org/put"
} 

post_requests["async_put"] = {
    f = http.async_put,
    param = nil,
    host = "https://httpbin.org/put",
    async = true
} 

post_requests["async_post"] = {
    f = http.async_post, 
    param = nil, 
    host = "https://httpbin.org/post",
    async = true
} 

post_requests["async_request with PUT"] = {
    f = http.async_request,
    param = "PUT",
    host = "https://httpbin.org/put",
    async = true
} 

post_requests["async_request with POST"] = {
    f = http.async_request,
    param = "POST",
    host = "https://httpbin.org/post",
    async=true
}


value_header = "application/json"
headers = {["Content-Type"]= value_header}
function post_test(value)
    if value.async then
        local ctx = {}
        ctx.ok = true
        ctx.msg = nil

        if value.param then
            response = value.f(http, value.param, value.host, 
            {
                done=good_done_cb,
                write=write1,
                read = function(cnt, ctx1) end, 
                headers = headers, ctx=ctx
            })
        else
            response = value.f(http, value.host, 
            {
                done=good_done_cb,
                write=write1,
                read = function(cnt, ctx1) end, 
                headers = headers, ctx=ctx
            })
        end
        while not ctx.done
            do
                fiber.sleep(0.001)
            end
        if response ~= true then
            return false, response
        end
        return ctx.ok, ctx.msg
    else
        if value.param then
           local r = value.f(http, value.param, value.host, '',{headers=headers})
           if not r or r.code ~= 200 then
               return false, 'request is expecting 404, returned: ' .. r.code
           end
        
           if json.decode(r.body)["headers"]["Content-Type"] ~= value_header then
               return false, "Unexpected body answer: " .. r.body
           end
        else
            local r = value.f(http, value.host, '',{headers=headers})
            if not r or r.code ~= 200 then
                return false, 'request is expecting 404, returned: ' .. code
            end
        
            if json.decode(r.body)["headers"]["Content-Type"] ~= value_header then
               return false, "Unexpected body answer: " .. r.body
            end
        end
    end
    return true
end

for key, value in pairs(post_requests)
    do
        run("post/put test " .. key, post_test, value)
    end

--
--}}
--

--{{
--Stats tests

function check_stats(st, pst)
    assert(st.sockets_added == st.sockets_deleted)
    assert(st.active_requests == 0)
    assert(st.loop_calls > 0)
    assert(pst.pool_size == pool_size)
    assert(pst.free == pst.pool_size)
end

local old_stat = http:stat()
local old_pstat = http:pool_stat()
check_stats(old_stat, old_pstat)

http:get(host_good)

local new_stat = http:stat()
local new_pstat = http:pool_stat()
check_stats(new_stat, new_pstat)

if old_stat.failed_requests ~= new_stat.failed_requests then
    print("old stats: \n" .. json.encode(old_stat))
    print("new stats: \n" .. json.encode(new_stat))
    print_err(false, "stats failed_requests wrong", "sync correct request", true)
end

if old_stat.total_requests ~= new_stat.total_requests - 1 then 
    print_err(false, "stats total_requests wrong", "sync correct request", true)
end

if old_stat.http_200_responses ~= new_stat.http_200_responses - 1 then 
    print_err(false, "stats 200_responses wrong", "sync correct request", true)
end

old_stat = http:stat()
check_stats(old_stat, old_pstat)
http:get(host_bad)

new_stat = http:stat()
check_stats(new_stat, new_pstat)


if old_stat.failed_requests ~= new_stat.failed_requests then
    print("old stats: \n" .. json.encode(old_stat))
    print("new stats: \n" .. json.encode(new_stat))
    print_err(false, "stats failed_requests wrong", "sync bad request", true)
end

if old_stat.total_requests ~= new_stat.total_requests - 1 then 
    print_err(false, "stats total_requests wrong", "async bad request", true)
end

if old_stat.http_200_responses ~= new_stat.http_200_responses then 
    print_err(false, "stats 200_responses wrong", "async bad request", true)
end

print("[+] stats")

--
--}}
--
