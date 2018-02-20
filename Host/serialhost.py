#!/usr/bin/python3
#
# This software is open source bla bla bla (I'll refine this later)
# 
# Nice example for pyserial logging into a file:
# https://github.com/gskielian/Arduino-DataLogging/blob/master/PySerial/README.md
#
import time
import pytz
import re
import serial
import signal
import datetime
import calendar
import pytz
import ntplib
import intelhex                         # (install with) sudo pip3 install intelhex
from serial.tools import list_ports

# ih = intelhex.IntelHex("Readout.hex")
# ih[0]    # ... is first byte, given as an integer. Must convert to b'' before writing via serial.

dt_utc = datetime.datetime.fromtimestamp(datetime.datetime.utcnow().timestamp())
dt_local =  datetime.datetime.fromtimestamp(datetime.datetime.now().timestamp())
local_timezone = time.tzname[0]
local_dst = time.localtime().tm_isdst
local_pytz = pytz.timezone(local_timezone)

# Use NTP to calculate computer clock offset to UTC.
c = ntplib.NTPClient()
try:
    ntp_object = c.request('europe.pool.ntp.org', version=4)
except:
    time_offset = 0.0
    time_method = "L"                       # 'L' stands for local.
else:
    time_offset = ntp_object.offset
    time_method = "N"                       # 'N' stands for network.
del(c, ntp_object)                          # Don't need these any more.

logging_interval_eightseconds = {   1:75,                   # 10 minutes
                                    2:225,                  # 30 minutes
                                    3:450,                  #  1 hour
                                    4:1350,                 #  3 hours
                                    5:2700,                 #  6 hours
                                    6:5400,                 # 12 hours
                                    7:10800 }               # 24 hours

def interval_text(eightseconds):
    intext = ""
    seconds = eightseconds*8%60
    minutes = int(eightseconds*8/60) % 60
    hours = int(eightseconds*8/60/60)
    if(hours):
        intext = intext + "%d hour%s" % (hours, "s" if hours>1 else "")
    if(hours and minutes):
        intext += " "
    if(minutes):
        intext = intext + "%d minute%s" % (minutes, "s" if minutes>1 else "")
    if(minutes and seconds):
        intext += " "
    if(seconds):
        intext = intext + "%d second%s" % (seconds, "s" if seconds>1 else "")
    return(intext)

def gettime():
    c = ntplib.NTPClient()
    try:
        ltime = c.request('europe.pool.ntp.org', version=4).tx_time
    except ntplib.NTPException:
        return(0)
    return(ltime)




# Set up a handler for Ctrl+c
def sigint_handler(sig_number, frame):
    print("\n\nI hope this wasn't messy.")
    quit()

def exit_program(message="Bye bye!"):
    print(message)
    raise SystemExit

def month_name(month_number):
    if month_number >= 1 and month_number <= 12:
        return( datetime.datetime(1, month_number, 1).strftime("%B") )
    else:
        return(None)

# is_dst: Takes an abbreviated or extended time zone name (see pytz.common_timezones and pytz.all_timezones)
# and a timezone-unaware datetime, and returns whether 
# ????? Is the naive_datetime here assumed to be UTC????????? Should check this!!!!!
def is_dst(zonename, naive_datetime):
    local_pytz = pytz.timezone(zonename)
    future_datetime = pytz.utc.localize(naive_datetime)
    return future_datetime.astimezone(local_pytz).dst() != datetime.timedelta(0)

# https://stackoverflow.com/questions/79797/how-do-i-convert-local-time-to-utc-in-python/79877#79877
def localToUtc(zonename, naive_datetime):
    local_pytz = pytz.timezone(zonename)
    local_dt = local_pytz.localize(naive_datetime)
    return(local_dt.astimezone(pytz.utc))



