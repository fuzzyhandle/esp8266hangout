import machine
from ds18x20 import DS18X20
import onewire
import time
import ujson
import urequests
import mybuddy

#Safetynet during development
#Give enough time to delete file before execution
for x in range (5):
  print (".",end='')
  time.sleep (1)
print ("")

def get_wifi_config():
  keystext = open("wifi.json").read()
  wificonfig = ujson.loads(keystext)
  return wificonfig
  
def setwifimode(stationmode=False,apmode=False):
  import network
  sta = network.WLAN(network.STA_IF) # create station interface
  ap = network.WLAN(network.AP_IF)
  sta.active(stationmode)
  ap.active(apmode)  
  return [sta,ap]
  
def connecttonetwork(retry = 5):
  import network
  wlan =  network.WLAN(network.STA_IF)
  wlan.active(True)
  wificonfig = get_wifi_config()
  wlan.connect(wificonfig['ssid'], wificonfig['password'])
  
  
  for x in range (retry):
    if wlan.isconnected():
      break
    time.sleep(5)

  if wlan.isconnected():
    print(wlan.ifconfig())
  return wlan.isconnected()
   
def wifioffdeepsleep(sleepinterval_seconds):
  setwifimode (False, False)
  mybuddy.deepsleep(sleepinterval_seconds *1000)

def posttocloud(temperature):
  
  keystext = open("sparkfun_keys.json").read()
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

  #Try to read save data from memory
  _rtc = machine.RTC()
  memorystring = _rtc.memory()
  if len(memorystring) == 0:
    print("No Data in RTC")
  else:
    try:
      import json
      print ("Memory Data string %s"%(memorystring))
      rtcdata = json.loads(memorystring)
    except ValueError:
      print ("Error parsing RTC data")
      rtcdata = None

  if rtcdata is None:
    rtcdata = {}
  
  
  #Connect to Network
  if not connecttonetwork():
    print ("No connection to wifi")
    wifioffdeepsleep(15*60)
  else:
    if not mybuddy.have_internet():
      #No internet. Sleep and retry later
      print ("No connection to internet")
      wifioffdeepsleep(5*60)  
    
    #Flow comes here only when we have wifi
    try:
      mybuddy.setntptime(10)
    except:
      print ("Error setting NTP Time. Going to sleep")
      wifioffdeepsleep(2*60)
    
  rtcdata['ntptime'] = time.time()
  
  #Micropython has no timezone support. Timezone is always UTC.
  #Bad Hack. 
  #Add the Delta for India Time
  localtime = time.localtime(rtcdata['ntptime']+ 19800) 
  
  p = machine.Pin(2) # Data Line is on GPIO2 aka D4
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

  if True:  
    import json
    _rtc = machine.RTC()
    datastring = json.dumps(rtcdata)
    print("Saving Data in RTC %s"%(datastring))
    _rtc.memory(datastring)
    time.sleep (2)
  
  
  sleepinterval_seconds = 5 * 60
  nextcheck = rtcdata['ntptime'] + (sleepinterval_seconds - rtcdata['ntptime'] % sleepinterval_seconds)
  sleepinterval_seconds = nextcheck - rtcdata['ntptime']
  
  wifioffdeepsleep(sleepinterval_seconds)
  