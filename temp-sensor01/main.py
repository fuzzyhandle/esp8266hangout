from machine import Pin
from ds18x20 import DS18X20
import onewire
import time

p = Pin(2) # Data Line is on GPIO2 aka D4
ow = onewire.OneWire(p)
ds = DS18X20(ow)
lstrom = ds.scan()
#Assuming we have only 1 device connected
rom = lstrom[0]

while True:
  ds.convert_temp()
  time.sleep_ms(750)
  temperature = round(float(ds.read_temp(rom)),1)
  print("Temperature: {:02.1f}".format(temperature))
  time.sleep(10)
