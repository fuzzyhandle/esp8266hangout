from machine import Pin
from ds18x20 import DS18X20
import onewire
import time
import machine
import ujson
import urequests

def posttocloud(temperature):
  keystext = open("sparkfun_keys.json").read()
  keys = ujson.loads(keystext)
  url = keys['inputUrl'] + "?private_key=" + keys['privateKey'] + "&temp=" + str(temperature)
  #data = {'temp':temperature}
  #data['private_key'] = keys['privateKey']
  #print (keys['inputUrl'])
  #print(keys['privateKey'])
  #datajson = ujson.dumps(data)
  #print (datajson)
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