# `SCIF30005 Pico DAQ`


This repository includes a project showing an example of how to use RaspberryPi Pico functionality in a DAQ context.

## Build instructions

Compalation of code will be done using BlueCrystal login nodes. To build the project for the first time, log into BlueCrystal, and use the copy-and-paste instructions below.

```
# 1. Load software packages needed to compile the Pico uController code.
module add GCC/7.2.0-2.29 tools/cmake/3.20.0 git-2.38.1-gcc-11.3.0-oz2 tools/gcc-arm-none-eabi/10.3

# 2. Create, and move into, a project directory for Pico DAQ code.
mkdir pico_daq
cd pico_daq

# 3. Get a copy of the Pico SDK.
git clone https://github.com/raspberrypi/pico-sdk.git --branch master
cd pico-sdk
git submodule update --init

# 4. Configure the path of the Pico SDK.
PICO_SDK_PATH=$( readlink -f $(pwd)/ ); export PICO_SDK_PATH=${PICO_SDK_PATH};
cd ..

# 5. Get a copy of the Pico DAQ repository, and set up a build directory.
git clone https://github.com/sparamesvaran/SCIF30005_DAQ.git
cd SCIF30005_DAQ
mkdir build
cd build

# 6. Configure the Pico DAQ cmake project, and build it.
cmake ..
make -j 4
```