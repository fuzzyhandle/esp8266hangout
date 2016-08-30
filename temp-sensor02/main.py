from machine import Pin
from ds18x20 import DS18X20
import onewire
import time
import ujson
import urequests

def posttocloud(temperature):
  
  keystext = open("sparkfun_keys.json").read()
  keys = ujson.loads(keystext)
  params = {}
  params['temp'] = temperature
  params['private_key'] = keys['privateKey']
  
  #data.sparkfun doesn't support putting data into the POST Body.
  #We had to add the data to the query string
  #Copied the Dirty hack from
  #https://github.com/matze/python-phant/blob/24edb12a449b87700a4f736e43a5415b1d021823/phant/__init__.py
  payload_str = "&".join("%s=%s" % (k, v) for k, v in params.items())
  url = keys['inputUrl'] + "?" + payload_str
  resp = urequests.request("POST", url)
  print (resp.text)

while True:
  p = Pin(2) # Data Line is on GPIO2 aka D4
  ow = onewire.OneWire(p)
  ds = DS18X20(ow)
  lstrom = ds.scan()
  #Assuming we have only 1 device connected
  rom = lstrom[0]

  ds.convert_temp()
  time.sleep_ms(750)
  temperature = round(float(ds.read_temp(rom)),1)
  #print("Temperature: {:02.1f}".format(temperature))
  posttocloud(temperature)
  time.sleep(10)