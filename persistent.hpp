// ************************************************************************************
// IoT / MQTT ultrasonic sensor
// ************************************************************************************
// ************************************************************************************
// Persistent.hpp
// ************************************************************************************
// Copyright (c) 2021 - Patrick Fial
// ************************************************************************************

// ************************************************************************************
// Includes
// ************************************************************************************

class Persistent
{
  public:
    enum PersistenceIndices
    {
      piDeviceId = 0,
      piLastMeasure,
      piSkippedUpdates
    };

    Persistent();

    int getValue(int index = 0);
    void setValue(int index, int to);
    bool isInitialized() { return initialized; }
    
  private:
    int initialized;
    
    int readInt(int offset);
    void writeInt(int offset, int value);
};
