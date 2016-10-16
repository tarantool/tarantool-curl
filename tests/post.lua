#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = '../curl/driver.so'
-- }}

local curl = require('curl')
local json = require('json')

http = curl.http()

local headers = { my_header = "1", my_header2 = "2" }
local json_body = json.encode({key="value"})

-- Sync request
local r = http:sync_get_request(
  'POST', 'https://tarantool.org', json_body
  {headers=headers})
if r.code ~= 200 then
  error('request is expecting 200')
end
print('server has responsed, data', r.body)

-- Aync request
local my_ctx = { out_buffer = json_body }

http:request('POST', 'tarantool.org', {
  read = function(cnt, ctx)
    local to_server = ctx.out_buffer:sub(1, cnt)
    ctx.out_buffer = ctx.out_buffer:sub(cnt + 1)
    return to_server
  end,
  write = function(data, ctx)
    print('server has responsed, data', data)
    return data:len()
  end,
  done = function(res, code, ctx)
    if cide ~= 200 then
      error('request is expecting 200')
    end
    print('server has responsed, statuses', res, code)
  end,
  ctx = my_ctx,
  headers = headers,
})
