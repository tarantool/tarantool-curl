--
--  Copyright (C) 2016-2017 Tarantool AUTHORS: please see AUTHORS file.
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
local fiber = require('fiber')
local curl_driver = require('curl.driver')
local yaml = require('yaml')

local curl_mt

--
--  Create a new curl instance.
--
--  Returns:
--     curl object or raise error
--
local http = function(VERBOSE)
  local curl = curl_driver.new()
  ok, version = curl:version()
  if not ok then
    version = '0.0.1'
  end
  return setmetatable({
    VERSION = version, -- Str fmt: X.X.X
    VERBOSE = VERBOSE,
    curl = curl,
  }, curl_mt)
end

--
-- Internal
-- {{
local function read_cb(cnt, ctx)
    local res = ctx.readen:sub(1, cnt)
    ctx.readen = ctx.readen:sub(cnt + 1)
    return res
end

local function write_cb(data, ctx)
    ctx.written = ctx.written .. data
    return data:len()
end

local function done_cb(res, code, ctx)
    ctx.done = true
    ctx.res = res
    ctx.code = code
    fiber.wakeup(ctx.fiber)
end
--
-- }}
--

--
--  <async_request> This function does HTTP request
--
--  Parameters:
--
--    method  - HTTP method, like GET, POST, PUT and so on
--    url     - HTTP url, like https://tarantool.org/doc
--    body    - this parameter is optional, you may use it for passing the
--              body to a server. Like 'My text string!'
--    options - this is a table of options.
--              ca_path - a path to ssl certificate dir;
--              ca_file - a path to ssl certificate file;
--              headers - a table of HTTP headers;
--              timeout - a deadline of this function execution.
--              If the deadline is expired then this function raise an error.
--              Default is 90 seconds.
--  Returns:
--     {code=NUMBER, body=STRING}
--
local function sync_request(self, method, url, body, options)

    options = options or {}

    local ctx = {
        done = false,
        fiber = fiber.self(),
        written = '',
        readen = body or '',
    }

    local headers = options.headers or {}

    --
    -- I have to set CL since CURL-engine works async
    if body then
        headers['Content-Length'] = body:len()
    end

    self.curl:async_request(method, url, {
        ca_path = options.ca_path,
        ca_file = options.ca_file,
        headers = headers,
        read = read_cb,
        write = write_cb,
        done = done_cb,
        ctx = ctx
    })

    while not ctx.done do
        fiber.sleep(0.001)
    end
    if ctx.res ~= 0 then
        return error(ctx.code)
    end

    return {code = ctx.code, body = ctx.written}
end

curl_mt = {
  __index = {
    --
    --  <async_request> This function does HTTP request
    --
    --  Parameters:
    --
    --    method  - HTTP method, like GET, POST, PUT and so on
    --    url     - HTTP url, like https://tarantool.org/doc
    --    options - this is a table of options.
    --              ca_path - a path to ssl certificate dir;
    --              ca_file - a path to ssl certificate file;
    --              headers - a table of HTTP headers.
    --                        NOTE if you pass a body,
    --                        then please you have to set Content-Length
    --                        header!
    --              curl:async_request(...,)
    --              timeout - a deadline of this function execution.
    --              If the deadline is expired then this function raise an error.
    --              Default is 90 seconds.
    --              done - a callback function which called only if request has
    --              completed.
    --              write - a callback. if a server returns some data, then
    --                      this function was benig called.
    --              Example:
    --                function(data, context)
    --                  context.in_buffer = context.in_buffer .. data
    --                  return data:len()
    --                end
    --
    --              read - a callback. if this client have to pass some
    --                     data to a server, then this function was benig called.
    --              Example:
    --                function(content_size, context)
    --                  local out_buffer = context.out_buffer
    --                  local to_server = out_buffer:sub(1, content_size)
    --                  context.out_buffer = out_buffer:sub(content_size + 1)
    --                  return to_server
    --                end
    --              done - a callback. if request has completed, then this
    --              function was being called.
    --
    --              ctx - this is a user defined context.
    --  Returns:
    --     0 or raise an error
    --
    request = function(self, method, url, options)
      local curl = self.curl
      if not method or not url or not options then
        error('function expects method, url and options')
      end
      return curl:async_request(method, url, options)
    end,

    --
    -- see <request>
    --
    get_request = function(self, url, options)
      return self.curl:async_request('GET', url, options)
    end,

    --
    -- See <sync_request>
    --
    sync_request = sync_request,

    --
    -- See <sync_request>
    --
    sync_post_request = function(self, url, body, options)
      return sync_request(self, 'POST', url, body, options)
    end,

    --
    -- See <sync_request>
    --
    sync_get_request = function(self, url, options)
      return sync_request(self, 'GET', url, '', options)
    end
  },
}

--
-- Export
--
return {
  http = http,
}
