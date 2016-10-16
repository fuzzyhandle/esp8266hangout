class mywifi:
  @classmethod
  def connect(cls):
    import network
    import ujson
    import time
    
    keystext = open("wifi.json").read()
    wificonfig = ujson.loads(keystext)    
    
    wlan =  network.WLAN(network.STA_IF)
    wlan.active(True)
    
    wlan.connect(wificonfig['ssid'], wificonfig['password'])
  
    for x in range (2):
      if wlan.isconnected():
        break
      time.sleep(10)

    if wlan.isconnected():
      print(wlan.ifconfig())
    return wlan.isconnected()
