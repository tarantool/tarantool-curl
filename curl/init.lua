--
--  Copyright (C) 2016 Tarantool AUTHORS: please see AUTHORS file.
--
--  Redistribution and use in source and binary forms, with or
--  without modification, are permitted provided that the following
--  conditions are met:
--
--  1. Redistributions of source code must retain the above
--     copyright notice, this list of conditions and the
--     following disclaimer.
--
--  2. Redistributions in binary form must reproduce the above
--     copyright notice, this list of conditions and the following
--     disclaimer in the documentation and/or other materials
--     provided with the distribution.
--
--  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
--  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
--  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
--  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
--  <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
--  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
--  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
--  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
--  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
--  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
--  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
--  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
--  SUCH DAMAGE.
--
local fiber = require'fiber'
local curl_driver = require('curl.driver')

local curl_mt

--
--  Create a new mosquitto client instance.
--
--  Parameters:
--  	id -            String to use as the client id. If NULL, a random client id
--  	                will be generated. If id is NULL, clean_session must be true.
--  	clean_session - set to true to instruct the broker to clean all messages
--                   and subscriptions on disconnect, false to instruct it to
--                   keep them. See the man page mqtt(7) for more details.
--                   Note that a client will never discard its own outgoing
--                   messages on disconnect. Calling <connect> or
--                   <reconnect> will cause the messages to be resent.
--                   Use <reinitialise> to reset a client to its
--                   original state.
--                   Must be set to true if the id parameter is NULL.
--
--  Returns:
--     mqtt object<see mqtt_mt> or raise error
--
local http = function()
  local curl = curl_driver.new()
  return setmetatable({
    VERSION = curl:version(), -- Str fmt: X.X.X
    POLL_INTERVAL = 0.0,

    curl = curl,
    fiber = nil,
  }, curl_mt)
end


local function read_cb(cnt, ctx)
    local res = ctx.readen:sub(1, cnt)
    ctx.readen = ctx.readen:sub(cnt + 1)
    return res
end

local function write_cb(data, ctx)
    return data:len()
end

local function done_cb(res, code, ctx)
    ctx.done = true
    ctx.res = res
    ctx.code = code
    fiber.wakeup(ctx.fiber)
end

local function sync_request(self, method, url, body, options)
    options = options or {}
    local ctx = {
        done = false,
	fiber = fiber.self(),
	written = '',
	readen = body}

    self.curl:async_request(method, url, {
        ca_path = options.ca_path,
        ca_file = options.ca_file,
        headers = options.headers,
        read = read_cb,
        write = write_cb,
        done = done_cb,
        ctx = ctx})

    while not ctx.done do
        fiber.sleep(1)
    end
    if ctx.res ~= 0 then
        return error(ctx.code)
    end
    return {code = ctx.code, body = ctx.written}
end

curl_mt = {

  __index = {

    request = function(self, method, url, options)
      return self.curl:async_request(method, url, options)
    end,
    sync_request = sync_request
  },
}

--
-- Export
--
return {
  http = http,
}
