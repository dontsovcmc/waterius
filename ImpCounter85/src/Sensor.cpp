#include "Sensor.h"
#include <USIWire.h>



/* Read the light from sensor. Set at medium sensitivity 101ms sampling rate*/
uint16_t SensorTsl::readLux() {
	Wire.begin();

	writeByte( TSL2561_REG_TIMING, 0x01 ); // Start integration with 101ms sample time, no gain.
	writeByte( TSL2561_REG_CONTROL, 0x03 ); // Power up TSL2561
	delay( 101 ); // Wait for sampling to end
	uint16_t data0 = readUInt( TSL2561_REG_DATA_0 );
	uint16_t data1 = readUInt( TSL2561_REG_DATA_1 );

	// Calculate the real lux value
	float lux = 0;
	float d0 = data0 * 4 * 16; // integration time 101 and 0 gain.
	float d1 = data1 * 4 * 16;
	float ratio = d1 / d0;

	if ( ratio < 0.5 ) 	lux = 0.0304 * d0 - 0.062 * d0 * pow( ratio, 1.4 );
	else if ( ratio < 0.61 ) lux = 0.0224 * d0 - 0.031 * d1;
	else if ( ratio < 0.80 ) lux = 0.0128 * d0 - 0.0153 * d1;
	else if ( ratio < 1.30 ) lux = 0.00146 * d0 - 0.00112 * d1;
	return lux; // Even though its calculated in float we convert to int to save data in buffer
}



/* Read one byte from I2C slave */
void SensorTsl::writeByte( uint8_t reg, uint8_t value )
{
	Wire.beginTransmission( TSL2561_ADDR );
	Wire.write( ( reg & 0x0F ) | TSL2561_CMD );
	Wire.write( value );
	Wire.endTransmission();
}



/* Read an uint from I2C slave */
uint16_t SensorTsl::readUInt( unsigned char address )
{
	char high, low;
	uint16_t value;

	Wire.beginTransmission( TSL2561_ADDR );
	Wire.write( ( address & 0x0F ) | TSL2561_CMD );
	Wire.endTransmission();

	Wire.requestFrom( TSL2561_ADDR, 2 );
	low = Wire.read();
	high = Wire.read();
	value = word( high, low );
	return value;
}



/* Get raw temperature from sensor. Real temp is calculated on server */
float SensorSi::readTemp() {
	return makeMeasurment( SI7021_TEMP_MEASURE );
}



/* Get raw humidity from sensor. Real humidity is calculated on server */
float SensorSi::readHumidity() {
	return makeMeasurment( SI7021_HUMD_MEASURE );
}



/* Make on measurement on the SI7021 */
uint16_t SensorSi::makeMeasurment( uint8_t command ) {
	Wire.beginTransmission( SI7021_ADDR );
	Wire.write( command );
	Wire.endTransmission();

	delay( 25 ); // Wait for sensor to measure

	Wire.requestFrom( SI7021_ADDR, 3 );

	unsigned int msb = Wire.read();
	unsigned int lsb = Wire.read();
	
	lsb &= 0xFC; // Clear the last to bits of LSB to 00.
	unsigned int mesurment = msb << 8 | lsb;

	return mesurment;
}