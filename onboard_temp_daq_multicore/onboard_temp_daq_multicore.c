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
#include "pico/multicore.h"

#include "pico/util/datetime.h"
#include "pico/util/queue.h"

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'
#define FLAG_VALUE 123
#define ADC_QUEUE_SIZE 1

#define STOP_ADC_READ_IF_QUEUE_FULL true
#define PICO_ADC_READ_SLEEP_US 3800

typedef struct
{
    uint64_t timestamp;
    uint16_t adc;
} adc_sample_t;

queue_t adc_queue;

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */

float convert_adc_to_temperature(uint16_t adc_value, const char unit) {
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_value * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
    
    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

float read_onboard_temperature(const char unit) {
    return convert_adc_to_temperature(adc_read(), unit);
}

// function to run core 1
void core1_temperature_read() {

    // we send core 0 the flag value back
    multicore_fifo_push_blocking(FLAG_VALUE);

    // we wait to receive flag value from core 0
    uint32_t g = multicore_fifo_pop_blocking();

    if (g != FLAG_VALUE)
    {
        printf("Hmm, that's not right on core 1. Abort!\n");
        return;
    }

    uint64_t samples_sent=0;
    uint64_t ticks_start=time_us_64();
    while (true)
    {
        adc_sample_t sample;
        sample.adc = adc_read();
        sample.timestamp = time_us_64();

        bool push_success = queue_try_add(&adc_queue, &sample);
        if (push_success)
        {
            ++samples_sent;
        }
        else
        {
            if (STOP_ADC_READ_IF_QUEUE_FULL)
            {
                uint64_t ticks_end=time_us_64();
                uint64_t end_start_diff = ticks_end - ticks_start;
                printf("queue full after %llu samples sent, and %llu ticks!\n", samples_sent, end_start_diff);
                break;
            }
            else
            {
                continue;
            }
        }
        // equalise (approximately) sampling and send-out rates
        sleep_us(PICO_ADC_READ_SLEEP_US);
    }
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


    // set up queue
    queue_init(&adc_queue, sizeof(adc_sample_t), ADC_QUEUE_SIZE);

    // do not start the core handshake until the user enters 'enter'
    while (true)
    {
        char c = getchar_timeout_us(0);        
        if(c == 13)
        {
            printf("Hello, multicore!\n");
            break;
        }
    }

    // launch core 1 with the method core1_temperature_read(), i.e. core 1 will execute core1_temperature_read()
    multicore_launch_core1(core1_temperature_read);

    // start the handshake process, wait for core 1 to send the flag value
    uint32_t g = multicore_fifo_pop_blocking();

    // check the data that core 1 sent
    if (g != FLAG_VALUE)
    {
        // we did not receive the flag value we expected, exit
        printf("Hmm, that's not right on core 0. Abort!\n");
        return -1;
    }

    // send core 1 the flag value back
    multicore_fifo_push_blocking(FLAG_VALUE);

    while (true) {

        // receive temp adc
        adc_sample_t sample;
        queue_remove_blocking(&adc_queue, &sample);

        // convert temperature to a float
        float temperature = convert_adc_to_temperature(sample.adc, TEMPERATURE_UNITS);

        // send the temperature data along with its timestamp
        printf("Onboard temperature @ %llu = %.02f %c\n", sample.timestamp, temperature, TEMPERATURE_UNITS);
    }
}