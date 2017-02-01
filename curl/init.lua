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

local fiber       = require('fiber')
local curl_driver = require('curl.driver')
local yaml        = require('yaml')

local curl_mt

--
--  Create a new curl instance.
--
--  Returns:
--     curl object or raise error
--
local http = function(pipeline, max_conns)
  curl = curl_driver.new(pipeline or 0, max_conns or 5)
  local ok, version = curl:version()
  if not ok then
    error("can't get curl:version()")
  end

  return setmetatable({VERSION     = version,
                       curl        = curl, },
                       curl_mt )
end

--
-- Internal {{{
local function read_cb(cnt, ctx)
    local res = ctx.readen:sub(1, cnt)
    ctx.readen = ctx.readen:sub(cnt + 1)
    return res
end

local function write_cb(data, ctx)
    ctx.written = ctx.written .. data
    return data:len()
end

local function done_cb(curl_code, http_code, error_message, ctx)
    ctx.done          = true
    ctx.http_code     = http_code
    ctx.curl_code     = curl_code
    ctx.error_message = error_message
    fiber.wakeup(ctx.fiber)
end
-- }}}

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
--              ca_path                             - a path to ssl certificate dir;
--              ca_file                             - a path to ssl certificate file;
--              headers                             - a table of HTTP headers;
--              max_conns                           - max amount of cached alive connections;
--              keepalive_idle & keepalive_interval - non-universal keepalive knobs (Linux, AIX, HP-UX, more);
--              low_speed_time & low_speed_limit    - If the download receives less than "low speed limit" bytes/second
--                                                    during "low speed time" seconds, the operations is aborted.
--                                                    You could i.e if you have a pretty high speed connection, abort if
--                                                    it is less than 2000 bytes/sec during 20 seconds;
--              read_timeout                        - Time-out the read operation after this amount of seconds;
--              connect_timeout                     - Time-out connect operations after this amount of seconds, if connects are;
--                                                    OK within this time, then fine... This only aborts the connect phase;
--              dns_cache_timeout                   - DNS cache timeout;
--
--  Returns:
--              {code=NUMBER, body=STRING}
--
local function sync_request(self, method, url, body, opts)

    if not method or not url then
        error("sync_request expects (method, url, ...)")
    end

    opts = opts or {}

    local ctx = {done          = false,
                 http_code     = 0,
                 curl_code     = 0,
                 error_message = '',
                 fiber         = fiber.self(),
                 written       = '',
                 readen        = body or '', }

    local headers = opts.headers or {}

    -- I have to set CL since CURL-engine works async
    if body then
        headers['Content-Length'] = body:len()
    end

    local ok, emsg = self.curl:async_request(method, url,
                                  {ca_path            = opts.ca_path,
                                   ca_file            = opts.ca_file,
                                   headers            = headers,
                                   read               = read_cb,
                                   write              = write_cb,
                                   done               = done_cb,
                                   ctx                = ctx,
                                   max_conns          = opts.max_conns,
                                   keepalive_idle     = opts.keepalive_idle,
                                   keepalive_interval = opts.keepalive_interval,
                                   low_speed_time     = opts.low_speed_time,
                                   low_speed_limit    = opts.low_speed_limit,
                                   read_timeout       = opts.read_timeout,
                                   connect_timeout    = opts.connect_timeout,
                                   dns_cache_timeout  = opts.dns_cache_timeout,
                                   curl_verbose       = opts.curl_verbose, } )

    -- Curl can't add a new request
    if not ok then
        error("curl has an internal error, msg = " .. emsg)
    end

    if opts.read_timeout ~= nil then
        fiber.sleep(opts.read_timeout)
    else
        fiber.sleep(60)
    end

    -- Curl has an internal error
    if ctx.curl_code ~= 0 then
        error("curl has an internal error, msg = " .. ctx.error_message)
    end

    -- Curl did a request and he has a response
    return {code = ctx.http_code, body = ctx.written}
end

curl_mt = {
  __index = {
    --
    --  <request> This function does HTTP request
    --
    --  Parameters:
    --
    --    method  - HTTP method, like GET, POST, PUT and so on
    --    url     - HTTP url, like https://tarantool.org/doc
    --    options - this is a table of options <see async_request>.
    --
    --              curl:async_request(...)
    --
    --              write - a callback. if a server returns some data, then
    --                      this function was being called.
    --              Example:
    --                function(data, context)
    --                  context.in_buffer = context.in_buffer .. data
    --                  return data:len()
    --                end
    --
    --              read - a callback. if this client have to pass some
    --                     data to a server, then this function was beign called.
    --              Example:
    --                function(content_size, context)
    --                  local out_buffer = context.out_buffer
    --                  local to_server = out_buffer:sub(1, content_size)
    --                  context.out_buffer = out_buffer:sub(content_size + 1)
    --                  return to_server
    --                end
    --
    --              done - a callback. if request has completed, then this
    --                     function was being called.
    --
    --              ctx - this is a user defined context.
    --  Returns:
    --     0 or raise an error
    --
    request = function(self, method, url, options)
      local curl = self.curl
      if not method or not url or not options then
        error('request expects (method, url, options)')
      end
      return curl:async_request(method, url, options)
    end,

    --
    -- see <async_request>
    --
    get_request = function(self, url, options)
      return self.curl:async_request('GET', url, options)
    end,

    --
    -- See <async_request>
    --
    sync_request = sync_request,

    --
    -- See <async_request>
    --
    sync_post_request = function(self, url, body, options)
      return sync_request(self, 'POST', url, body, options)
    end,

    --
    -- See <async_request>
    --
    sync_put_request = function(self, url, body, options)
      return sync_request(self, 'PUT', url, body, options)
    end,

    --
    -- See <async_request>
    --
    sync_get_request = function(self, url, options)
      return sync_request(self, 'GET', url, '', options)
    end,

    --
    -- This function returns a table with many values of statistic.
    --
    stat = function(self)
      return self.curl:stat()
    end,

    --
    -- Should be called at the end of work.
    -- This function cleans all resources (i.e. destructor).
    --
    free = function(self)
      self.curl:free()
    end,
  },
}

--
-- Export
--
return {
  http = http,
}
