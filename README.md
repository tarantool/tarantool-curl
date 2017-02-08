<a href="http://tarantool.org">
    <img src="https://avatars2.githubusercontent.com/u/2344919?v=2&s=250" align="right">
</a>
<!--a href="https://travis-ci.org/tarantool/curl">
    <img src="https://travis-ci.org/tarantool/curl.png?branch=master" align="right">
</a-->

# libcurl bindings for Tarantool
    
The `tarantool/curl` package exposes some functionality of
[libcurl](http://https://curl.haxx.se/libcurl),
a library for data transfer via URLs.
With this package, Tarantool Lua applications can
send and receive data over the Internet using a variety of protocols.

The advantage of integrating `curl` with Tarantool, which is an application
server plus a DBMS, is that anyone can handle all of the tasks associated with
webs (control, manipulation, storage, access) with the same high-level language
and with minimal delay.

## Table of contents

* [How to install](#how-to-install)
* [Getting started](#getting-started)
* [API reference](#api-reference)
* [Example function](#example-function)
* [Example program](#example-program)

## How to install

We assume that you have Tarantool 1.7 and an operating system with developer
tools including `cmake`, a C suppors gnu99 compiler, `git` and Lua.

You will need the `curl` and `libev` developer packages. To download and install it, say
(example for Ubuntu):
```
sudo apt-get install libev libev-dev
sudo apt-get install curl
sudo apt-get install libcurl4-openssl-dev
```

The `curl` package itself is on
[github.com/tarantool/curl](github.com/tarantool/curl).
To download and install it, say:
```
cd ~
git clone https://github.com/tarantool/curl.git ~/tarantool-curl
cd tarantool-curl
cmake .
make
```

This should produce a library named `./curl/driver.so`.
Make sure this file, or a copy, is on the path that the Lua "require" statement
uses (`package.path`). The easiest way to ensure this is to add:
```
sudo make install
```

## Getting started

Start Tarantool in the interactive mode. Execute these requests:
```lua
curl = require('curl')
http = curl.http()
response = http:request('GET', 'http://tarantool.org', '')
response.code
```

If all goes well, you should see:
```
...
tarantool> response = http:request('GET', 'http://tarantool.org', '')
---
...

tarantool> response.code
---
- 200
...
```

This means that you have successfully installed `tarantool/curl` and
successfully executed an instruction that brought data from the Internet.

## API reference

The `curl` package contains one component, `http()`, which has three options: 

    * `max_conn` - number (default 10000) 
    * `pipeline` - 0 or 1 (default 0)
    * `pool_size`- number (default 5)
    
This component contains following functions:

* `VERSION` -- a version as string

* `request(method, url [, options])` -- Synchronous data transfer 
  request. See details below.

* `get(url [, options])` -- This is the same as `request('GET', 
  url [, options])`.

* `post(url, body [, options])` -- Post request, this is the same as 
  `request('POST', url [, options]).`

* `put(url, body [, options])` -- Put request, this is the same as 
  `request('PUT', url [, options]).`

* `async_request(self, method, url[, options])` -- This function does HTTP 
  request. See details below.

* `async_get(self, url, options)` -- This is the same as `async_request('GET', 
  url [, options])`.

* `async_post(self, url, options)` -- This is the same as `async_request('POST', 
  url [, options])`.

* `async_put(self, url, options)` -- This is the same as `async_request('PUT', 
  url [, options])`.

* `stat()` -- This function returns a table with many values of statistic. See 
  details below.
```lua
  local r = http:stat()
  r = {
    active_requests -- this is number of currently executing requests

    sockets_added -- this is a total number of added sockets into libev loop

    sockets_deleted -- this is a total number of deleted sockets from libev loop

    loop_calls -- this is a total number of iterations over libev loop

    total_requests -- this is a total number of requests

    http_200_responses -- this is a total number of requests which have returned a code HTTP 200

    http_other_responses -- this is a total number of requests which have requests not a HTTP 200

    failed_requests -- this is a total number of requests which have
                    -- failed (included system erros, curl errors, 
                    -- but not HTTP errors)
  }
```

* `free()` -- Should be called at the end of work. This function cleans all 
  resources (i.e. destructor).

The `request`, `get`, `post`, `put` functions return a table {code, body} or an error.

The `async_request`, `async_get`, `async_post`, `async_put` functions return true either error.

The parameters that can go with the operations are:

* `method` -- type = string; value = any HTTP method, for example 'GET',
  'POST', 'PUT'.

* `body` -- type = string; value = anything that should be passed to the
  server, for example '{data: 123}'.

* `url` -- type = string; value = any universal resource locator, for
  example 'http://mail.ru'.

* `options` -- type = table; value = one or more of the following:

    * `ca_path` - a path to an SSL certificate directory;

    * `ca_file` - a path to an SSL certificate file;

    * `headers` - a table of HTTP headers, for example:  
      `{headers = {['Content-type'] = 'application/json'}}`  
      Note: If you pass a value for the body parameter, you must set Content-Length header.

    * `keepalive_idle` & `keepalive_interval` - non-universal keepalive
      knobs (Linux, AIX, HP-UX, more);

    * `low_speed_time` & `low_speed_limit` - If the download receives
      less than "low speed limit" bytes/second during "low speed time" seconds,
      the operations is aborted. You could i.e if you have a pretty high speed
      connection, abort if it is less than 2000 bytes/sec during 20 seconds;

    * `read_timeout` - Time-out the read operation after this amount of seconds;

    * `connect_timeout` - Time-out connect operations after this amount of
      seconds, if connects are; OK within this time,
      then fine... This only aborts the connect phase;

    * `dns_cache_timeout` - DNS cache timeout;

    * `curl:async_*(...,)` - a further call;

    * `ctx` - user-defined context;

    * `done` - name of a callback function which is invoked when a request
      was completed;
      ```lua
      function done(curl_code, http_code, curl_error_message, my_ctx)
        my_ctx.done          = true
        my_ctx.http_code     = http_code
        my_ctx.curl_code     = curl_code
        my_ctx.error_message = curl_error_message
        fiber.wakeup(my_ctx.callee_fiber_id)
      end
      ```
    * `write` - name of a callback function which is invoked if the
      server returns data to the client;
      For example, the `write` function might look like this:
      ```lua
      function(data, context)
        context.in_buffer = context.in_buffer .. data
        return data:len()
      end
      ```

    * `read` - name of a callback function which is invoked if the
      client passes data to the server.
      For example, the `read` function might look like this:
      ```lua
      function(content_size, context)
        local out_buffer = context.out_buffer
        local to_server = out_buffer:sub(1, content_size)
        context.out_buffer = out_buffer:sub(content_size + 1)
        return to_server`
      end
      ```

## Example function

In this example, we define a function named `d()` and make three GET requests:
* asynchronous with `d()` as a callback,
* synchronous with `d()` as a callback, and
* synchronous without callbacks.

```lua
http = require('curl').http()
function d() print('done') end
result=http:async_request('GET','mail.ru',{done=d()})
result=http:request('GET','mail.ru','',{read_timeout=1,done=d())})
result=http:get('http://mail.ru/page1')
```

## Example program

In this example, we make two requests:
* GET request to `mail.ru` (and print the resulting HTTP code), and
* PUT request to `rbc.ru` (we send JSON data and then print the request body).

```lua
#!/usr/bin/env tarantool
local curl = require('curl')
http = curl.http()
res = http:request('GET', 'mail.ru')
print('GET', res.body)
res = http:request('PUT', 'www.rbc.ru', '{data: 123}',
   {headers = {['Content-type'] = 'application/json'}})
print('PUT', res.body)
```

More examples could be found into a directory tests/*.lua
