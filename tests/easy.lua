#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = '../curl/driver.so'
-- }}

local curl = require('curl')

http = curl.http()

res = http:sync_request('GET', 'mail.ru')
print('GET', res.body)
res = http:sync_request('PUT', 'www.rbc.ru', '{data: 123}',
  {headers = {['Content-type'] = 'application/json'}})
print('PUT', res.body)
