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
        # Counter
        if sensorNumber == 0:
            counter = self.getUInt()
            self.logging.info("counter1 = " + str(counter))
            #self.db.writeData("device1", "counter1", counter, at )
        # voltage
        elif sensorNumber == 1:
            counter2 = self.getUInt()
            self.logging.info("counter2 = " + str(counter2))
            #self.db.writeData( "device1", "counter2", counter2, at )
