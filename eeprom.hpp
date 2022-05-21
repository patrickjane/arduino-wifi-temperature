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

class Eeprom
{
  public:
    void begin();

    unsigned long getValue(int index = 0);
    void setValue(int index, unsigned long to);
    bool isInitialized() { return initialized; }
    
  private:
    int initialized;
};