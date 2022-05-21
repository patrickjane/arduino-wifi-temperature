// ************************************************************************************
// IoT / MQTT ultrasonic sensor
// ************************************************************************************
// ************************************************************************************
// Persistent.cpp
// ************************************************************************************
// Copyright (c) 2021 - Patrick Fial
// ************************************************************************************

#define EEPROM_SIZE sizeof(short) + sizeof(long)
#define EEPROM_COOKIE 8472

// ************************************************************************************
// Includes
// ************************************************************************************

#include "eeprom.hpp"
#include "config.h"

#include <EEPROM.h>

// ************************************************************************************
// ctor/dtor
// ************************************************************************************

void Eeprom::begin()
{
#ifdef USE_ESP32
  EEPROM.begin(EEPROM_SIZE);
#endif

#ifdef USE_ESP8266
   EEPROM.begin(EEPROM_SIZE);
#endif
 
  short cookie = 0;
  EEPROM.get(0, cookie);

  initialized = cookie == EEPROM_COOKIE;
}

// ************************************************************************************
// getValue
// ************************************************************************************

unsigned long Eeprom::getValue(int index)
{
  if (!isInitialized())
    return -1;

  unsigned long res = -1;

  EEPROM.get(sizeof(short), res);  

  return res;
}

// ************************************************************************************
// setValue
// ************************************************************************************

void Eeprom::setValue(int index, unsigned long to)
{
  short cookie = EEPROM_COOKIE;
  EEPROM.put(0, cookie);
  EEPROM.put(sizeof(short) + index*sizeof(int), to);
  EEPROM.commit();
}
