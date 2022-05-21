// ************************************************************************************
// IoT / MQTT ultrasonic sensor
// ************************************************************************************
// ************************************************************************************
// Persistent.cpp
// ************************************************************************************
// Copyright (c) 2021 - Patrick Fial
// ************************************************************************************

#define COOKIE 8472
#define BUF_SIZE 256

#include "config.h"

#ifdef USE_ESP32
#   include <Esp.h>
#   include <string.h>
#   define BUF_SIZE 16
    RTC_DATA_ATTR int values[16];
#endif

// ************************************************************************************
// Includes
// ************************************************************************************

#include "persistent.hpp"
#include "config.h"

// ************************************************************************************
// ctor/dtor
// ************************************************************************************

Persistent::Persistent()
{
  initialized = readInt(0) == COOKIE;
}

// ************************************************************************************
// getValue
// ************************************************************************************

int Persistent::getValue(int index)
{
  if (!isInitialized())
    return -1;

#ifdef USE_ESP32
  return readInt(index + 1);
#endif

  return readInt(sizeof(int) + index * sizeof(int));
}

// ************************************************************************************
// setValue
// ************************************************************************************

void Persistent::setValue(int index, int to)
{
#ifdef USE_ESP32
  writeInt(0, COOKIE);
  writeInt(index + 1, to);
  initialized = 1;
#endif

#ifdef USE_8266
  uint32_t cookie = COOKIE;
  uint32_t val = to;
  ESP.rtcUserMemoryWrite(0, &cookie, sizeof(uint32_t));
  ESP.rtcUserMemoryWrite(sizeof(int), (uint32_t*)&val, sizeof(uint32_t));
#endif
}

// ************************************************************************************
// readInt
// ************************************************************************************

int Persistent::readInt(int offset)
{
  int res = -1;

  if (offset >= BUF_SIZE)
    return res;

#ifdef USE_ESP32
  res = values[offset];
#endif

#ifdef USE_EPS8266
  ESP.rtcUserMemoryRead(sizeof(uint32_t), (uint32_t*)&res, sizeof(uint32_t));
#endif

  return res;
}

// ************************************************************************************
// writeInt
// ************************************************************************************

void Persistent::writeInt(int offset, int value)
{
  int val = val;

  if (offset >= BUF_SIZE)
    return;

#ifdef USE_ESP32  
  values[offset] = value;
#endif

#ifdef USE_EPS8266
  ESP.rtcUserMemoryWrite(offset, (uint32_t*)&val, sizeof(uint32_t));
#endif
}
