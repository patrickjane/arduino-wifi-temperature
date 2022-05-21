// ************************************************************************************
// Board configuration
// ************************************************************************************

//#define USE_ESP32     // use THIS line when using Sparkfun Thing ESP32 / Firebeetle ESP32 / Weemos LOLIN 32 lite ...
#define USE_ESP8266   // use THIS line when using NodeMCU / Firebeetle ESP8266
//#define USE_UNO       // use THIS line when using Arduino Uno wifi

// ************************************************************************************
// WiFi + MQTT
// ************************************************************************************

#define WIFI_PASS    "MY_WIFI_SSID"
#define WIFI_SSID    "MY_WIFI_PASSWORD"
#define WIFI_NAME    "terr_tempsensor"

#define MQTT_ID      "MY_MQTT_HOSTNAME"
#define MQTT_HOST    "MY_MQTT_HOSTNAME"
#define MQTT_PORT    1883

#define TOPIC_STATE  "ha/sensor/tempsensor/status"

// ************************************************************************************
// OLED Display, NTP & temperature sensor
// ************************************************************************************

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
// On a NodeMCU:             4(SDA),  5(SCL) (lib default), D1=5 and D2=4

#define SCREEN_WIDTH           128    // OLED display width, in pixels
#define SCREEN_HEIGHT          64     // OLED display height, in pixels
#define OLED_RESET             -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS         0x3C   //< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define TEMP_SENSOR_PIN        13
#define MEASURE_EVERY_SECONDS  30 * 1000

#define NTP_HOSTNAME           "MY_NTP_HOSTNAME"

// ************************************************************************************
// serial log messages
// ************************************************************************************

//#define SS_DEBUG

#ifdef SS_DEBUG
#  define PRINTLN Serial.println
#  define PRINT   Serial.print
#  define PRINTF  Serial.printf
#else
#  define PRINTLN //
#  define PRINT   //
#  define PRINTF  //
#endif
