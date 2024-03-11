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


def temperature_readout(mode):

        device_name='/dev/tty.usbmodem11101'
        serial_device = serial.Serial(device_name)  # open serial port

        n=0

        # csv
        f_csv = open(f"temp_data_{mode}.csv", "w")

        serial_device.write(b'\r')

        line = serial_device.readline()
        text = line.decode()
        words=text.split()
        target=int(words[5])
        print(f"target is {target}")

        if mode == 'packed':
                pack_size=int(words[9])        
                print(f"pack size: {pack_size}")
                debug=int(words[11])
                print(f"debug: {debug}")

        first_ts=0
        first_adc=0

        last_ts=0
        last_adc=0
        while n<target:
                if mode == 'packed':
                        if n==0:
                                ts_bytes = serial_device.read(8)
                                adc_bytes = serial_device.read(2)

                                # extract data
                                timestamp = int.from_bytes(ts_bytes, "little")
                                adc = int.from_bytes(adc_bytes, "little")

                                first_ts=timestamp
                                first_adc=adc
                        else:
                                if pack_size == 2:

                                        ts_diff_data_bytes=serial_device.read(1)
                                        adc_diff_data_bytes=serial_device.read(1)
                                
                                        timestamp_diff=int.from_bytes(ts_diff_data_bytes, "little")
                                        adc_diff=int.from_bytes(adc_diff_data_bytes, "little", signed="True")
                                elif pack_size == 1:
                                        data_bytes=serial_device.read(1)
                                        byte = int.from_bytes(data_bytes, "little")
                                        timestamp_diff=byte&0x7
                                        adc_diff=(byte>>3)&0xf
                                        adc_sign=(byte>>7)
                                        if adc_sign==0:
                                                adc_diff=-adc_diff

                                timestamp=last_ts+timestamp_diff
                                adc=last_adc+adc_diff
                                
                                if debug:
                                        diff_line = serial_device.readline()
                                        diff_text = diff_line.decode()
                                        print(f"calc:{timestamp_diff},{adc_diff}")
                                        print(diff_text)

                        print(f"{n}:{timestamp},{adc}\n")
                        last_ts=timestamp
                        last_adc=adc
                elif mode == 'binary':        
                        ts_bytes = serial_device.read(8)
                        adc_bytes = serial_device.read(2)

                        # extract data
                        timestamp = int.from_bytes(ts_bytes, "little")
                        adc = int.from_bytes(adc_bytes, "little")
                
                temperature = convert_adc_to_temperature(adc)
                #print(f"{timestamp},{adc},{temperature}")
                # csv
                f_csv.write(f"{timestamp},{adc},{temperature}")

                n+=1

        lines=0
        while(lines<4):
                benchmark_line = serial_device.readline()
                benchmark_text = benchmark_line.decode()
                print(benchmark_text)
                lines+=1

        f_csv.close()

if __name__ == '__main__':
        mode = sys.argv[1]
        temperature_readout(mode)