# get_logging_start: As the user a series of questions about when they want to start logging in local time.
# Returns a UTC datetime to be sent to the logger.
def get_logging_start():
    while(1):
        print("\nPlease enter local date and time you would like logging to begin at.")
        print("This can be up to six days from now.")
        ltime = time.localtime()

        # Get the year
        if ltime.tm_mon == 12 and ltime.tm_day > 24:
            f_year = input("Year? (%d or %d) " % (ltime.tm_year, ltime.tm_year+1))
            if f_year.isdigit():
                f_year = int(f_year)
            else:
                continue
        else:
            f_year = ltime.tm_year

        # Get the month
        f_mon =   input("%s or %s? (type %d or %d) " % (    month_name(ltime.tm_mon),
                                                            month_name(ltime.tm_mon+1),
                                                            ltime.tm_mon,
                                                            ltime.tm_mon + 1))
        if f_mon.isdigit():
            f_mon = int(f_mon)
        else:
            continue

        # Get the day of month
        if f_mon == ltime.tm_mon:
            minday = ltime.tm_mday

            maxday = int((datetime.date(f_year, f_mon+1, 1) - datetime.timedelta(days=1)).day)
        else:
            minday = 1
            maxday = ltime.tm_mday
            if f_mon == 2 and maxday > 28:                  # Make sure we don't offer February 
                if calendar.isleap(f_year):
                    maxday = 29
                else:
                    maxday = 28
            elif maxday == 31:                # Make sure we don't offer June/September/November 31
                maxday = int((datetime.date(f_year, f_mon+2, 1) - datetime.timedelta(days=1)).day)

        f_mday =    int(input("Day of month? (%d to %d) " % (minday, maxday)))
        if f_mday < minday or f_mday > maxday:
            print("\nLet's try again")
            continue
        
        # Get the hour (24h format)
        if f_mday == ltime.tm_mday and f_mon == ltime.tm_mon:
            minhour = ltime.tm_hour
        else:
            minhour = 0

        f_hour =    input("Hour? (%d to 23) " % minhour)
        if f_hour.isdigit():
            f_hour = int(f_hour)

        # Get the minute
        if f_year == ltime.tm_year and f_mon == ltime.tm_mon and f_mday == ltime.tm_mday and f_hour == ltime.tm_hour:
            minmin = ltime.tm_min
        else:
            minmin = 0
        f_minute =  input("Minute? (%d to 59) " % minmin)
        if f_minute.isdigit():
            f_minute = int(f_minute)
        return(datetime.datetime(f_year, f_mon, f_mday, f_hour, f_minute))

        # Is there a daylight saving time change between now and start of logging? If so, inform user and 
        # ask them if they want to proceed. If not, repeat start datetime setting.

# Get the logging interval in eightseconds from the user
def get_logging_interval():
    firstint = min(logging_interval_eightseconds.keys())
    lastint = max(logging_interval_eightseconds.keys())
    while(1):
        print("\nPlease select the temperature logging interval.")
        for i in range(firstint, lastint+1):
            print("%d:\t%s" % (i, interval_text(logging_interval_eightseconds[i])))
        print("%d:\tOther" % int(lastint+1))

        interval = input("Interval? (%d to %d) " % (firstint, lastint+1))
        if interval.isdigit():
            interval = int(interval)
            if firstint <= interval <= lastint:
                return(logging_interval_eightseconds[interval])
            elif interval == lastint+1:
                print("Ok let's choose")


