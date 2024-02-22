#!/usr/bin/python3

import sys
import matplotlib.pyplot as plt
import os

def make_file_sizes_plot(n_events_range):
        n_events=[]
        csv_sizes=[]
        binary_sizes=[]
        hdf5_sizes=[]
        
        for n in n_events_range:
                
                n_events.append(n)

                csv_file=f"temp_data_{n}_entries.csv"
                csv_size = os.path.getsize(csv_file)
                csv_sizes.append(csv_size)

                binary_file=f"temp_data_{n}_entries.dat"
                binary_size = os.path.getsize(binary_file)
                binary_sizes.append(binary_size)

                hdf5_file=f"temp_data_{n}_entries.hdf5"
                hdf5_size = os.path.getsize(hdf5_file)
                hdf5_sizes.append(hdf5_size)

        plt.scatter(n_events, binary_sizes, color='blue')
        plt.scatter(n_events, csv_sizes, color='green')
        plt.scatter(n_events, hdf5_sizes, color='orange')

        plt.legend(['binary','csv','hdf5'])

        plt.xlabel ('# events')
        plt.ylabel ('file size [B]')

        plt.show()

if __name__ == '__main__':
        start = int(sys.argv[1])
        stop = int(sys.argv[2])
        step = int(sys.argv[3])

        n_events_range=range(start,stop,step)
        make_file_sizes_plot(n_events_range)