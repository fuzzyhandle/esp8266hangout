#define BLYNK_DEBUG
#define BLYNK_PRINT Serial

#include <wifi_config.h>
#include <blynk_config.h>

#include <BlynkSimpleEsp8266.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SimpleTimer.h>

long v0_masterswitch = 0;
long v1_sleepinterval = 30;

//Default start at 6 am
int v2_irrigation_min_starttime = 6 * 3600;
//Default stop at 7 pm
int v2_irrigation_max_endtime = 19 * 3600;

WiFiUDP Udp;
NTPClient ntpclient(Udp);

SimpleTimer timer;

bool isFirstConnect = true;

void timerfunc()
{
  //ESP.deepSleep(v1_sleepinterval * 60* 1000000,RF_DEFAULT);
  BLYNK_LOG("Starting to Sleep for %d minutes", v1_sleepinterval);
  ESP.deepSleep(60 * v1_sleepinterval * 1000000,RF_DEFAULT);
  delay(1000);
}

BLYNK_CONNECTED() {
  //Blynk.syncVirtual(V0);
  //Blynk.syncVirtual(V1);
  if (isFirstConnect){
    Blynk.syncAll();
    isFirstConnect = false;
    BLYNK_LOG("Synced");
  }


  //Blynk.syncVirtual(V0, V1, V2);
}


BLYNK_WRITE(V0) // There is a Widget that WRITEs data to V0
{
   v0_masterswitch = param.asInt();
   BLYNK_LOG("Change Master Switch is %d" , param.asInt());
}

BLYNK_WRITE(V1) // There is a Widget that WRITEs data to V1
{
   v1_sleepinterval = param.asInt();
   BLYNK_LOG("Change Sleep Interval is %d" , param.asInt());
}

BLYNK_WRITE(V2) // There is a Widget that WRITEs data to V2
{

  TimeInputParam t(param);
  v2_irrigation_min_starttime = param[0].asInt();
  v2_irrigation_max_endtime = param[1].asInt();

/*
  if (t.hasStartTime()){
    BLYNK_LOG1("Start Time Specified");
    v2_irrigation_min_starttime = (t.getStartHour() * 3600);
  }

 if (t.hasStopTime())
 {
    BLYNK_LOG1("Start Time Specified");
    v2_irrigation_max_endtime = (t.getStopHour() * 3600);
  }
*/
  BLYNK_LOG ("Irrigation Start Time %d",v2_irrigation_min_starttime);
  BLYNK_LOG ("Irrigation Stop Time %d",v2_irrigation_max_endtime);
}

void sendheartbeat()
{
  //String stringHeartbeat = String("") + ntpclient.getHours() + ":" + ntpclient.getMinutes();
  String stringHeartbeat =  ntpclient.getFormattedTime ();
  BLYNK_LOG("Heart Beat is %s", stringHeartbeat.c_str());
  Blynk.virtualWrite(V3, stringHeartbeat.c_str());
}

void setup(/* arguments */) {
  /* code */
  Serial.begin(115200);
  BLYNK_LOG("In setup");
  //Serial.println("Hello from setup");
  //Serial.printf("%s %s %s\n", BLYNK_AUTH,WIFI_SSID,WIFI_PASSWORD);
  Blynk.begin(BLYNK_AUTH,WIFI_SSID,WIFI_PASSWORD);

  BLYNK_LOG("Waiting for BLYNK Server");
  while (! Blynk.connected())
  {
    BLYNK_DEBUG(".");
    delay(1000);
  }

  ntpclient.setTimeOffset(19800);
  ntpclient.begin();
  ntpclient.update();

  timer.setInterval(30000, timerfunc);
}

void loop(/* arguments */) {
  /* code */

  //TODO
  /*  Put code here to check for wifi and internet connectivity.
  Deep sleep for a long time if check fails to avoid battery wastage */

  Blynk.run();

  //Serial.printf("%s\n", "In loop");
  //Serial.printf("%s %d\n", "Current Time is", NTPClient.getEpochTime());

  // BLYNK_LOG ("Sleep Interval is %d",v1_sleepinterval);
  // BLYNK_LOG ("Master Switch is %d",v0_masterswitch);
  //
  // BLYNK_LOG ("Irrigation Start Time %d",v2_irrigation_min_starttime);
  // BLYNK_LOG ("Irrigation Stop Time %d",v2_irrigation_max_endtime);
  //

  sendheartbeat();
  timer.run();

  //delay(30000);
  //Blynk.syncAll();
  //BLYNK_LOG("Ending to Sleep");
}

/*
const int statusLED = 2;
#include <ESP8266WiFi.h>
void setup() {
  pinMode(statusLED, OUTPUT);
  digitalWrite(statusLED, HIGH);

  Serial.begin(115200);
  Serial.println("The ESP-12e has started.");
  delay(500);
}

void loop() {
  Serial.println("Loop Start.");
  delay(1000);
  digitalWrite(statusLED, 0);
  delay(1000);
  digitalWrite(statusLED, 1);
  Serial.println("Loop End");
}
*/
