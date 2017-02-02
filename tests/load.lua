#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = 'curl/driver.so'
-- }}
--

-- Includes
box.cfg { slab_alloc_factor = 0.1 }

local curl  = require('curl')
local fiber = require('fiber')
local json  = require('json')

local num     = 10
local host    = '127.0.0.1:10000'
local curls   = { }
local headers = { }

-- Init [[
for i = 1, num do
  headers["My-header" .. i] = "my-value"
end

for i = 1, num do
  table.insert(curls, {url = host .. '/',
                       http = curl.http(),
                       body = json.encode({stat = box.stat(),
                                           info = box.info() }),
                       headers = headers,
                       connect_timeout = 5,
                       read_timeout = 5,
                       dns_cache_timeout = 1,
                      } )
end
-- ]]

-- Start test
for i = 1, num do

  local obj = curls[i]

  for j = 1, 100 do
      fiber.create(function()
          pcall(
            function()
              obj.http:post(obj.url, obj.body,
                            {headers = obj.headers,
                             keepalive_idle = 30,
                             keepalive_interval = 60,
                             connect_timeout = obj.connect_timeout,
                             read_timeout = obj.read_timeout,
                             dns_cache_timeout = obj.dns_cache_timeout, })
              obj.http:get(obj.url,
                            {headers = obj.headers,
                             keepalive_idle = 30,
                             keepalive_interval = 60,
                             connect_timeout = obj.connect_timeout,
                             read_timeout = obj.read_timeout,
                             dns_cache_timeout = obj.dns_cache_timeout, })
            end)
      end )
  end
end

-- Join test
fiber.create(function()

  local os   = require('os')
  local yaml = require('yaml')
  local rest = num

  ticks = 0

  while true do

    fiber.sleep(1)

    for i = 1, num do

      local obj = curls[i]

      if obj.http ~= nil and obj.http:stat().active_requests == 0 then
        local st = obj.http:stat()
        assert(st.sockets_added == st.sockets_deleted)
        assert(st.active_requests == 0)
        assert(st.loop_calls > 0)
        obj.http:free()
        rest = rest - 1
        curls[i].http = nil
      end

    end

    if rest <= 0 then
      break
    end

    ticks = ticks + 1

    -- Test failed
    if ticks > 80 then
      os.exit(1)
    end

  end

  print('[+] load OK')

  os.exit(0)
end )
