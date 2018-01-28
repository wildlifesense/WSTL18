#!/usr/bin/python3
#
# This software is open source bla bla bla (I'll refine this later)
#
import serial
from serial.tools import list_ports

print("Serial devices: \n")

iternum = 1

# List all serial ports found on this computer (that the serial library can use).
for device in list_ports.comports():
    print(str(iternum) + ") " + device.device + "\n")
    iternum += 1


# Assuming there's only one serial device attached to /dev/ttyUSB0. 1Mbits with 2 stopbits as set on WSTL18.
ser = serial.Serial('/dev/ttyUSB0', 1000000, stopbits=2, timeout=None)
#ser.flush()

# List the device used.
print("Opened " + ser.name)

#the_file = open('somefile.txt', 'a')
#the_counter = 1000000

while(1):
    #the_counter -= 1
    x = ser.read()
    print(x)
    ser.flush()     # I was having issues with byte print order, but I don't think it's relevant any more.
    #the_file.write("%s" % x)
    #temperature_reading = ser.read(2)#.decode()
    #temp =  round(int.from_bytes(temperature_reading, 'big') * 0.00390625, 1)
    #print("%.1f" % temp)

"""
try:
    dataIn = self.port.read()
except serial.SerialException as e:
    #There is no new data from serial port
    return None
except TypeError as e:
    #Disconnect of USB->UART occured
    self.port.close()
    return None
else:
    #Some data was received
    return dataIn

"""
