#!/usr/bin/python3
#
# This software is open source bla bla bla (I'll refine this later)
#
import time
import re
import serial
import signal
from serial.tools import list_ports

#
# Set up a handler for Ctrl+c
#
def sigint_handler(sig_number, frame):
    print("\n\nI hope this wasn't messy.")
    quit()

def exit_program(message):
    print(message)
    raise SystemExit

signal.signal(signal.SIGINT, sigint_handler)


#
# Quit if there's no serial device connected.
#
"""
if not list_ports.comports():
    print("No serial devices found.")
    exit_program()
"""

#
# Show all serial ports found on this computer (that the serial library can use).
#
print("Serial devices: \n")
iternum = 1
for device in list_ports.comports():
    print(str(iternum) + ") " + device.device + "\n")
    iternum += 1

#
# Ask user to verify that date, time, and timezone are correct.
# This is important as data logger users often realize log times
# are off, especially if they've travelled to another country
# to conduct their research or the laptop's time was off and
# they didn't think it was important until after they looked
# at their logger data.
#
#timenow = time.strftime("%Y%m%d%H%M%S%Z", time.localtime())
while (1):
    timeok = input("Is it %s?\n(Yes/No/Repeat): " % time.strftime("%A %B %d, %Y %I:%M:%S%p", time.localtime()) )
    if not timeok:
        continue
    if timeok[0].lower() == 'y':
        break
    elif timeok[0].lower() == 'n':
        print("Please fix your computer's time and try again.")
        exit_program()

while (1):
    zoneok = input("Is your time zone %s?\n(Yes/No) " % time.strftime("%Z", time.localtime()))
    if not zoneok:
        continue
    if zoneok[0].lower() == 'y':
        break
    elif zoneok[0].lower() == 'n':
        print("Please fix your computer's time zone and try again.")
        exit_program()

actions_help = {    'I': 'Read and show device information',
                    'B': 'Begin logging immediately',
                    'F': 'Begin logging at a future time',
                    'D': 'Download data from logger',
                    'E': 'End logging',
                    'C': 'Clear logger (only possible after data downloaded)',
                    'T': 'End logging, download data, and clear logger' }
allinitials = ""
for key in actions_help.keys():
    allinitials += key

#
# Show available actions to user
#
print("Available actions:")
for key, value in actions_help.items():
    print("[" + key + "] " + value)
print("All actions will be repeated for all loggers connected.\nPress Ctrl+c to exit program.")

#
# User selects action
#
while(1):
    desired_action = input("Please select desired action [I/B/F/D/E/C]: ")
    if desired_action:
        desired_action = desired_action[0].upper()
        if desired_action in allinitials:
            break


print("You have chosen to " + actions_help[desired_action].lower() + ".")
exit_program()

# Assuming there's only one serial device attached to /dev/ttyUSB0. 1Mbits with 2 stopbits as set on WSTL18.
ser = serial.Serial('/dev/ttyUSB0', 1000000, stopbits=2, timeout=None)
#ser.flush()
#the_file = open('somefile.txt', 'a')
#the_counter = 1000000
# List the device used.
print("Opened " + ser.name)


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
