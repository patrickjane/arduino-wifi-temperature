// ************************************************************************************
// IoT / MQTT ultrasonic sensor
// ************************************************************************************
// ************************************************************************************
// sunsensor.ino
// ************************************************************************************
// Copyright (c) 2021 - Patrick Fial
// ************************************************************************************

// ************************************************************************************
// Includes
// ************************************************************************************

#include <MQTT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "NTP.h"

#include "config.h"
#include "icons.h"
#include "persistent.hpp"
#include "eeprom.hpp"

#if defined(USE_UNO)
  #include <WiFiNINA.h>
#elif defined(USE_ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#include "WiFiUdp.h"


// ************************************************************************************
// Globals
// ************************************************************************************

WiFiClient wifiClient;
MQTTClient client;
Persistent persistence;
Eeprom eeprom;
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int wifiIconStep = 0;
bool connected = false;
bool sensorError = false;
char buffer[100+1];
char deviceIdText[100+1];
unsigned long deviceID = 0;
unsigned long lastMeasure = 0;
float lastValue = 0.0;
float minValue = 0.0;
float maxValue = 0.0;
bool reInitMinMax = true;
int currentDayNum = -1;

// ************************************************************************************
// Functions
// ************************************************************************************

void connectMQTT();
void disconnectMQTT();
bool connectWIFI();
void disconnectWIFI();

void doMeasure();
bool updateMQTT();
void updateDisplay(int wifiNum = -1);

// ************************************************************************************
// Setup
// ************************************************************************************

