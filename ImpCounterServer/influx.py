from influxdb import InfluxDBClient
import time

# Class that handles storage of values in the influx db database.
class influx:

    # When instanciated it connects to the database with provided credentials and serverinfo
    def __init__(self, logging, host, port, user, passwd, db):
        self.client = InfluxDBClient(host, port, user, passwd, db)
        logging.info("Influx DB connected")
        self.logging = logging


    # Writes one measurement to the influxdb database.
    def writeData(self, place, measurement, value, time):
        timeEpochNs = int(time.strftime('%s')) * 1000000000  
        timeStr = time.strftime("%y-%m-%d %H:%M:%S")

        json_body = [
            {
                "measurement": measurement,
                "tags": {
                    "place": place
                },
                "time": timeEpochNs,
                "fields": {
                    "value": value
                }
            }
        ]
        self.client.write_points(json_body)
        self.logging.info("Influx write: place=" + place + ", " + measurement + "=" + str(value) + ", Time=" + timeStr ) 
