#ifndef _SENSOR_h
#define _SENSOR_h


#define TSL2561_ADDR		  0x39 
#define TSL2561_CMD           0x80
#define	TSL2561_REG_DATA_0    0x0C
#define	TSL2561_REG_DATA_1    0x0E
#define	TSL2561_REG_CONTROL   0x00
#define	TSL2561_REG_TIMING    0x01

#define SI7021_ADDR			  0x40
#define SI7021_TEMP_MEASURE   0xF3
#define SI7021_HUMD_MEASURE   0xF5

#include <Arduino.h>



class SensorTsl {
 protected:
	 void writeByte( uint8_t address, uint8_t value );
	 uint16_t readUInt( unsigned char address );
 public:
	 uint16_t readLux();
};



class SensorSi {
protected:
public:
	float readTemp();
	float readHumidity();
	uint16_t makeMeasurment( uint8_t command );
};


#endif