void setup()
{
#ifdef SS_DEBUG
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
#endif

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    PRINTLN(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.display();
  display.setTextColor(WHITE);
  delay(2000);

  if (persistence.isInitialized())
  {
    deviceID = persistence.getValue(Persistent::piDeviceId);

    PRINTF("Read device ID 0x%x from persistence\n", deviceID);
  }
  else
  {
    eeprom.begin();
    
    if (eeprom.isInitialized())
    {
      deviceID = eeprom.getValue();
      PRINTF("Read device ID 0x%x from EEPROM\n", deviceID);

      persistence.setValue(Persistent::piDeviceId, deviceID);
    }
    else
    {
      PRINTLN("Initializing EEPROM");
      deviceID = random(2147483646);
      eeprom.setValue(0, deviceID);
    }

  }

  sprintf(deviceIdText, "ID: 0x%x\n", deviceID);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printlnUTF8(deviceIdText);
  display.display();
  delay(2000);

#if !defined(USE_UNO) && !defined(USE_ESP8266)
  char nameBuf[50];
  snprintf(nameBuf, 50, "%s-0x%x", WIFI_NAME, deviceID);
  WiFi.setHostname(nameBuf);
#endif

  client.begin(MQTT_HOST, MQTT_PORT, wifiClient);
  client.setOptions(0, true, 360000);

  bool succeeded = connect();

  while (!succeeded) 
  {
      PRINTLN("Connect loop in setup");
      delay(20000);
      succeeded = connect();
  }  

  PRINTLN("Starting NTP client");
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

#ifdef NTP_HOSTNAME  
  ntp.begin(NTP_HOSTNAME);
#else
  ntp.begin();
#endif
  
  PRINTLN("Starting temperature sensor");
  sensors.begin();
  PRINTLN("UP");
}

// ************************************************************************************
// loop
// ************************************************************************************

void loop()
{
  delay(500);

  if (!lastMeasure || (millis() - lastMeasure) >= MEASURE_EVERY_SECONDS)
  {
    lastMeasure = millis();
    doMeasure();
  }

  ntp.update();
  client.loop();
  delay(250);

  int dayNum = ntp.day();

  if (currentDayNum == -1 || currentDayNum != dayNum)
  {
    currentDayNum = dayNum;
    minValue = 0.0;
    maxValue = 0.0;
    reInitMinMax = true;
  }

  if (!client.connected()) {
    disconnect();
    delay(2000);
    connect();
  }
}

// ************************************************************************************
// connect/disconnect WIFI+MQTT
// ************************************************************************************

bool connect()
{
  bool res = connectWIFI();

  if (!res)
    return res;
    
  connectMQTT();
  updateDisplay();
  return true;
}

void disconnect()
{
  disconnectMQTT(); 
  disconnectWIFI();
  updateDisplay();
}

const char* status2Text(int status)
{
  switch (status)
  {
    case WL_IDLE_STATUS:     return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:       return "WL_CONNECTED";
    case WL_CONNECT_FAILED:  return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:    return "WL_DISCONNECTED";
    case WL_NO_SHIELD:       return "WL_NO_SHIELD";
    default: break;
  }

  snprintf(buffer, 100, "Unknown (%d)", status);

  return buffer;
}

// ************************************************************************************
// connect/disconnect WIFI
// ************************************************************************************

bool connectWIFI()
{
  PRINTF("[WIFI] Connecting to network %s ...", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  static const int maxTries = 600;

  while (WiFi.status() != WL_CONNECTED) 
  {
    if (tries++ >= maxTries)
    {
        PRINTF("\n[WIFI] Giving up after %d tries \n", maxTries);
        return false;
    }

    updateDisplay(wifiIconStep++);
    PRINT(".");
    delay(400);
  }

  PRINTLN(" connected");
  return true;
}

void disconnectWIFI()
{
  PRINTF("[WIFI] Disconnecting to network %s ...\n", WIFI_SSID);
  WiFi.disconnect();
  PRINTLN("[WIFI] Disconnected");
  connected = false;
}

// ************************************************************************************
// connect/disconnect MQTT
// ************************************************************************************

void connectMQTT() 
{
  PRINTF("[MQTT] Connecting to host %s ...", MQTT_HOST);

  while (!client.connect(MQTT_ID)) 
  {
    PRINT(".");
    updateDisplay(wifiIconStep++);
    delay(400);
  }

  connected = true;
  PRINTLN(" connected");
}

void disconnectMQTT()
{
  PRINTF("[MQTT] Disonnecting from host %s ...\n", MQTT_HOST);
  client.disconnect();
  PRINTLN("[MQTT] Disconnected");
  connected = false;
}

// ************************************************************************************
// doMeasure
// ************************************************************************************

void doMeasure()
{
  sensorError = false;
  sensors.requestTemperatures();

  float tempC = sensors.getTempCByIndex(0);
 
  // Check if reading was successful
 
  if (tempC == DEVICE_DISCONNECTED_C) 
  {
    sensorError = true;
    updateDisplay();
    Serial.println("Error: Could not read temperature data");
    return;
  }

  lastValue = tempC;

  if (reInitMinMax || lastValue < minValue)
    minValue = lastValue;

  if (reInitMinMax || lastValue > maxValue)
    maxValue = lastValue;

  reInitMinMax = false;

  updateDisplay();
  updateMQTT();
}

// ************************************************************************************
// updateDisplay
// ************************************************************************************

void updateDisplay(int wifiNum)
{
  display.clearDisplay();
  display.setTextSize(2.5);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  sprintf(buffer, "%2.1f°C", lastValue);
  display.printlnUTF8(buffer);

  if (wifiNum >= 0)
  {
    int _wifiNum = (wifiNum % 3 + 1);
  
    switch (_wifiNum)
    {
      case 1: display.drawBitmap(128-16, 0, wifi1_icon16x16, 16, 16, SSD1306_WHITE); break;
      case 2: display.drawBitmap(128-16, 0, wifi2_icon16x16, 16, 16, SSD1306_WHITE); break;
      case 3: display.drawBitmap(128-16, 0, wifi3_icon16x16, 16, 16, SSD1306_WHITE); break;
      default: break;
    }
  } 
  else if (connected)
  {
    display.drawBitmap(128-16, 0, wifi3_icon16x16, 16, 16, SSD1306_WHITE);
  }

  if (sensorError) 
  {
    display.drawBitmap(128-16, 20, noconnection_icon16x16, 16, 16, SSD1306_WHITE);
  }
    
  int xpos = 0;
  int ypos = 48;
  display.drawBitmap(xpos, ypos+1, arrow_down_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setTextSize(1);
  display.setCursor(xpos, ypos);
  sprintf(buffer, "%2.1f°C", minValue);
  display.printlnUTF8(buffer);
  
  xpos += 51;
  display.drawBitmap(xpos, ypos+1, arrow_up_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setCursor(xpos, ypos );
  sprintf(buffer, "%2.1f°C", maxValue);
  display.printlnUTF8(buffer);
  display.display();
}

// ************************************************************************************
// updateMQTT
// ************************************************************************************

bool updateMQTT()
{
  char topicBuffer[200];
  snprintf(buffer, 100, "{ \"value\": %2.2f }", lastValue);
  snprintf(topicBuffer, 100, "%s/0x%x", TOPIC_STATE, deviceID);

  bool res = client.publish(topicBuffer, buffer, true, 0);

  if (!res)
    PRINTLN("Failed to publish new sensor value");
  else
    PRINTF("-> %s [%s]\n", topicBuffer, buffer);

  return res;
}
