#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
ADC_MODE(ADC_VCC);
//#include <ESP8266WiFiMulti.h>

//AWS
#include <sha256.h>
#include <Utils.h>
#include <AWSClient2.h>

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <MQTTClient.h>
#include <Countdown.h>



//AWS MQTT Websocket
#include <Client.h>
#include <AWSWebSocketClient.h>
#include <CircularByteBuffer.h>

#include "wifi_config.h"
#include "aws-iot-config.h"


//AWS IOT config, change these:
char wifi_ssid[]       = WIFI_SSID;
char wifi_password[]   = WIFI_PASSWORD;



//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;
const uint deepSleepInterval = 60 * 60* 1000000U;
const uint deepSleepIntervalOnError = 5 * 60* 1000000U;

//ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;


//# of connections
long connection = 0;
float h = 0.0f;
float t = 0.0f;

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;

//callback to handle mqtt messages
//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;

  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect () {

  if (client == NULL) {
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  } else {

    if (client->isConnected ()) {
      client->disconnect ();
    }
    delete client;
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  }


  //delay is not necessary... it just help us to get a "trustful" heap space value
  delay (1000);
  Serial.print (millis ());
  Serial.print (" - conn: ");
  Serial.print (++connection);
  Serial.print (" - (");
  Serial.print (ESP.getFreeHeap ());
  Serial.println (")");




  int rc = ipstack.connect(aws_endpoint, port);
  if (rc != 1)
  {
    Serial.println("error connection to the websocket server");
    return false;
  } else {
    Serial.println("websocket layer connected");
  }


  Serial.println("MQTT connecting");
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  char* clientID = generateClientID ();
  data.clientID.cstring = clientID;
  rc = client->connect(data);
  delete[] clientID;
  if (rc != 0)
  {
    Serial.print("error connection to MQTT server");
    Serial.println(rc);
    return false;
  }
  Serial.println("MQTT connected");
  return true;
}

//subscribe to a mqtt topic
void subscribe_get_shadow () {
  Serial.println("Subscribing to get shadow");
  //subscript to a topic
  int rc = client->subscribe(aws_topic_get_accepted, MQTT::QOS0, messageArrived);
  if (rc != 0) {
    Serial.print("rc from MQTT subscribe is ");
    Serial.println(rc);
    return;
  }
  Serial.println("MQTT subscribed");
}

void sendmessage1 () {
  //send a message
  MQTT::Message message;
  char buf[100];

  double voltage = ESP.getVcc()/1000.0;

  String messagebody = String( "{\"state\":{") + "\"reported\":{" + "\"voltage\":\"" + voltage + "\"" "}}}";

  Serial.println(aws_topic);
  Serial.println(messagebody);

  strcpy(buf, messagebody.c_str());
  message.qos = MQTT::QOS1;
  message.retained = true;
  message.dup = false;
  message.payload = (void*)buf;
  message.payloadlen = strlen(buf)+1;
  int rc = client->publish(aws_topic, message);
}

void sendmessage_to_read_shadow () {
  Serial.println("Publishing to get shadow");

  //send a message
  MQTT::Message message;
  char buf[100];

  String messagebody = "{}";

  Serial.println(aws_topic_get);
  Serial.println(messagebody);

  strcpy(buf, messagebody.c_str());
  //strcpy(buf, "{\"state\":{\"reported\":{\"temperature\": false}, \"desired\":{\"on\": false}}}");
  message.qos = MQTT::QOS1;
  message.retained = true;
  message.dup = false;
  message.payload = (void*)buf;
  message.payloadlen = strlen(buf)+1;
  int rc = client->publish(aws_topic_get, message);
}

void setup() {
  Serial.begin (115200);
  WiFi.mode(WIFI_OFF);
  //WiFi.disconnect(true);

  //Warm up the DHT and sleep for it to be fully operational.
  //Needs about 2 seconds as per AM2302 datasheet
  delay (3000);

  Serial.setDebugOutput(1);

  //read the data from sensor

  //Setup Wifi
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.printDiag(Serial);

  Serial.println ("connecting to wifi");
  int starttime = millis();
  while(WiFi.status() != WL_CONNECTED)
  {
    if (starttime + 15000 < millis() )
    {
      //Goto sleep
      WiFi.disconnect(true);
      ESP.deepSleep(deepSleepIntervalOnError,WAKE_RF_DEFAULT);
      delay(1000);
    }
    delay(500);
    Serial.print (".");
  }
  Serial.println ("\nconnected");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  //fill AWS parameters
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_key);
  awsWSclient.setAWSSecretKey(aws_secret);
  awsWSclient.setUseSSL(true);

  if (connect ())
  {
      subscribe_get_shadow();
      sendmessage_to_read_shadow();
  }
}


void loop() {
  //Check if connected
  if (awsWSclient.connected ())
  {
     client->yield();
  }
  else
  {
    Serial.println ("Reconnecting");
    if (connect ())
    {
      subscribe_get_shadow();
      sendmessage_to_read_shadow();
    }
  }
}
