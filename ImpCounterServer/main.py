import logging
from server import server
from influx import influx
from sensorParser import *


# Configuration
LISTEN_HOST = '192.168.50.10'
LISTEN_PORT = 5001

INFLUX_HOST = 'localhost'
INFLUX_PORT = 8086
INFLUX_USER = 'Username'
INFLUX_PASS = 'Password'
INFLUX_DB   = 'Weather'

LOG_LEVEL   = logging.INFO


# Setup logging object for passing to all modules
logging.basicConfig(level=LOG_LEVEL, format='%(asctime)s %(levelname)s %(message)s')

# Create database object used for communicating with influxdb
db = influx(logging, INFLUX_HOST, INFLUX_PORT, INFLUX_USER, INFLUX_PASS, INFLUX_DB)

# Create a parser object for each device. Each one is responsible of discecting packets into measurement data and then store it in the database
# We give it a reference to our database object
devID1parse = deviceID1Parse(logging, db)

# Now we create a parser list of the callback functions of each paser. The TCP listener needs this
# in order to know what to do with each device ID
parserList = {}
parserList[1] = devID1parse.parsePacket

# Startup the TCP listener. Tell it that it should throw received raw packets at the parser
srv = server(logging, LISTEN_HOST, LISTEN_PORT, parserList )
srv.startServer()

# We will never end here, because startServer is in an infinite loop.