#!/usr/bin/env nodejs
/*
 * Copyright (C) 2016 - 2017 Tarantool AUTHORS: please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
http = require('http')
http.createServer(function (req, res) {
    setTimeout(function () {
        res.writeHead(200, {'Content-Type': 'text/plain'});
        res.end("Hello World");
    }, 1 )
    }).on('request', function(request, response){
        if (request.method == "TRACE"){
            response.writeHead(200, {'Content-Type': 'text/plain'});
            response.end("Hello World!!!Trace!!!");
        }
    }
    ).on('connection', function (socket) {
    socket.setTimeout(10000*2);
    }).on('connect', (req, cltSocket, head) => {
            url = require('url')
            net = require('net')
          //var srvUrl = url.parse(`http://${req.url}`);
          var srvSocket = net.connect(10000, "127.0.0.1", () => {
          cltSocket.write('HTTP/1.1 200 Connection Established\r\n' +
                                              'Proxy-agent: Node.js-Proxy\r\n' +
                                                                  '\r\n');
                      srvSocket.write(head);
                      srvSocket.end()
                      cltSocket.end()    
                      });
    }
    ).listen(10000);
