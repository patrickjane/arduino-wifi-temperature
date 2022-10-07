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
#include <time.h>

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

short wifiIconStep = 0;
unsigned short sensorErrors = 0;
bool nightMode = false;
time_t displayStart = 0;
time_t displayEnd = 0;
struct tm timeStruct;
bool connected = false;
bool sensorError = false;
char buffer[100+1];
char deviceIdText[30+1];
char currentTimeString[7];
unsigned long deviceID = 0;
unsigned long lastMeasure = 0;
float lastTemperatureValue = 0.0;
float minTemperatureValue = 0.0;
float maxTemperatureValue = 0.0;
float lastHumidityValue = 0.0;
bool reInitMinMax = true;
short currentDayNum = -1;

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
void setNightMode(unsigned long fromHour, unsigned long fromMinute, unsigned long toHour, unsigned long toMinute);
void setNightMode(const char* from, const char* to, bool saveToEeprom, bool off = false);

// ************************************************************************************
// Setup
// ************************************************************************************

void setup()
{
#ifdef SS_DEBUG
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
#endif

  PRINTLN("Init display");

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    PRINTLN(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  memset(currentTimeString, 0, sizeof(currentTimeString));

  PRINTLN("Starting temperature sensor");

#ifdef USE_BOSCH_TEMP
  bool status = bme.begin(SENSOR_ADDRESS);
  
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
  display.ssd1306_command(SSD1306_SETCONTRAST); 
  display.ssd1306_command(1);
  delay(2000);

  eeprom.begin();
  
  if (eeprom.isInitialized())
  {
    deviceID = eeprom.getValue(eiDeviceID);
    PRINTF("Read device ID 0x%x from EEPROM\n", deviceID);

    unsigned long fromHour = eeprom.getValue(eiNightModeStartHour);
    unsigned long fromMinute = eeprom.getValue(eiNightModeStartMinute);
    unsigned long toHour = eeprom.getValue(eiNightModeEndHour);
    unsigned long toMinute = eeprom.getValue(eiNightModeEndMinute);

    if (fromHour < 30)
    {
      setNightMode(fromHour, fromMinute, toHour, toMinute);
    }
#if defined(DISPLAY_START) && defined(DISPLAY_END)
    else if (fromHour != 777)
    {
      setNightMode(DISPLAY_START, DISPLAY_END, false);
    }
#endif    
  }
  else
  {
    PRINTLN("Initializing EEPROM");
    deviceID = random(2147483646);
    eeprom.setCookie();
    eeprom.setValue(eiDeviceID, deviceID);

#if defined(DISPLAY_START) && defined(DISPLAY_END)
    setNightMode(DISPLAY_START, DISPLAY_END, true);
#else
    eeprom.setValue(eiNightModeStartHour, 100);
    eeprom.setValue(eiNightModeStartMinute, 100);
    eeprom.setValue(eiNightModeEndHour, 100);
    eeprom.setValue(eiNightModeEndMinute, 100);
#endif

    eeprom.commit();
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
#ifdef TOPIC_CONTROL  
  client.onMessageAdvanced(onMessageReceived);
#endif

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
  static int lastMinute = -1;

  if (!lastMeasure || (millis() - lastMeasure) >= MEASURE_EVERY_SECONDS)
  {
    lastMeasure = millis();
    doMeasure();

#ifdef WIFI_ENABLED
    ntp.update();
#endif
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

  timeStruct.tm_hour = ntp.hours();
  timeStruct.tm_min = ntp.minutes();
  time_t nowTime = mktime(&timeStruct);

  snprintf(currentTimeString, 6, "%0.2d:%0.2d", ntp.hours(), ntp.minutes());

  if (lastMinute != ntp.minutes())
  {
    updateDisplay();
    lastMinute = ntp.minutes();
  }
  
  bool nightModeNow = displayStart != 0 && !(nowTime >= displayStart && nowTime < displayEnd);

  if (nightModeNow != nightMode)
  {
    if (nightModeNow)
      display.dim(true);
    else
      display.dim(false);

    nightMode = nightModeNow;
    
    PRINTF("Night mode switched to: %s\n", nightMode ? "on" : "off");
  }      

  if (!client.connected()) {
    PRINTLN("MQTT client lost connection, disconnecting WiFi and trying to reconnect ...");
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

  ntp.update();
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

  PRINTLN(" connected");  

#ifdef TOPIC_CONTROL
  char topicBuffer[200];
  snprintf(topicBuffer, 200, "%s/0x%x", TOPIC_CONTROL, deviceID);
  
  PRINTF("[MQTT] Subscribing to control topic %s ...\n", topicBuffer);
  
  if (!client.subscribe(topicBuffer))
  {
    PRINT(".");
    updateDisplay(wifiIconStep++);
    delay(400);
  }  
#endif

  connected = true;
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
  bool hadSensorError = sensorError;

  sensorError = false;
  
#ifdef USE_BOSCH_TEMP
  tempC = bme.readTemperature();
  hum = bme.readHumidity();

  if (isnan(tempC) || isnan(hum))
  {
    if (!hadSensorError)
      sensorErrors++;

    sensorError = true;
    updateDisplay();
    PRINTLN("Error: Could not read temperature data");
    return;
  }
#endif

#ifdef USE_DALLAS_TEMP
  sensors.requestTemperatures();

  tempC = sensors.getTempCByIndex(0);
 
  // Check if reading was successful
 
  if (tempC == DEVICE_DISCONNECTED_C) 
  {
    if (!hadSensorError)
      sensorErrors++;
    
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
      case 1: display.drawBitmap(128-16, 8, wifi1_icon16x16, 16, 16, SSD1306_WHITE); break;
      case 2: display.drawBitmap(128-16, 8, wifi2_icon16x16, 16, 16, SSD1306_WHITE); break;
      case 3: display.drawBitmap(128-16, 8, wifi3_icon16x16, 16, 16, SSD1306_WHITE); break;
      default: break;
    }
  } 
  else if (connected)
  {
    display.drawBitmap(128-16, 8, wifi3_icon16x16, 16, 16, SSD1306_WHITE);
  }

  if (sensorError) 
  {
    display.drawBitmap(128-16, 28, noconnection_icon16x16, 16, 16, SSD1306_WHITE);
  }
    
  int xpos = 0;
  int ypos = 32;

#ifdef USE_BOSCH_TEMP
  display.drawBitmap(xpos, ypos+1, drop_icon16x16, 16, 16, SSD1306_WHITE);
  xpos += 14;
  display.setTextSize(1);
  display.setCursor(xpos, ypos);
  sprintf(buffer, "%.0f%%", lastHumidityValue);
  display.printlnUTF8(buffer);
#endif

  if (strlen(currentTimeString))
  {
    xpos += 53;
    display.setTextSize(1);
    display.setCursor(xpos, ypos);
    display.printlnUTF8(currentTimeString);
  }
  
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
  snprintf(buffer, 100, "{ \"temperature\": %2.2f, \"humidity\": %2.2f, \"errors\": %d }", lastTemperatureValue, lastHumidityValue, sensorErrors);
#elif defined(USE_DALLAS_TEMP)
  snprintf(buffer, 100, "{ \"temperature\": %2.2f, \"errors\": %d }", lastTemperatureValue, sensorErrors);
#endif

  snprintf(topicBuffer, 100, "%s/0x%x", TOPIC_STATE, deviceID);

  res = client.publish(topicBuffer, buffer, false, 0);

  if (!res)
    PRINTLN("Failed to publish new sensor value");
  else
    PRINTF("-> %s [%s]\n", topicBuffer, buffer);
#endif

  return res;
}

// ************************************************************************************
// onMessageReceived
// ************************************************************************************

void onMessageReceived(MQTTClient* client, char* topic, char* payload, int payload_length) 
{
  char topicBuffer[200];
  snprintf(topicBuffer, 200, "%s/0x%x", TOPIC_CONTROL, deviceID);
    
  if (strcmp(topic ? topic : "", topicBuffer))
  {
    PRINTF("Ignoring message for unexpected topic %s\n", topic ? topic : "");
    return;
  }
  
  if (!payload || !*payload)
  {
    PRINTF("Ignoring message without payload\n");
    return;
  }

  char command[30+1]; *command = 0;

  if (!getJsonValue(payload, "command", command, 30))
  {
    PRINTF("Ignoring message without 'command' property\n");
    return;
  }

  if (!strcmp(command, "setNightMode"))
  {
    char from[10]; *from= 0;
    char to[10]; *to= 0;
    char off[10]; *off= 0;    

    if (!getJsonValue(payload, "from", from, 10) && !getJsonValue(payload, "off", off, 10))
    {
      PRINTF("Ignoring setNightMode message without 'from'/'off' property\n");
      return;
    }

    if (!strcmp(off, "true"))
    {
      setNightMode("", "", true, true);
      eeprom.commit();
      return;
    }

    if (!getJsonValue(payload, "to", to, 10))
    {
      PRINTF("Ignoring setNightMode message without 'to' property\n");
      return;
    }

    setNightMode(from, to, true);
    eeprom.commit();
  }

  if (!strcmp(command, "display"))
  {
    char off[10]; *off= 0;    

    if (!getJsonValue(payload, "off", off, 10))
    {
      PRINTF("Ignoring display message without 'off' property\n");
      return;
    }

    if (!strcmp(off, "true"))
      display.dim(true);
    else
      display.dim(false);
  }  
}

// ************************************************************************************
// setNightMode
// ************************************************************************************

void setNightMode(const char* from, const char* to, bool saveToEeprom, bool off)
{
  if (off)
  {
    if (saveToEeprom)
    {
      eeprom.setValue(eiNightModeStartHour, 777);
      eeprom.setValue(eiNightModeStartMinute, 777);
      eeprom.setValue(eiNightModeEndHour, 777);
      eeprom.setValue(eiNightModeEndMinute, 777);
      displayStart = displayEnd = 0;

      PRINTF("Display now always on\n");
    }
    
    return;  
  }
  
  if (!from || !*from || !to || !*to || strlen(from) != 5 || strlen(to) != 5)
    return;
    
  char buf1[50];
  char buf2[50];

  memset(&timeStruct, 0, sizeof(struct tm));

  timeStruct.tm_hour = atoi(from);
  timeStruct.tm_min = atoi(from+3);
  displayStart = mktime(&timeStruct);

  if (saveToEeprom)
  {
    eeprom.setValue(eiNightModeStartHour, timeStruct.tm_hour);
    eeprom.setValue(eiNightModeStartMinute, timeStruct.tm_min);
  }

  strftime(buf1, 50, "%R", &timeStruct);  

  timeStruct.tm_hour = atoi(to);
  timeStruct.tm_min = atoi(to+3);
  displayEnd = mktime(&timeStruct);

  if (saveToEeprom)
  {
    eeprom.setValue(eiNightModeEndHour, timeStruct.tm_hour);
    eeprom.setValue(eiNightModeEndMinute, timeStruct.tm_min);
  }

  strftime(buf2, 50, "%R", &timeStruct);  

  PRINTF("Display on between %s - %s\n", buf1, buf2);
}

void setNightMode(unsigned long fromHour, unsigned long fromMinute, unsigned long toHour, unsigned long toMinute)
{
  char buf1[50];
  char buf2[50];

  memset(&timeStruct, 0, sizeof(struct tm));

  timeStruct.tm_hour = fromHour;
  timeStruct.tm_min = fromMinute;
  displayStart = mktime(&timeStruct);

  strftime(buf1, 50, "%R", &timeStruct);  

  timeStruct.tm_hour = toHour;
  timeStruct.tm_min = toMinute;
  displayEnd = mktime(&timeStruct);

  strftime(buf2, 50, "%R", &timeStruct);  

  PRINTF("Display on between %s - %s (e)\n", buf1, buf2);
}

// ************************************************************************************
// getJsonValue
// ************************************************************************************

bool getJsonValue(const char* source, const char* property, char* dest, int destLen)
{
  if (!source || !strlen(source))
    return false;

  char* p = strstr(source, property);
  char* p2;

  if (!p)
    return false;

  p += strlen(property);

  while (*p && (*p == ' ' ||  *p == ':' || *p == '"'))
    p++;

  if (!(*p))
    return false;

  bool isString = *(p-1) == '"';
  p2 = p;

  if (isString)
  {
    while (*p && (*p != '"' || *(p-1) == '\\'))
      p++;
  }
  else
  {
    while (*p && *p != ',' && *p != ' ' && *p != '}' && *p != ']')
      p++;
  }

  if (!(*p))
    return false;

  int valueLen = p - p2;

  if (valueLen > 0 && valueLen < destLen+1)
  {
    strncpy(dest, p2, valueLen);
    dest[valueLen] = 0;
    return true;
  }

  return false;
}
