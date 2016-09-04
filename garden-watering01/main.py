import machine
from ds18x20 import DS18X20
import onewire
import time
import ujson
import urequests
# ntptime is a helper module which gets packaged into the firmware
# Check https://raw.githubusercontent.com/micropython/micropython/master/esp8266/scripts/ntptime.py
import ntptime
import pickle

#Safetynet during development
#Give enough time to delete file before execution
for x in range (3):
  print (".",end='')
  time.sleep (1)
print ("")


# class RTCUserData:
  
  # def __init__(self):
    # self._ntptime = 0
    # self._lastwateringtime = 0
    
  # @property
  # def ntptime(self):
      # print("Getting value")
      # return self._ntptime

  # @ntptime.setter
  # def ntptime(self, value):
    # print("Setting value")
    # self._ntptime = value

  # @property
  # def lastwateringtime(self):
      # print("Getting value")
      # return self._lastwateringtime

  # @lastwateringtime.setter
  # def lastwateringtime(self, value):
    # print("Setting value")
    # self._lastwateringtime = value

def waternow(pulseduration=10):
  print ("Watering started")
  #GPIO Pin 5 aka D1 is connected to relay signal pin
  wateringpumprelayPin = machine.Pin(5, machine.Pin.OUT,value=0)
  try:
    wateringpumprelayPin.high()
    time.sleep(pulseduration)
  finally:
    wateringpumprelayPin.low()
  
  print ("Watering done")
  
def deepsleep():

  # configure RTC.ALARM0 to be able to wake the device
  rtc = machine.RTC()
  rtc.irq(trigger=rtc.ALARM0, wake=machine.DEEPSLEEP)

  # set RTC.ALARM0 to fire after some time. Time is given in milliseconds here
  rtc.alarm(rtc.ALARM0, 30*1000)

  #Make sure you have GPIO16 connected RST to wake from deepSleep.
  # put the device to sleep
  print ("Going into Sleep now")
  machine.deepsleep()

def get_temperature():
  p = machine.Pin(2) # Data Line is on GPIO2 aka D4
  ow = onewire.OneWire(p)
  ds = DS18X20(ow)
  lstrom = ds.scan()
  #Assuming we have only 1 device connected
  rom = lstrom[0]

  ds.convert_temp()
  time.sleep_ms(750)
  temperature = round(float(ds.read_temp(rom)),1)
  print("Temperature: {:02.1f}".format(temperature))  
  return temperature
  
def post_temperature_tocloud(temperature):
  
  keystext = open("sparkfun_keys_temperature.json").read()
  keys = ujson.loads(keystext)
  params = {}
  params['temp'] = "{:02.1f}".format(round(temperature,1))
  params['private_key'] = keys['privateKey']
  
  #data.sparkfun doesn't support putting data into the POST Body.
  #We had to add the data to the query string
  #Copied the Dirty hack from
  #https://github.com/matze/python-phant/blob/24edb12a449b87700a4f736e43a5415b1d021823/phant/__init__.py
  payload_str = "&".join("%s=%s" % (k, v) for k, v in params.items())
  url = keys['inputUrl'] + "?" + payload_str
  resp = urequests.request("POST", url)
  print (resp.text)
  
if __name__ == "__main__":
  #Put things here which can be done before needing wifi
  
  #Get the last run time from RTC memory
  resetcause = machine.reset_cause()
  rtcdata = None

  #if resetcause == machine.DEEPSLEEP_RESET:
    #Woken up from RTC time out.
    #Try to read save data from memory
  _rtc = machine.RTC()
  memorystring = _rtc.memory()
  if len(memorystring) == 0:
    print("No Data in RTC")
  else:
    import json
    rtcdata = json.loads(memorystring)
    
  if rtcdata is None:
    rtcdata = {}
  
  import network
  wlan = network.WLAN(network.STA_IF)
  
  while not wlan.isconnected() :
    time.sleep(1)
  
  #Flow comes here only when we have wifi
  ntptime.settime()
  rtcdata['ntptime'] = ntptime.time()

  #No data of last watering
  #Water immediately
  if rtcdata.get('lastwateringtime',0) == 0:
    waternow()
    rtcdata['lastwateringtime'] = time.time()
  else:
    #Check if delta has expired
    if rtcdata['lastwateringtime'] + 120 <= time.time():
      waternow()
      rtcdata['lastwateringtime'] = time.time()
  
  # try:
    # temperature = get_temperature()
    # post_temperature_tocloud(temperature)
  # except:
    # print "Error Posting Temperature"
  
  if True:
    import json
    _rtc = machine.RTC()
    #datastring = json.dumps(rtcdata)
    datastring = json.dumps(rtcdata)
    print("Saving Data in RTC %s"%(datastring))
    #print (len(datastring))
    _rtc.memory(datastring)
    
    time.sleep (2)
    
  
  deepsleep()
  