import mybuddy

import machine
import time
import ujson
import urequests


#Can be any value.
#Using GUID to rule out duplicates
MQTT_CLIENT_ID = "578ee6a0-d0ad-40be-8687-162a6d6136e4"

#Safetynet during development
#Give enough time to delete file before execution
for x in range (5):
  print (".",end='')
  time.sleep (1)
print ("")

def dowatering(pulseduration=10):
  print ("Watering started")
  #GPIO Pin 4 aka D2 (Node MCU) is connected to relay signal pin
  wateringpumprelayPin = machine.Pin(4, machine.Pin.OUT,value=0)
  try:
    wateringpumprelayPin.high()
    time.sleep(pulseduration)
  finally:
    wateringpumprelayPin.low()
  
  print ("Watering done")
  

def get_adafruit_config():
  keystext = open("adafruit.json").read()
  adafruitconfig = ujson.loads(keystext)
  return adafruitconfig
  
def get_adafriut_mqtt_client(connect=False):
  from umqtt.simple import MQTTClient
  adafruitconfig = get_adafruit_config()
  c = MQTTClient(MQTT_CLIENT_ID, adafruitconfig['servername'],adafruitconfig['port'],adafruitconfig['username'],adafruitconfig['aiokey'] )
  if connect:
    c.connect()
  return c
  
def read_from_cloud(feedname):
  adafruitconfig = get_adafruit_config()
  url = "http://" + adafruitconfig['servername'] + "/api/feeds/" + feedname
  #url = "http://io.adafruit.com/api/feeds/" + feedname
  #print (url)
  #print (adafruitconfig['aiokey'])
  headers = {'X-AIO-Key':adafruitconfig['aiokey']}
  resp = urequests.request("GET", url, headers=headers)
  #print (resp.content)
  respdata = resp.json()
  return  respdata['last_value'] 
  
def post_to_cloud(feedname,value):
  adafruitconfig = get_adafruit_config()
  client = get_adafriut_mqtt_client(connect=True)
  client.publish('{0}/feeds/{1}'.format(adafruitconfig['username'], feedname), str(value),qos=1)
  client.disconnect()

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
    machine.idle()
    time.sleep(1)

  if wlan.isconnected():
    print(wlan.ifconfig())
  return wlan.isconnected()
   
def wifioffdeepsleep(sleepinterval_seconds):
  setwifimode (False, False)
  mybuddy.deepsleep(sleepinterval_seconds *1000)

  
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
  if not (connecttonetwork() and mybuddy.have_internet()):
    #No internet. Sleep and retry later
    wifioffdeepsleep(1*60)
  else:
    #Flow comes here only when we have wifi
    try:
      mybuddy.setntptime(10)
    except:
      print ("Error setting NTP Time. Going to sleep")
      wifioffdeepsleep(2*60)
    
  rtcdata['ntptime'] = time.time()
  post_to_cloud('i-am-alive', rtcdata['ntptime'])
  
  #Micropython has no timezone support. Timezone is always UTC.
  #Bad Hack. 
  #Add the Delta for India Time
  localtime = time.localtime(rtcdata['ntptime']+ 19800) 
  
  #Read data from Cloud
  lastwateringtime = int(read_from_cloud ('last-watering-time'))
  if lastwateringtime is not None:
    rtcdata['lastwateringtime'] = lastwateringtime
  
  wateringdosageduration = int(read_from_cloud ('water-dosage-length'))
  wateringdosageinterval = 60* 60* int(read_from_cloud ('water-dosage-frequency'))
  wateringsystemstatus = (read_from_cloud ('watering-system-automation') == 'ON')
  wateringearliestpossiblehour = int(read_from_cloud ('earliest-morning-watering-time'))
  wateringlatestpossiblehour = int(read_from_cloud ('latest-afternoon-watering-time'))
  wateringupdateinterval = int(read_from_cloud ('watering-system-update-interval'))
  
  validhour = True
  print ("Hour of the day is %d"%(localtime[3]))
  if ((localtime[3] < wateringearliestpossiblehour) or (localtime[3] >= wateringlatestpossiblehour)):
    validhour = False

  print ("Watering system is Enabled" if wateringsystemstatus  else "Watering system is Disabled")  
  print ("Current Time is within the watering hours window" if validhour  else "Current Time is outside the watering hours window")  
  
  wateritnow = False

  if wateringsystemstatus and validhour:
    #No data of last watering
    #Water immediately
    if rtcdata.get('lastwateringtime',0) == 0:
      wateritnow = True
    else:
      #Check if delta has expired
      if rtcdata['lastwateringtime'] + wateringdosageinterval <= rtcdata['ntptime']:
        wateritnow = True

  if wateritnow:
    dowatering(wateringdosageduration)
    rtcdata['lastwateringtime'] = rtcdata['ntptime']
    post_to_cloud('last-watering-time',rtcdata['lastwateringtime'])  

  rtcdata['wateringdosageduration'] = wateringdosageduration
  rtcdata['wateringdosageinterval'] = wateringdosageinterval
  rtcdata['i-am-alive'] = time.time()

  if wateringsystemstatus:
    post_to_cloud('next-watering-in', rtcdata['lastwateringtime'] + rtcdata['wateringdosageinterval'] - rtcdata['ntptime'])
  
  if True:
    import json
    _rtc = machine.RTC()
    datastring = json.dumps(rtcdata)
    print("Saving Data in RTC %s"%(datastring))
    _rtc.memory(datastring)
    time.sleep (2)
  
  post_to_cloud('i-am-alive', rtcdata['i-am-alive'])
  
  sleepinterval_seconds = wateringupdateinterval * 60
  
  
  if wateritnow:
    sleepinterval_seconds -= (time.time() - rtcdata['ntptime'])

  wifioffdeepsleep(sleepinterval_seconds)