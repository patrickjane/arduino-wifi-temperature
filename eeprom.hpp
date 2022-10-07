// ************************************************************************************
// IoT / MQTT ultrasonic sensor
// ************************************************************************************
// ************************************************************************************
// Eeprom.hpp
// ************************************************************************************
// Copyright (c) 2021 - Patrick Fial
// ************************************************************************************

// ************************************************************************************
// Includes
// ************************************************************************************

enum EepromIndex
{
   eiDeviceID = 0,
   eiNightModeStartHour,
   eiNightModeStartMinute,
   eiNightModeEndHour,
   eiNightModeEndMinute,
};

class Eeprom
{
  public:
    void begin();

    unsigned long getValue(int index);
    void setCookie();
    void setValue(int index, unsigned long to);
    void commit();
    bool isInitialized() { return initialized; }
    
  private:
    int initialized;
};
