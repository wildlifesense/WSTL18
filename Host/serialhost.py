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
connected_devices = list()                  # List containing mcu ids of connected devices. Used to check re-connect within session.

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

logging_interval_eightseconds = {   1:15,                   # 2 minutes
                                    2:75,                   # 10 minutes
                                    3:225,                  # 30 minutes
                                    4:450,                  #  1 hour
                                    5:1350,                 #  3 hours
                                    6:2700,                 #  6 hours
                                    7:5400,                 # 12 hours
                                    8:10800 }               # 24 hours

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
    print("\n\nGoodbye and thanks for all the fish!")
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
        print("This can be up to six days and one hour from now.")
        ltime = time.localtime()
        datetime_min = datetime.datetime.now()
        datetime_max = datetime_min + datetime.timedelta(days=6, hours=1)

        # Get the year
        if datetime_min.year == datetime_max.year:
            f_year = int(datetime_min.year)
        else:
            f_year = input("Year? (%d or %d) " % (datetime_min.year, datetime_max.year))
            if f_year.isdigit():
                f_year = int(f_year)
            else:
                continue

        # Get the month
        if datetime_min.month == datetime_max.month:
            f_mon = datetime_min.month
        else:
            f_mon = input("%s or %s? (type %d or %d) " % (month_name(datetime_min.month),
                                                            month_name(datetime_max.month),
                                                            datetime_min.month,
                                                            datetime_max.month))
            if f_mon.isdigit():
                f_mon = int(f_mon)
            else:
                continue

        # Get the day of month
        if f_mon == datetime_min.month:
            minday = datetime_min.day

            maxday = (datetime.date(f_year, f_mon+1, 1) - datetime.timedelta(days=1)).day
        else:
            minday = 1
            maxday = datetime_max.day
        f_mday = input("Day of month? (%d to %d) " % (minday, maxday))
        if f_mday.isdigit():
            f_mday = int(f_mday)
            if f_mday < minday or f_mday > maxday:
                print("\nLet's try again")
                continue
        else:
            continue
        
        # Get the hour (24h format)
        if f_mday == datetime_min.day and f_mon == datetime_min.month:
            minhour = datetime_min.hour
        else:
            minhour = 0

        if f_mday == datetime_max.day and f_mon == datetime_max.month:
            maxhour = datetime_max.hour
        else:
            maxhour = 23

        f_hour =    input("Hour? (%d to %d) " % (minhour, maxhour))
        if f_hour.isdigit():
            f_hour = int(f_hour)
        else:
            continue

        # Get the minute
        if f_year == datetime_min.year and f_mon == datetime_min.month and f_mday == datetime_min.day and f_hour == datetime_min.hour:
            minmin = datetime_min.minute
            maxmin = 59
        elif f_year == datetime_max.year and f_mon == datetime_max.month and f_mday == datetime_max.day and f_hour == datetime_max.hour:
            minmin = 0
            maxmin = datetime_max.minute
        else:
            minmin = 0
            maxmin = 59

        f_minute =  input("Minute? (%d to %d) " % (minmin, maxmin))
        if f_minute.isdigit():
            f_minute = int(f_minute)
        else:
            continue
        return(datetime.datetime(f_year, f_mon, f_mday, f_hour, f_minute))
#################################################### Work below here ##########################

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
                print("You can set an arbitrary interval as an integer multiplier of 8 second counts.")
                print("For example, type 900 for a 2 hour interval (900 * 8 seconds = 7200 seconds = 120 minutes = 2 hours).")
                print("Plese note that intervals under 10 minutes will lead to reduced battery lifetime.")
                interval = input("8-second multiplier (1 to 65535): ")
                if interval.isdigit():
                    interval = int(interval)
                    if 1 <= interval <= 65535:
                        return(interval)
                continue


