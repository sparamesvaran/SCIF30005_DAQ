#!/usr/bin/python3

import serial
import sys
import matplotlib.pyplot as plt

def temperature_readout(target):

        device_name='/dev/cu.usbmodem1201'
        serial_device = serial.Serial(device_name)  # open serial port

        f_csv_data = open(f"temp_data_{target}_entries.csv", "w")
        f_csv_benchmark = open(f"benckmark_data_{target}_entries.csv", "w")

        n=0
        last_sample_received=None

        ticks_to_send=[]
        while n<target:
                line = serial_device.readline()
                text = line.decode()
                
                # extract data
                words = text.split()

                first_word=words[0]

                if (first_word == "Onboard"):
                        #temp data
                        sample_counter=int(words[3])
                        timestamp = int(words[5])
                        temperature = float(words[7])

                        print(f"sample: {sample_counter}, last: {last_sample_received} ")

                        if last_sample_received != None:
                                if last_sample_received == sample_counter-1:
                                        print("sequence ok\n")
                                        # send back ok
                                        serial_device.write(b'\r')
                                else:
                                        print("sequence not ok!\n")
                                        # sequence not ok, send back fail flag
                                        serial_device.write(b'f')
                                        return
                        else:
                                print("sequence start\n")
                                # first data, send back ok
                                serial_device.write(b'\r')
                        last_sample_received=sample_counter

                        f_csv_data.write(f"{sample_counter},{timestamp},{temperature}\n")
                        n=n+1
                elif (first_word == "ticks"):
                        #benchmark data
                        ticks=int(words[11])
                        ticks_to_send.append(ticks)
                        f_csv_benchmark.write(f"{ticks}\n")

        f_csv_data.close()
        f_csv_benchmark.close()

if __name__ == '__main__':
        target = int(sys.argv[1])
        temperature_readout(target)