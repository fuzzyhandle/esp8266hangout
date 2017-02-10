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

#include <DHT.h>
#include <DHT_U.h>
#define AM2302_WIRE_BUS 4
#define DHTTYPE DHT22   // DHT 22  (AM2302)



#include "wifi_config.h"
#include "aws-iot-config.h"


//AWS IOT config, change these:
char wifi_ssid[]       = WIFI_SSID;
char wifi_password[]   = WIFI_PASSWORD;



//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

//ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

DHT dht(AM2302_WIRE_BUS, DHTTYPE);

//# of connections
long connection = 0;

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
void subscribe () {
   //subscript to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

//send a message to a mqtt topic
// void sendmessage () {
//     //send a message
//     MQTT::Message message;
//     char buf[100];
//     strcpy(buf, "{\"state\":{\"reported\":{\"on\": false}, \"desired\":{\"on\": false}}}");
//     message.qos = MQTT::QOS0;
//     message.retained = false;
//     message.dup = false;
//     message.payload = (void*)buf;
//     message.payloadlen = strlen(buf)+1;
//     int rc = client->publish(aws_topic, message);
// }

void sendmessage () {
    //send a message
    MQTT::Message message;
    char buf[100];


    float h = dht.readHumidity();

    // Read temperature as Celsius
    float t = dht.readTemperature();

    char strTemperature[8];
    char strRH[8];

    dtostrf(t, 2, 2, strTemperature);
    dtostrf(h, 2, 2, strRH);

    double voltage = ESP.getVcc()/1000.0;
    //Serial.print("Voltage is ");
    //Serial.println(voltage);


    //String messagebody = String( "{\"state\":{") + "\"temperature\":\"" + strTemperature + "\"" + ", \"humidity\":\"" + strRH + "\"" "}}"  ;
    String messagebody = String( "{\"state\":{") + "\"reported\":{" + "\"temperature\":\"" + t + "\"" + ", \"humidity\":\"" + h + "\"" + ", \"voltage\":\"" + voltage + "\"" "}}}"  ;

    Serial.println(aws_topic);
    Serial.println(messagebody);

    strcpy(buf, messagebody.c_str());
    //strcpy(buf, "{\"state\":{\"reported\":{\"temperature\": false}, \"desired\":{\"on\": false}}}");
    message.qos = MQTT::QOS1;
    message.retained = true;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(aws_topic, message);
}


void setup() {
    Serial.begin (115200);
    delay (2000);
    Serial.setDebugOutput(1);

    dht.begin();

    //fill with ssid and wifi password
    //WiFiMulti.addAP(wifi_ssid, wifi_password);
    //WiFi.setOutputPower(0);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);

    Serial.println ("connecting to wifi");
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print (".");
    }
    Serial.println ("\nconnected");

    //fill AWS parameters
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);

    if (connect ()){
      subscribe ();
      sendmessage ();
    }

}

void loop() {

  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
      client->yield();
      sendmessage();
  } else {
    //handle reconnection
    if (connect ()){
//      subscribe ();
    }
  }
  ESP.deepSleep(15 * 60* 1000000,WAKE_RF_DEFAULT);
  //ESP.deepSleep(1 * 60* 1000000,WAKE_RF_DEFAULT);
}
