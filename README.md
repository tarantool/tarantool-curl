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
tools including `cmake`, a C compiler, `git` and Lua.

You will need the `curl` developer package. To download and install it, say
(example for Ubuntu):
```
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
response = http:sync_request('GET', 'http://tarantool.org', '')
response.code
```

If all goes well, you should see:
```
...
tarantool> response = http:sync_request('GET', 'http://tarantool.org', '')
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

The `curl` package contains one component, `http()`, which contains following 
functions:

* `VERSION` -- a version as string

* `request(method, url [, options])` -- Asynchronous data transfer 
  request. See details below.

* `get_request(url [, options])` -- This is the same as `request('GET', 
  url [, options])`.

* `sync_request(method, url [, body [, options]])` -- This is similar to 
  `request(method, url [, options])` but it is synchronous.

* `sync_post_request(url, body [, options])` -- post request

* `sync_put_request(url, body [, options])` -- put request

* `sync_get_request(url [, body [, options]])` -- This is the same as 
  `sync_request('GET', url [, body [, options]])`.

* `stat()` -- This function returns a table with many values of statistic.

* `free` -- Should be called at the end of work.This function cleans all resources (i.e. destructor).

The `request` and `get_request` functions return either (boolean) true, 
or an error.

The `sync_request` and `sync_get_request` functions return a structured 
object containing (numeric) code and (string) body, or an error.

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

    * `curl:async_request(...,)` - a further call;

    * `ctx` - user-defined context, with a format that openSSL
      recognizes, which is passed to callback functions;

    * `done` - name of a callback function which is invoked when a request
      was completed;

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

## Example function

In this example, we define a function named `d()` and make three GET requests:
* asynchronous with `d()` as a callback,
* synchronous with `d()` as a callback, and
* synchronous without callbacks.

```lua
http = require('curl').http()
function d() print('done') end
result=http:request('GET','mail.ru',{done=d()})
result=http:sync_request('GET','mail.ru','',{read_timeout=1,done=d())})
result=http:sync_get_request('http://mail.ru/page1')
```

## Example program

In this example, we make two requests:
* GET request to `mail.ru` (and print the resulting HTTP code), and
* PUT request to `rbc.ru` (we send JSON data and then print the request body).

```lua
#!/usr/bin/env tarantool
local curl = require('curl')
http = curl.http()
res = http:sync_request('GET', 'mail.ru')
print('GET', res.body)
res = http:sync_request('PUT', 'www.rbc.ru', '{data: 123}',
   {headers = {['Content-type'] = 'application/json'}})
print('PUT', res.body)
```

