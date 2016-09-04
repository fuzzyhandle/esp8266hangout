import machine

def setntptime(maxretries=10):
  # ntptime is a helper module which gets packaged into the firmware
  # Check https://raw.githubusercontent.com/micropython/micropython/master/esp8266/scripts/ntptime.py
  import ntptime
  for i in range (maxretries):
    try:
      ntptime.settime()
      break
    except:
      if i+1 == maxretries:
        raise

def deepsleep(sleeptime=15*60*1000):
  # configure RTC.ALARM0 to be able to wake the device
  rtc = machine.RTC()
  rtc.irq(trigger=rtc.ALARM0, wake=machine.DEEPSLEEP)

  # set RTC.ALARM0 to fire after some time. Time is given in milliseconds here
  rtc.alarm(rtc.ALARM0, sleeptime)

  #Make sure you have GPIO16 connected RST to wake from deepSleep.
  # put the device to sleep
  print ("Going into Sleep now")
  machine.deepsleep()