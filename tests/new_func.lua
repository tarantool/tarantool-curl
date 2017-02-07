curl = require('curl')
json = require('json')
local pool_size = 10
local http = curl.http({pool_size=pool_size})

url = "https://httpbin.org/"

delete_data = http:delete(url .. "delete")
if delete_data.code ~= 200 then
    error("delete: wrong code")
end
option_data = http:http_options(url)
if option_data.code ~= 200 then
    error("option: wrong code")
end

head_data = http:head(url)
if head_data.code ~= 200 then
    error("head: wrong code")
end

url = "127.0.0.1:10000"
trace_data = http:trace(url)
if trace_data.code ~= 200 then
    error("trace: wrong code")
end


connect = http:http_connect(url)
if data.code ~= 200 then 
    error("connect: wrong code")
end

print("[+] Ok")
