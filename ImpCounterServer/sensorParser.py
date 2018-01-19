from generalParser import generalParse


# Child of the generic paser. It knows excactly what sensors are connected to the ESP8266/Attiny
class deviceID1Parse(generalParse):

    # Set up parent with general values
    def __init__(self, logging, db):
        self.db = db
        self.logging = logging
        generalParse.__init__(self, self.logging)


    # Overload method. This knows about the actual data from ESP. 
    # It gets a calculated time of measurement (at) and a sensornumber. The latter refers to the ordering of the sensorData struct in the ESP8266.
    # In this function we write the measurements to our database
    def handleNextMeasurement(self, at, sensorNumber):
        # Lightsensor
        if sensorNumber == 0:
            lightValue = self.getUInt()
            self.db.writeData( "balcony", "light", float(lightValue), at )
        # SI7021 humidity
        elif sensorNumber == 1:
            humidityValue =  ( 125.0 * self.getUInt() / 65536 ) - 6 # Convert raw humidity to real humidity
            self.db.writeData( "balcony", "humidity", humidityValue, at )
        # SI7021 temperature
        elif sensorNumber == 2:
            temperatureValue = ( 175.72 * self.getUInt() / 65536 ) - 46.85 # Convert raw temperature to real temperature
            self.db.writeData( "balcony", "temperature", temperatureValue, at )