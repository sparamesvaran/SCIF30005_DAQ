#!/usr/bin/python3

import serial
import struct
import numpy
import h5py
import sys

def convert_adc_to_temperature(adc_value):
    # 12-bit conversion, assume max value == ADC_VREF == 3.3 V
    conversionFactor = 3.3 / (1 << 12)

    adc = adc_value * conversionFactor
    tempC = 27.0 - (adc - 0.706) / 0.001721
    return tempC


def temperature_readout(target):

        device_name='/dev/tty.usbmodem1101'
        serial_device = serial.Serial(device_name)  # open serial port

        n=0

        # csv
        f_csv = open(f"temp_data.csv", "w")

        serial_device.write(b'\r')

        line = serial_device.readline()
        text = line.decode()
        print(text)

        while n<target:
                ts_bytes = serial_device.read(8)
                adc_bytes = serial_device.read(2)

                # extract data
                timestamp = int.from_bytes(ts_bytes, "little")
                adc = int.from_bytes(adc_bytes, "little")
                temperature = convert_adc_to_temperature(adc)

                print(f"{timestamp},{adc},{temperature}")
                # csv
                #f_csv.write(f"{timestamp},{adc},{temperature}")

                n+=1

        lines=0
        while(lines<3):
                line = serial_device.readline()
                text = line.decode()
                print(text)

        f_csv.close()

if __name__ == '__main__':

        temperature_readout(500)