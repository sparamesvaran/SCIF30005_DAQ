/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/stdio_usb.h"
#include "hardware/adc.h"
#include "pico/multicore.h"

#include "pico/util/datetime.h"
#include "pico/util/queue.h"

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'
#define FLAG_VALUE 123

#define CHUNK_FACTOR 512
#define BUFFER_SIZE 4096
#define BUFFER_INDEX_QUEUE_SIZE 1000

typedef struct
{
    uint64_t timestamp;
    uint16_t adc;
    uint64_t sequence_id;
} adc_sample_t;

queue_t buffer_index_queue;

adc_sample_t samples_buffer[BUFFER_SIZE];
uint64_t events_to_send=5000000;

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
    uint32_t sample_buffer_index=0;

    uint64_t ticks_sample_start=time_us_64();

    while(true)
    //while (samples_sent<events_to_send)
    {
        adc_sample_t sample;
        sample.adc = adc_read();
        sample.timestamp = time_us_64();
        sample.sequence_id = samples_sent;

        samples_buffer[sample_buffer_index]=sample;
        sample_buffer_index++;

        if (sample_buffer_index%CHUNK_FACTOR==0)
        {
            bool index_push_ok = queue_try_add(&buffer_index_queue, &sample_buffer_index);
            if (!index_push_ok)
            {
                break;
            }
        }

        if (sample_buffer_index==BUFFER_SIZE)
        {
            sample_buffer_index=0;
        }

        samples_sent++;
    }
    uint64_t ticks_sample_end=time_us_64();
    double total_sample_time=ticks_sample_end-ticks_sample_start;

    double average_sample_time=total_sample_time/samples_sent;
    printf("ave. sample time: %.2f\n",average_sample_time);
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
    queue_init(&buffer_index_queue, sizeof(uint16_t), BUFFER_INDEX_QUEUE_SIZE);

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

    uint64_t total_receive_time=0;
    uint64_t total_send_time=0;
    uint64_t samples_processed=0;

    int16_t last_adc=-4096;
    uint64_t last_timestamp=0;
    uint64_t last_sequence_id;

    bool run_processing=true;

    uint8_t send_buffer[CHUNK_FACTOR];
    uint32_t buffer_entries=0;

    uint64_t ticks_before_process = time_us_64();

    while (run_processing && samples_processed<events_to_send)
    {
        uint64_t ticks_before_receive = time_us_64();

        uint16_t end_index;
        queue_remove_blocking(&buffer_index_queue, &end_index);

        uint32_t start_index = end_index - CHUNK_FACTOR;

        uint64_t ticks_before_send = time_us_64();

        for (uint32_t i=start_index; i<end_index; ++i)
        {
            //uint64_t ticks_before_receive = time_us_64();
            adc_sample_t sample = samples_buffer[i];
            uint64_t current_sequence_id=sample.sequence_id;

            if (samples_processed)
            {
                if (last_sequence_id+1 != current_sequence_id)
                {
                    printf("data sequence error! %llu %llu abort\n", last_sequence_id, current_sequence_id);
                    run_processing=false;
                    break;
                }
            }

            last_sequence_id=current_sequence_id;
            ++samples_processed;

                if (last_timestamp == 0 )
                {
                    fwrite(&sample.timestamp, sizeof(sample.timestamp), 1, stdout);
                    fwrite(&sample.adc, sizeof(sample.adc), 1, stdout);
                }
                else
                {
                    uint8_t timestamp_diff = sample.timestamp - last_timestamp;
                    int8_t adc_diff = sample.adc-last_adc;
                    uint8_t sign = (adc_diff>0);
                    uint8_t pack=
                     ((sign&0x1) << 7) | (((uint8_t)adc_diff&0xf) << 3) | (timestamp_diff&0x7);

                    send_buffer[buffer_entries]=pack;
                    buffer_entries++;
                    if (buffer_entries == CHUNK_FACTOR)
                    {
                        fwrite(&send_buffer, sizeof(send_buffer[0]), buffer_entries, stdout);
                        fflush(stdout);
                        buffer_entries=0;
                    }
                }

            last_adc = sample.adc;
            last_timestamp=sample.timestamp;
        }
        uint64_t ticks_after_send = time_us_64();

        // calculate time taken to perform tasks
        uint64_t ticks_to_receive = ticks_before_send - ticks_before_receive;
        uint64_t ticks_to_send = ticks_after_send - ticks_before_send;

        total_receive_time=total_receive_time+ticks_to_receive;
        total_send_time=total_send_time+ticks_to_send;
    }

    uint64_t ticks_after_process = time_us_64();
    uint64_t ticks_to_process = ticks_after_process-ticks_before_process;

    double average_receive_time=(double)total_receive_time/samples_processed;
    double average_send_time=(double)total_send_time/samples_processed;
    double average_process_time=(double)ticks_to_process/samples_processed;

    sleep_ms(1000);

    printf("\nave. recv time: %.2f\n",average_receive_time);
    printf("ave. send time: %.2f\n",average_send_time);
    printf("ave. process time: %.2f\n",average_process_time);
}