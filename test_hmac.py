import hmac, hashlib, base64

key = b'5a062f26-b572-4369-b9b7-5a53b5decc5f-3558e9ed-e16f-47c3-babc-aff39d2db945'
payload = b'{"action":"currentTemperature","cause":{"type":"PERIODIC_POLL"},"createdAt":0,"deviceId":"6a7186e509efd1746c350d10","replyToken":"d6d94995-b8a5-4963-9632-993de2e94d35","type":"event","value":{"humidity":22,"temperature":22}}'

sig = hmac.new(key, payload, hashlib.sha256).digest()
print("HMAC_B64:", base64.b64encode(sig).decode())
