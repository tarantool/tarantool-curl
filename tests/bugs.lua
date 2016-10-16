#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['curl.driver'] = '../curl/driver.so'
-- }}

function run(is_skipping, desc, func)
  print('Running', desc)
  if not is_skipping then
    if func() then
      print('OK')
    end
  else
    print('SKIP')
  end
end

run(false, 'Issus https://github.com/tarantool/curl/issues/3', function()
  local curl = require('curl')
  local http = curl.http()
  local data = '{"a": "b"}'
  local resp = http:sync_request('POST', 'https://z4rx9vsaih.execute-api.us-east-1.amazonaws.com/prod/example', data)
  if resp.code ~= 200 then
    error('Expects 200 and ', data)
  end
  return true
end)
