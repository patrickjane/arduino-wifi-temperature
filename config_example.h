// ************************************************************************************
// Board configuration
// ************************************************************************************

//#define USE_ESP32            // use THIS line when using Sparkfun Thing ESP32 / Firebeetle ESP32 / Weemos LOLIN 32 lite ...
#define USE_ESP8266          // use THIS line when using NodeMCU / Firebeetle ESP8266
//#define USE_UNO              // use THIS line when using Arduino Uno wifi

// ************************************************************************************
// Temperature sensor
// ************************************************************************************

#define USE_BOSCH_TEMP          // use THIS line when using BME280 temperature/humidity sensor
//#define USE_DALLAS_TEMP         // use THIS line when using DS18B20 temperature sensor

#define MEASURE_EVERY_SECONDS  30 * 1000       // measure every 30s
#define TEMP_SENSOR_PIN        13              // read pin for DS18B20 temperature sensor
                                               // unused if using BOSCH BME280 temperature sensor (will use I2C)

// ************************************************************************************
// WiFi + MQTT + NTP
// ************************************************************************************

#define WIFI_ENABLED
#define WIFI_PASS    "MY_WIFI_SSID"
#define WIFI_SSID    "MY_WIFI_PASSWORD"
#define WIFI_NAME    "terr_tempsensor"

#define MQTT_ID      "MY_MQTT_HOSTNAME"
#define MQTT_HOST    "MY_MQTT_HOSTNAME"
#define MQTT_PORT    1883

#define TOPIC_STATE  "ha/sensor/tempsensor/status"

#define NTP_HOSTNAME   "ceres.universe"

// ************************************************************************************
// OLED Display
// ************************************************************************************

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
// On a NodeMCU:             D2=4(SDA),  D1=5(SCL) (lib default)

#define SCREEN_WIDTH           128    // OLED display width, in pixels
#define SCREEN_HEIGHT          64     // OLED display height, in pixels
#define OLED_RESET             -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS         0x3C   //< See datasheet for Address

// ************************************************************************************
// serial log messages / debugging
// ************************************************************************************

#define SS_DEBUG

#ifdef SS_DEBUG
#  define PRINTLN Serial.println
#  define PRINT   Serial.print
#  define PRINTF  Serial.printf
#else
#  define PRINTLN //
#  define PRINT   //
#  define PRINTF  //
#endif
