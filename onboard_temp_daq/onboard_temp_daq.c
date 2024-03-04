/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'
#define MAX_SEND_ATTEMPTS 5

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float read_onboard_temperature(const char unit) {
    
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

int main() {

    stdio_init_all();
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    /* Initialize hardware AD converter, enable onboard temperature sensor and
     *   select its channel (do this once for efficiency, but beware that this
     *   is a global operation). */
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    uint64_t sample_sequence_counter=0;

    while (true) {

        uint32_t send_attempt=0;
        bool sent_ok=false;

        // get the value of the Pico hardware timer before the ADC read operation
        uint64_t ticks_before_read = time_us_64();

        // read the temperature from the ADC, and convert to a float
        float temperature = read_onboard_temperature(TEMPERATURE_UNITS);

        // get the time at which the temperature data was obtained
        uint64_t sample_timestamp = time_us_64();

        // do not move on until we get acknowledgement that the data has been received
        while (!sent_ok)
        {
            if (send_attempt < MAX_SEND_ATTEMPTS)
            {
                // send the temperature data along with its timestamp and a sequence counter
                printf("Onboard temperature # %llu @ %llu = %.02f %c\n", sample_sequence_counter, sample_timestamp, temperature, TEMPERATURE_UNITS);

                // wait (0.01s) for acknowledgement from receive end
                char return_c = getchar_timeout_us(100000);
                if (return_c == 13)
                {
                    sent_ok=true;
                }
                else
                {
                    // data was not received ok, send it again
                    send_attempt++;
                    continue;
                }
            }
            else
            {
                printf("Send attempts max reached, giving up!\n");
                break;
            }
        }

        // get the value of the Pico hardware timer after the data send operation
        uint64_t ticks_after_send = time_us_64();

        // calculate time taken to perform reading and sending to data, and send information to screen
        uint64_t ticks_to_read = sample_timestamp - ticks_before_read;
        uint64_t ticks_to_send = ticks_after_send - sample_timestamp;
        printf("ticks taken to read data: %llu, ticks taken to send data: %llu\n", ticks_to_read, ticks_to_send);

        // flash the LED to indicate that the Pico is running
        //gpio_put(PICO_DEFAULT_LED_PIN, 1);
        //sleep_ms(10);
        //gpio_put(PICO_DEFAULT_LED_PIN, 0);
        
        // artificially slow down the temperature reading and sending to a rate of ~0.5 Hz
        //sleep_ms(1990);

        sample_sequence_counter++;
    }
}