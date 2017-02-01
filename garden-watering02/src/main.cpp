//#define BLYNK_DEBUG
#define BLYNK_PRINT Serial

#include <wifi_config.h>
#include <blynk_config.h>

#include <BlynkSimpleEsp8266.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SimpleTimer.h>

//*******************************
//Define Constants
//********************************

//Difference in seconds between Local time zone and UTC.
//Use positive for East of 0 deg longitude and Negative for West of 0 deg longitude
const int TIMEZONE_OFFSET = ((5*60) +30) * 60;
const uint PUMP_PIN = 4;

//*******************************
//Define variable for Virtual Pins
//********************************
long v0_masterswitch = 0;
long v1_sleepinterval = 30;
//Default start at 6 am
int v2_irrigation_min_starttime = 6 * 3600;
//Default stop at 7 pm
int v2_irrigation_max_endtime = 19 * 3600;

//V3 has no matching variable on purpose. Its a display text generated via value of V4

long v4_irrigation_last_dose =  0L;
int v5_irrigation_dosage_volume = 15;
int v6_irrigation_dosage_interval = 2 * 3600;
int v7_override =0;

//*******************************
//Define other variables
//********************************
bool pump_running = false;
WiFiUDP Udp;
NTPClient ntpclient(Udp,TIMEZONE_OFFSET);
bool isFirstConnect = true;
SimpleTimer timer_sleep_esp;
SimpleTimer timer_do_work;
SimpleTimer timer_stop_pump;


//Declare Function signature to avoid having the body before the call
void dowork();

void sleep_timerfunc_interval()
{

  BLYNK_LOG("Inside sleep_timerfunc_interval");

  //Don't deep sleep if pump is running.
  if (pump_running)
  {
    BLYNK_LOG("Pump is running");
    return;
  }

  BLYNK_LOG("Starting to Sleep for %d minutes", v1_sleepinterval);
  ESP.deepSleep(60 * v1_sleepinterval * 1000000,RF_DEFAULT);
  delay(1000);
}

void timerfunc_timeout_stop_pump()
{
  //ESP.deepSleep(v1_sleepinterval * 60* 1000000,RF_DEFAULT);
  BLYNK_LOG("Stopping the Pump");
  pump_running = false;
  digitalWrite(PUMP_PIN,LOW);
}


BLYNK_CONNECTED() {
  BLYNK_LOG("In BLYNK_CONNECTED");
  if (isFirstConnect){
    Blynk.syncAll();
    isFirstConnect = false;
    BLYNK_LOG("Synced via BLYNK_CONNECTED");
  }
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

  BLYNK_LOG ("Irrigation Start Time %d",v2_irrigation_min_starttime);
  BLYNK_LOG ("Irrigation Stop Time %d",v2_irrigation_max_endtime);
}

BLYNK_WRITE(V3) // There is a Widget that WRITEs data to V3
{
  BLYNK_LOG ("V3 is just a display text. It has no variable in h/w");
}

BLYNK_WRITE(V4) // There is a Widget that WRITEs data to V4
{
  v4_irrigation_last_dose = param.asLong();

  BLYNK_LOG ("Change Last watering time is %ld",v4_irrigation_last_dose);
}

BLYNK_WRITE(V5) // There is a Widget that WRITEs data to V4
{
  v5_irrigation_dosage_volume = param.asInt();
  BLYNK_LOG ("Change Time for watering is %d seconds",v5_irrigation_dosage_volume);
}

BLYNK_WRITE(V6) // There is a Widget that WRITEs data to V6
{
  v6_irrigation_dosage_interval = param.asInt();
  BLYNK_LOG ("Change Delta between consecutive watering cycles is %d seconds",v6_irrigation_dosage_interval);
}

BLYNK_WRITE(V7) // There is a Widget that WRITEs data to V4
{
  v7_override = param.asInt();
  BLYNK_LOG ("Change Override Switch %d",v7_override);
}


void setup(/* arguments */) {
  /* code */
  Serial.begin(115200);

  BLYNK_LOG("In setup");

  pinMode(PUMP_PIN,OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  Blynk.begin(BLYNK_AUTH,WIFI_SSID,WIFI_PASSWORD);

  BLYNK_LOG("Waiting for BLYNK Server");

  while (! Blynk.connected())
  {
    BLYNK_LOG(".");
  }

  ntpclient.begin();
  ntpclient.update();

  BLYNK_LOG("Starting timer");
  timer_sleep_esp.setInterval(30000L, sleep_timerfunc_interval);
  timer_do_work.setTimeout(10000L, dowork);
}

void loop(/* arguments */) {
  /* code */

  //TODO
  /*  Put code here to check for wifi and internet connectivity.
  Deep sleep for a long time if check fails to avoid battery wastage */
  timer_sleep_esp.run();
  Blynk.run();
  timer_do_work.run();

  if (pump_running)
  {
    timer_stop_pump.run();
  }
}

void dowork()
{
  BLYNK_LOG ("In dowork");

  //Check if we need to start the pump
  //get epoch
  long epoch =  ntpclient.getEpochTime();
  //get seconds from start of the day
  int seconds_since_start_of_day = epoch % 86400;

  BLYNK_LOG ("Epoch is %ld",epoch);
  BLYNK_LOG ("Last watering time is %ld",v4_irrigation_last_dose);
  BLYNK_LOG ("Time between consecutive cycles %d",v6_irrigation_dosage_interval);

  BLYNK_LOG ("Seconds since start of day %d",seconds_since_start_of_day);
  if (v0_masterswitch == HIGH)
  {
    BLYNK_LOG ("Override is %d", v7_override);
    if (  v7_override or (  epoch >= (v4_irrigation_last_dose + v6_irrigation_dosage_interval)))
    {

      BLYNK_LOG ("Watering needed based on last dose and interval");
      if (v7_override or ( ( seconds_since_start_of_day >= v2_irrigation_min_starttime ) and (seconds_since_start_of_day <= v2_irrigation_max_endtime)))
      {
          //BLYNK_LOG ("Current time is within min and max time window");
          BLYNK_LOG ("Watering conditions met. Waterning now.");
          //All Conditions met.
          pump_running = true;
          digitalWrite(PUMP_PIN, HIGH);

          //Set the last watering time to current time to avoid this getting triggered again till the next cycle
          v4_irrigation_last_dose = epoch;
          Blynk.virtualWrite(V4, v4_irrigation_last_dose);

          //Create a buffer to hold string HH:MM:SS
          //char v3_display_buffer[10];
          //sprintf(v3_display_buffer, "%02d:%02d:%02d",ntpclient.getHours(), ntpclient.getMinutes() ,ntpclient.getSeconds());
          String v3_display_buffer;
          v3_display_buffer = ntpclient.getFormattedTime();
          Blynk.virtualWrite(V3, v3_display_buffer);

          BLYNK_LOG ("Last watering time in text %s",v3_display_buffer.c_str());
          timer_stop_pump.setTimeout( v5_irrigation_dosage_volume * 1000, timerfunc_timeout_stop_pump);
      }
    }
  }

  //Reset Override
  if (v7_override)
  {
    Blynk.virtualWrite(V7, 0);
  }
}