#
# Running program
#
if __name__ == "__main__":
    signal.signal(signal.SIGINT, sigint_handler)        # Set handler for ctrl+c.
    version = "0.1"
    print("Wildlife Sense , version %s" % version)
    print("This is free software under the terms of GPL v3.0. See COPYING for details.")
    print("\nPress Ctrl+c to exit program.")

    #
    # Quit if there's no serial device connected.
    #
    """
    if not list_ports.comports():
        print("No serial devices found.")
        exit_program()
    """

    #
    # Ask user to verify that date, time, and timezone are correct.
	# This is important as data logger users often realize log times
	# are off, especially if they've travelled to another country
	# to conduct their research or the laptop's time was off and
	# they didn't think it was important until after they looked
	# at their logger data.
	#
	#print("This program and the WSTL18 use UTC time to prevent problems with timezone")
	#print("or daylight saving time changes.")
	# Move the two lines above to the manual.
	#print("")
    # or show if timezone/dst settings are not specified
    while (1):
        #print("UTC:        %s" % time.strftime("%A %B %d %Y, %H:%M:%S", time.gmtime()) )
        print("\nYour time zone is %s. Local time is %d hours %s UTC." % (
                            time.strftime("%Z", time.localtime()),
                            int(-time.timezone/3600),
                            "ahead of" if time.timezone<0 else "behind"))
        print("You are currently %s daylight saving time." % ("in" if time.localtime().tm_isdst else "not in"))
        print("Local date: %s" % time.strftime("%B %0d %Y", time.localtime()) )
        print("Local time: %s" % time.strftime("%H:%M", time.localtime()) )
        timeok = input("\nAre these correct? (Yes/No/Repeat): " )
        if not timeok:
            continue
        if timeok[0].lower() == 'y':
            break
        elif timeok[0].lower() == 'n':
            print("Please fix your computer's time and try again.")
            exit_program()

	#while (1):
	#    zoneok = input("Is your time zone %s?\n(Yes/No) " % time.strftime("%Z", time.localtime()))
	#    if not zoneok:
	#        continue
	#    if zoneok[0].lower() == 'y':
	#        break
	#    elif zoneok[0].lower() == 'n':
	#        print("Please fix your computer's time zone and try again.")
	#        exit_program()
	#
	
	#
	# Show all serial ports found on this computer (that the serial library can use).
	#
	#
	# Prepare list of available actions
	#
    actions_help = [    'I: Read and show device information',
                        'D: Download data from logger',

                        'B: Begin logging immediately',
                        'F: Begin logging at a future time, up to six days away',

                        'E: End logging',
                        'C: Clear logger (only possible after data downloaded)']
                        #'T: End logging, download data, and clear logger']
                        #'U: Update firmware.']
    allinitials = ""
    for item in actions_help:
        allinitials += item[0]


    #
    # Show available actions to user
    #
    print("\nAvailable actions:")
    for item in actions_help:
        print(item)
    print("\nSelected action will be repeated for all loggers connected in this session.")
	
	
	#
	# User selects action
	#
    while(1):
        desired_action = input("Please select desired action [%s]: " % ''.join([i+'/' if i!=allinitials[-1] else i for i in allinitials]))      # Replace list with something from allinitials
        if desired_action:
            desired_action = desired_action[0].upper()
            if desired_action in allinitials:
                break
	
	
    #
    # These actions need more user input
    #
    #
    # I,D,E,C, and T don't need any other input.
    # F needs the time to start logging
    # B and F need a logging interval
    if desired_action in "BF":
        if desired_action == "F":
            future_utc_dt = localToUtc(local_timezone, get_logging_start())
            print(future_utc_dt.strftime("%Y%m%d %H:%M:%S %Z"))
        logging_interval = get_logging_interval()

    if desired_action == "D":
        pass
        # Need to ask user if the program (if it has internet access?) should upload data to ovilog.com.
        # This will only work if the user is registered there?? Or the logger is registered?
	

#"""
    #if desired_action == 'B':
        #command_string = "B%s-" % time.strftime("%Y%m%d%H%M%S%Z", time.localtime())
    #elif desired_action == 'I':
        #command_string = "I-"
    #quit()
#"""
    # Show list of serial devices
    serial_devices = list_ports.comports()
    while not serial_devices:
        input("\nNo serial devices found. Please connect a serial device and press Enter.")
        serial_devices = list_ports.comports()
    if len(serial_devices) > 1:
        iternum = 1
        print("Serial devices: \n")
        for device in serial_devices: 
            print(str(iternum) + ": " + device.device + "\n")
            iternum += 1
        while(1):
            device_selected = input("Please select device to use: ")
            if device_selected.isdigit(): # Also check number is in available range of devices!!!!!! !!!####!!!!
                device_selected = serial_devices[int(device_selected)-1]
    elif len(serial_devices) == 1:
        device_selected = serial_devices[0].device

    #
    # Ok we've got everything. Let's start connecting to the loggers
    #
    # Assuming there's only one serial device attached to /dev/ttyUSB0. 1Mbits with 2 stopbits as set on WSTL18.
    ser = serial.Serial(device_selected, 1000000, stopbits=2, timeout=None)
    #ser.flush()
    #the_file = open('somefile.txt', 'a')
    #the_counter = 1000000
    # List the device used.
    print("Opened " + ser.name)
    
    #exit_program()

    ###############################################################################################
    #                                                                                             #
    ###############################################################################################
    
    print("Please connect and hold cable to a WSTL18 logger.")
    ser = serial.Serial(device_selected, 1000000, stopbits=2, timeout=None)
    while(1):
        #the_counter -= 1
        x = ser.read()                                  # Wait for the 'x' character
        if x == b'x':
            ser.write('HELLO'.encode('utf-8'))
            ser.flush()
            print("Sending..")
            ser.read(size=4)
            print(x)
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
