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

#include "NTP.h"

#include "config.h"
#include "icons.h"
#include "eeprom.hpp"

#if defined(USE_UNO)
  #include <WiFiNINA.h>
#elif defined(USE_ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#if defined(USE_BOSCH_TEMP)
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
#elif defined(USE_DALLAS_TEMP)
  #include <OneWire.h>
  #include <USE_DALLAS_TEMPTemperature.h>
#endif

#include "WiFiUdp.h"

// ************************************************************************************
// Globals
// ************************************************************************************

WiFiClient wifiClient;
MQTTClient client;
Eeprom eeprom;
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

#if defined(USE_BOSCH_TEMP)
  Adafruit_BME280 bme; // I2C
#elif defined(USE_DALLAS_TEMP)
  OneWire oneWire(TEMP_SENSOR_PIN);
  USE_DALLAS_TEMPTemperature sensors(&oneWire);
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int wifiIconStep = 0;
bool connected = false;
bool sensorError = false;
char buffer[100+1];
char deviceIdText[100+1];
unsigned long deviceID = 0;
unsigned long lastMeasure = 0;
float lastTemperatureValue = 0.0;
float minTemperatureValue = 0.0;
float maxTemperatureValue = 0.0;
float lastHumidityValue = 0.0;
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

  PRINTLN("Starting temperature sensor");

#ifdef USE_BOSCH_TEMP
  bool status = bme.begin(0x76);
  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
#endif

#ifdef USE_DALLAS_TEMP
  sensors.begin();
#endif

  display.display();
  display.setTextColor(WHITE);
  delay(2000);

  eeprom.begin();
  
  if (eeprom.isInitialized())
  {
    deviceID = eeprom.getValue();
    PRINTF("Read device ID 0x%x from EEPROM\n", deviceID);
  }
  else
  {
    PRINTLN("Initializing EEPROM");
    deviceID = random(2147483646);
    eeprom.setValue(0, deviceID);
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

  PRINTLN("Starting NTP client");
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

#ifdef NTP_HOSTNAME  
  ntp.begin(NTP_HOSTNAME);
#else
  ntp.begin();
#endif
  
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

#ifdef WIFI_ENABLED
  if (!connected)
  {
    bool succeeded = connect();

    if (!succeeded)
    {
      PRINTLN("Wifi connect failed, skipping NTP update/MQTT update");
      return;
    }
  }

  ntp.update();
  client.loop();
  delay(250);

  int dayNum = ntp.day();

  if (currentDayNum == -1 || currentDayNum != dayNum)
  {
    currentDayNum = dayNum;
    minTemperatureValue = 0.0;
    maxTemperatureValue = 0.0;
    reInitMinMax = true;
  }

  if (!client.connected()) {
    disconnect();
    delay(2000);
    connect();
  }
#endif
}

// ************************************************************************************
// connect/disconnect WIFI+MQTT
// ************************************************************************************

bool connect()
{
#ifdef WIFI_ENABLED  
  bool res = connectWIFI();

  if (!res)
    return res;
    
  connectMQTT();
  updateDisplay();
#endif
  return true;
}

void disconnect()
{
#ifdef WIFI_ENABLED
  disconnectMQTT(); 
  disconnectWIFI();
  updateDisplay();
#endif
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
#ifdef WIFI_ENABLED
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
#endif
  return true;
}

void disconnectWIFI()
{
#ifdef WIFI_ENABLED
  PRINTF("[WIFI] Disconnecting to network %s ...\n", WIFI_SSID);
  WiFi.disconnect();
  PRINTLN("[WIFI] Disconnected");
  connected = false;
#endif
}

// ************************************************************************************
// connect/disconnect MQTT
// ************************************************************************************

void connectMQTT() 
{
#ifdef WIFI_ENABLED
  PRINTF("[MQTT] Connecting to host %s ...", MQTT_HOST);

  while (!client.connect(MQTT_ID)) 
  {
    PRINT(".");
    updateDisplay(wifiIconStep++);
    delay(400);
  }

  connected = true;
  PRINTLN(" connected");
#endif
}

void disconnectMQTT()
{
#ifdef WIFI_ENABLED
  PRINTF("[MQTT] Disonnecting from host %s ...\n", MQTT_HOST);
  client.disconnect();
  PRINTLN("[MQTT] Disconnected");
  connected = false;
#endif
}

// ************************************************************************************
// doMeasure
// ************************************************************************************

void doMeasure()
{
  float hum = 0.0;
  float tempC = 0.0;
  
#ifdef USE_BOSCH_TEMP
  tempC = bme.readTemperature();
  hum = bme.readHumidity();

  if (isnan(tempC) || isnan(hum))
  {
    sensorError = true;
    updateDisplay();
    PRINTLN("Error: Could not read temperature data");
    return;
  }
#endif

#ifdef USE_DALLAS_TEMP

  sensorError = false;
  sensors.requestTemperatures();

  tempC = sensors.getTempCByIndex(0);
 
  // Check if reading was successful
 
  if (tempC == DEVICE_DISCONNECTED_C) 
  {
    sensorError = true;
    updateDisplay();
    PRINTLN("Error: Could not read temperature data");
    return;
  }
#endif

  lastTemperatureValue = tempC;
  lastHumidityValue = hum;

  if (reInitMinMax || lastTemperatureValue < minTemperatureValue)
    minTemperatureValue = lastTemperatureValue;

  if (reInitMinMax || lastTemperatureValue > maxTemperatureValue)
    maxTemperatureValue = lastTemperatureValue;

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
  display.setTextSize(2.0);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  sprintf(buffer, "%2.1f°C", lastTemperatureValue);
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
  int ypos = 32;

#ifdef USE_BOSCH_TEMP
  display.drawBitmap(xpos, ypos+1, drop_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setTextSize(1);
  display.setCursor(xpos, ypos);
  sprintf(buffer, "%2.0f%%", lastHumidityValue);
  display.printlnUTF8(buffer);
#endif

  xpos = 0;
  ypos = 48;
  display.drawBitmap(xpos, ypos+1, arrow_down_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setTextSize(1);
  display.setCursor(xpos, ypos);
  sprintf(buffer, "%2.1f°C", minTemperatureValue);
  display.printlnUTF8(buffer);
  
  xpos += 51;
  display.drawBitmap(xpos, ypos+1, arrow_up_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setCursor(xpos, ypos );
  sprintf(buffer, "%2.1f°C", maxTemperatureValue);
  display.printlnUTF8(buffer);
  display.display();
}

// ************************************************************************************
// updateMQTT
// ************************************************************************************

bool updateMQTT()
{
  bool res = true;

#ifdef WIFI_ENABLED
  if (!connected)
    return res;
  
  char topicBuffer[200];

#if defined(USE_BOSCH_TEMP)
  snprintf(buffer, 100, "{ \"temperature\": %2.2f, \"humidity\": %2.2f }", lastTemperatureValue, lastHumidityValue);
#elif defined(USE_DALLAS_TEMP)
  OneWire oneWire(TEMP_SENSOR_PIN);
  snprintf(buffer, 100, "{ \"temperature\": %2.2f }", lastTemperatureValue);
#endif

  snprintf(topicBuffer, 100, "%s/0x%x", TOPIC_STATE, deviceID);

  res = client.publish(topicBuffer, buffer, true, 0);

  if (!res)
    PRINTLN("Failed to publish new sensor value");
  else
    PRINTF("-> %s [%s]\n", topicBuffer, buffer);
#endif

  return res;
}
