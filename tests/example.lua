#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
-- package.preload['curl.driver'] = '../curl/driver.so'
-- }}

local curl = require('curl')

http = curl.http()

local headers = { my_header = "1", my_header2 = "2" }

-- Sync request
--local r = http:sync_get_request('https://ya.ru', {headers=headers})
--if r.code ~= 200 then
--  error('request is expecting 200')
--end
--print('server has responsed, data', r.code)
--
-- Async request
local my_ctx = {}

http:request('PUT', 'https://httpbin.org/put', {
  read = function(cnt, ctx) end,
  write = function(data, ctx)
    print('server has responsed, data \n'.. data)
    return data:len()
  end,
  done = function(res, code, ctx)
    ctx.done = true  
    if code ~= 200 then
      error('request is expecting 200')
    end
    print('server has responsed, statuses', code)
  end,
  ctx = my_ctx,
  headers = headers,
})

fiber = require('fiber')
while not my_ctx.done do
    fiber.sleep(0.001)
end
print ("Reached")
--res = http:sync_request('GET', 'mail.ru')
--print('GET', res.body)
--res = http:sync_request('PUT', 'www.rbc.ru', '{data: 123}',
--  {headers = {['Content-type'] = 'application/json'}})
--print('PUT', res.body)