#
# Running program
#
if __name__ == "__main__":
    signal.signal(signal.SIGINT, sigint_handler)        # Set handler for ctrl+c.
    version = "0.1"
    print("Wildlife Sense NestProbe TL1 Manager, version %s" % version)
    print("This is free software under the terms of GPL v3.0. See COPYING for details.")
    print("\nPress Ctrl+c to exit program.")

    #
    # Quit if there's no serial device connected.
    #
    if not list_ports.comports():
        print("No serial devices found.")
        exit_program()

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
        #print("UTC:        %s" %time.strftime("%A %B %d %Y, %H:%M:%S", time.gmtime()) )
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

	#
	# Show all serial ports found on this computer (that the serial library can use).
	#
	#
	# Prepare list of available actions
	#
    actions_help = [    'I: Read and show device information',
                        'D: Download data from logger',

                        'B: Begin logging immediately',
                        'F: Begin logging at a future time, up to six days and one hour from now',

                        'E: End logging',
                        'C: Clear logger (only possible after data downloaded)',
                        
                        "A: Add logger(s) to this manager's collection",
                        "R: Remove logger(s) from this manager's collection",
                        'S: Write local serial number to this/these device/s',]
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
            future_local_dt_naive = get_logging_start()
            future_utc_dt = localToUtc(local_timezone, future_local_dt_naive)
            print(future_local_dt_naive.strftime("\nLogging will begin at %H:%M on %B %d, %Y (+/- 8 seconds)."))
        logging_interval = get_logging_interval()

    if desired_action == "D":
        pass
        # Need to ask user if the program (if it has internet access?) should upload data to nestprobe.com.
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


    ###############################################################################################
    #                                                                                             #
    # Ok we've got everything. Let's start connecting to the loggers.                             #
    #                                                                                             #
    ###############################################################################################
    

    print("Please connect and hold cable to a WSTL18 logger.")
    # 1Mbits with 2 stopbits as set on WSTL18.
    ser = serial.Serial(device_selected, 1000000, stopbits=2)
    print("Opened " + ser.name)
    while(1):
        ser.timeout = None
        x = ser.read(32) 
        print(x)       # Debugging
        model         = x[0:4].decode('utf-8')
        mcu_serial    = x[4:14].hex().upper()
        local_serial  = x[14:16].hex().upper()
        firmware_version = x[16:17].hex().upper()
        firmware_crc  = x[17:19].hex().upper()
        battery_level = int.from_bytes(x[20:22], byteorder='big')
        logger_flags  = x[

        logger_status = x[19:20].decode('utf-8')
        logger_count = int.from_bytes(x[22:24], byteorder='big')
        #if mcu_serial in connected_devices:
            #print("\nThis device was already connected in this session. Please connect another device to continue or exit.")
            #continue
        #else:
            #connected_devices.append(mcu_serial)
        if logger_status == 'I' or logger_status == 'L' or True:        # WHAT THE HELL! FIΧ THIS CRAP!!!
            if desired_action == 'B':
                datetime_now = datetime.datetime.fromtimestamp(datetime.datetime.utcnow().timestamp()).strftime("%Y%m%d%H%M%S")
                command_string =  desired_action.encode('utf-8') +                      \
                                  datetime_now.encode('utf-8') +                        \
                                  (3000).to_bytes(length=2, byteorder='big') +          \
                                  logging_interval.to_bytes(length=2, byteorder='big')      # 2 bytes, total = 19
                ser.write(command_string)
                print(command_string)
            elif desired_action == 'I':
                print("\nModel: " + model)
                print("Serial number: " + mcu_serial)
                print("Firmware CRC: " + firmware_crc)
                print("Status: " + logger_status)
                print("Count: " + str(logger_count))
                # + " \tLogs: " + str(logger_count) )
            elif desired_action == 'D':
                ser.write('D000000000000000000'.encode('utf-8'))
                ser.timeout = 1.0
                filedata = ser.read(512*1024)
                print("Read %d bytes" % len(filedata))
                #print("Logger is idle, no data to download.")
                datetime_now = datetime.datetime.fromtimestamp(datetime.datetime.utcnow().timestamp()).strftime("%Y%m%d%H%M%S")
                data_filename = mcu_serial + datetime_now + ".TL01Data"
                with open(data_filename, 'wb') as mydatafile:
                    mydatafile.write(filedata)
                    mydatafile.close()
                    print("Dumped data in file %s" % data_filename)

        #elif logger_status == 'L':
            #if desired_action == 'B':
                #print("Logger is logging, cannot be set to start logging again.")
        #if desired_action == 'D':
