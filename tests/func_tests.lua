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
http = curl.http()

--{{
-- Testing sync get calls
sync_funcs = {}
sync_funcs["sync_get_request"] = {f = http.sync_get_request, param=nil} 
sync_funcs["sync_request with GET"] = {f = http.sync_request, param = "GET"}



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
--
--}}

--{{
-- Testing async 
function good_done_cb(res, code, ctx)
    ctx.done = true
    if code ~= 200 then
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

function bad_done_cb(res, code, ctx)
    ctx.done = true
    if code ~= 404 then
       ctx.ok = false 
       ctx.msg = 'request is expecting 404, returned: ' .. code
       error(res)
    end
end

async_funcs = {}
async_funcs["get_request"] = {f = http.get_request, param=nil} 
async_funcs["request with GET"] = {f = http.request, param = "GET"}

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
    while not ctx.done do
        fiber.sleep(0.001)
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

function write1(data, ctx)
    if json.decode(data)["headers"]["Content-Type"] ~= "json/application" then
        ctx.ok = false
        ctx.msg = "Unexpected body answer: " .. data
    end
    return data:len()
end


post_requests = {}
post_requests["sync_post_request"] = {
    f = http.sync_post_request, 
    param = nil, 
    host = "https://httpbin.org/post"
} 

post_requests["sync_request with POST"] = {
    f = http.sync_request, 
    param = "POST", 
    host = "https://httpbin.org/post"
} 

post_requests["sync_request with PUT"] = {
    f = http.sync_request,
    param = "PUT",
    host = "https://httpbin.org/put"
} 

post_requests["async_request with PUT"] = {
    f = http.request,
    param = "PUT",
    host = "https://httpbin.org/put",
    async=true
} 

post_requests["async_request with POST"] = {
    f = http.request,
    param = "POST",
    host = "https://httpbin.org/post",
    async=true
}


value_header = "json/application"
headers = {["Content-Type"]= value_header}
function post_test(value)
    if value.async then
        local ctx = {}
        ctx.ok = true
        ctx.msg = nil

       response = value.f(http, value.param, value.host, 
       {
           done=good_done_cb,
           write=write1,
           read = function(cnt, ctx1) end, 
           headers = headers, ctx=ctx
       })

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
        run("post test " .. key, post_test, value)
    end
