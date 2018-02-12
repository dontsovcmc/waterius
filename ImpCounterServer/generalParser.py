import datetime
import struct


# Takes a raw incoming tcp packet and parses it
class generalParse:

    # instanciation requires some timing variables in order to correctly timestamp measurements.
    def __init__(self, logging):
        self.logging = logging
        self.lastPacketReceivedAt = datetime.datetime(2000,1,1,0,0,0)


    # Takes a raw packet as input. It determines if the time between measurements can be calculated or not.
    # If it can't, that provided values attiny are used.
    # Then it traverse the packet and gets all the measurement values
    def parsePacket(self, packet):
        self.packet = packet
        self.dataPos = 0

        self.getDeviceSettings()
        packetReceivedAt = datetime.datetime.now()

        secondsSinceLastPacket = (packetReceivedAt - self.lastPacketReceivedAt).total_seconds()
        if abs(self.expectedWakeupTime-secondsSinceLastPacket) < self.expectedWakeupTime / 3: 
            # Last packet received within 33% allowed deviation.
            timeBetweenMeasurents = secondsSinceLastPacket / self.measurementsReceived()
            self.logging.info("Calculated time between measurements = " + str(timeBetweenMeasurents) )
        else:
            # It's been too long time since last packet, we assume a fixed time between measurements
            timeBetweenMeasurents = self.expectedWakeupTime / self.expectedMeasurementsPerWakeup
            self.logging.info("Expected time between measurements = " + str(timeBetweenMeasurents) )

        sensorNumber = 0
        # The first measurement timestamp is calculated back from when we received the data and the number of values received.
        at = packetReceivedAt - datetime.timedelta( seconds=timeBetweenMeasurents * self.measurementsReceived() )
        while self.bytesLeft() > 1: # While we still have unprocessed bytes in the packet
            self.handleNextMeasurement(at, sensorNumber) # Here lies all the intelligence of interpreting datastructure
            sensorNumber += 1
            if sensorNumber >= self.sensorCount: # When iterated all sensors, we prepare for a new measurement
                sensorNumber = 0
                at += datetime.timedelta( seconds=timeBetweenMeasurents ) # calculate time of next measurement

        self.lastPacketReceivedAt = packetReceivedAt


    # Extracts a byte from received data and moves the data pointer forward
    def getByte(self):
        value = self.packet[self.dataPos]
        self.dataPos += 1
        return value


    # Extracts an unsigned integer from received data and moves the data pointer forward
    def getUInt(self):
        i1 = self.packet[self.dataPos]
        self.dataPos += 1
        i2 = self.packet[self.dataPos]
        self.dataPos += 1
        i3 = self.packet[self.dataPos]
        self.dataPos += 1
        i4 = self.packet[self.dataPos]
        self.dataPos += 1
        return i1 | i2 << 8 | i3 << 16 | i4 << 24


    # Extracts a float from received data and moves the data pointer forward 
    def getFloat(self):
        binFloat = bytearray(4)
        for i in range(0,4):
            binFloat[i] = self.packet[self.dataPos]
            self.dataPos += 1
        floatValue = struct.unpack('f', binFloat)
        return floatValue[0]


    # Returns the amount of unprocessed bytes in the packet
    def bytesLeft(self):
        return len(self.packet) - self.dataPos


    # Returns the number of measurements received
    def measurementsReceived(self):
        return ( len(self.packet) - 9 ) / self.bytesPerMeasurement # adjusting for stats in begining of packet


    # To be overloaded for discecting sensor data. Here we do nothing. 
    # Doesn't make sense, because we don't know the specific sensors at this point
    # Here lies all the intelligence of interpreting datastructure
    def handleNextMeasurement(self, at, sensorNumber):
        return True


    # Gets all the timing settings and deviceID from device.
    def getDeviceSettings(self):
        self.bytesReady = self.getUInt()
        self.expectedWakeupTime = self.getUInt()
        self.measurementsEvery = self.getUInt()
        self.vcc = self.getUInt()
        self.bytesPerMeasurement = self.getByte()
        self.deviceID = self.getByte()
        self.sensorCount = self.getByte()
        dymmy = self.getByte()
        self.expectedMeasurementsPerWakeup = self.expectedWakeupTime / self.measurementsEvery
        self.logging.info("Device ID=" + str(self.deviceID) +
                          ", Sensors=" + str(self.sensorCount) +
                          ", Measurement bytes="  + str(self.bytesReady) +
                          ", Bytes/Meas=" + str(self.bytesPerMeasurement) + 
                          ", Measure every=" + str(self.measurementsEvery) +
                          ", Wakeup every=" + str(self.expectedWakeupTime) +
                          ", vcc=" + str(self.vcc)  )

        