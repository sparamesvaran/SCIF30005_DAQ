#!/usr/bin/python3

import serial
import struct
import numpy
import h5py
import sys

def temperature_readout(target):

        device_name='/dev/cu.usbmodem11201'
        serial_device = serial.Serial(device_name)  # open serial port

        n=0

        # csv
        f_csv = open(f"temp_data_{target}_entries.csv", "w")
        
        # binary
        f_binary = open(f"temp_data_{target}_entries.dat", 'wb')

        # HDF5
        f_hdf5 = h5py.File(f"temp_data_{target}_entries.hdf5", 'w')
        f_hdf5.create_dataset("timestamp", (0,), dtype=numpy.uint64, maxshape=(None,), compression='gzip',)
        f_hdf5.create_dataset("temperature", (0,), maxshape=(None,), compression='gzip',)

        while n<target:
                line = serial_device.readline()
                text = line.decode()
                
                # extract data
                words = text.split()
                timestamp = words[3]
                temperature = words[5]

                # csv
                f_csv.write(f"{timestamp},{temperature}")

                # binary
                f_binary.write(struct.pack('<Q', int(timestamp)))
                f_binary.write(struct.pack('<f', float(temperature)))

                # HDF5
                f_hdf5["timestamp"].resize((f_hdf5["timestamp"].shape[0] + 1), axis = 0)
                f_hdf5["timestamp"][-1:] = [int(timestamp)]

                f_hdf5["temperature"].resize((f_hdf5["temperature"].shape[0] + 1), axis = 0)
                f_hdf5["temperature"][-1:] = [float(temperature)]

                n+=1

        f_csv.close()
        f_binary.close()
        f_hdf5.close()

if __name__ == '__main__':
        start = int(sys.argv[1])
        stop = int(sys.argv[2])
        step = int(sys.argv[3])

        n_events_range=range(start,stop,step)
        for n in n_events_range:
                temperature_readout(n)