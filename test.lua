package.preload['curl.driver'] = '../curl/driver.so'

local curl = require('curl')
local fiber = require('fiber')

http = curl.http()

local function write(data, ctx)
--  print(data)
  return data:len()
end

local function done(result, ctx)
  fiber.wakeup(ctx.fiber)
end

local function read(cnt, ctx)
  local read = ctx.read
  local res = read.data:sub(read.pos, read.pos + cnt - 1)
  read.pos = read.pos + res:len()
  return res
end


--http:request('GET', 'www.rbc.ru', {
--  write = write,
--  done = done,
--  ctx = {fiber = fiber.self()},
--  headers = {cnt = '12345'}})

--fiber.sleep(60)

--http:request('PUT', 'www.rbc.ru', {
--  read = read,
--  write = write,
--  done = done,
--  ctx = {fiber = fiber.self(),
--    read = {data = '{test: 123}', pos = 1}},
--  headers = {['Content-type'] = 'application/json'}})

--fiber.sleep(60)
--

print(http:sync_request('GET', 'mail.ru'))
print(http:sync_request('PUT', 'www.rbc.ru', '{data: 123}',
  {headers = {['Content-type'] = 'application/json'}}))

os.exit()
