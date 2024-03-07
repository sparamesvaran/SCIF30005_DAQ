add_executable(onboard_temp_daq_multicore_buffer
        onboard_temp_daq_multicore_buffer.c
        )

# Add pico_multicore which is required for multicore functionality
target_link_libraries(onboard_temp_daq_multicore_buffer
        pico_stdio
        pico_stdlib
        pico_multicore
        hardware_adc
        hardware_rtc)

pico_enable_stdio_usb(onboard_temp_daq_multicore_buffer 1)
pico_enable_stdio_uart(onboard_temp_daq_multicore_buffer 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(onboard_temp_daq_multicore_buffer)