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

int parse_char_buffer_chunk_to_int(const char* buffer, size_t chunk_start, size_t chunk_size)
{   
    // we assume the max length of the char buffer chunk to parse here 
    char temp_buffer[1024];
    strncpy(temp_buffer, &buffer[chunk_start], chunk_size);
    temp_buffer[chunk_size] = '\0'; // terminate the string
    return atoi(temp_buffer);
}

int parse_char_buffer_chunk_to_rtc_weekday_int(const char* buffer, size_t chunk_start, size_t chunk_size)
{   
    // we assume the max length of the char buffer chunk to parse here
    char temp_buffer[1024];
    strncpy(temp_buffer, &buffer[chunk_start], chunk_size);
    temp_buffer[chunk_size] = '\0'; // terminate the string
    
    int rtc_weekday=-1;

    if(strcmp(temp_buffer, "Mon") == 0)
    {
        rtc_weekday=1;
    }
    else if(strcmp(temp_buffer, "Tue") == 0)
    {
         rtc_weekday=2;
    }
    else if(strcmp(temp_buffer, "Wed") == 0)
    {
        rtc_weekday=3;
    }
    else if(strcmp(temp_buffer, "Thu") == 0)
    {
        rtc_weekday=4;
    }
    else if(strcmp(temp_buffer, "Fri") == 0)
    {
        rtc_weekday=5;
    }
    else if(strcmp(temp_buffer, "Sat") == 0)
    {
        rtc_weekday=6;
    }
    else if(strcmp(temp_buffer, "Sun") == 0)
    {
        rtc_weekday=0;
    }
    return rtc_weekday;
}

int main() {

    stdio_init_all();
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    
    // variable to hold the time and date to initialise the RTC with
    char initial_datetime_buf[24];
    memset(initial_datetime_buf, 0, sizeof(initial_datetime_buf));

    // counters and necessary for the interface for reading in the current date and time string
    uint16_t received_chars=0;
    bool rtc_init_begun=false;
    bool time_date_confirm_begun=false;

    while (true)
    {
        if (!rtc_init_begun)
        {
            printf("Please hit enter to begin.\n");
        }
        
        // calling with (0) means no timeout, do not wait for a character if none is available
        char c = getchar_timeout_us(0);        
        
        // was a character entered?
        if (c != 255)
	    {
            if (!rtc_init_begun)
            {   
                // Was 'enter" entered?
                if (c == 13)
                {
                    printf("Welcome! Let's initialise the RTC! Please enter the time and date to load into the RTC. The format is: dd-mm-yyyy-Dow-hh-mm-ss. For example: 06-03-2012-Tue-09-21-00.\n");
                    rtc_init_begun=true;
                    received_chars=0;
                }
                else
                {
                    printf("You entered %c, not enter!!!\n", c);
                }
            }
            else
            {   // has the process of confirming the entered string begun?
                if (!time_date_confirm_begun)
                {
                    printf("You entered the character: %c\n", c);
                    initial_datetime_buf[received_chars] = c;
                    received_chars++;
                    // once we receive the expected number of characters, we need to confirm with the user if they are correct
                    if (received_chars == (sizeof(initial_datetime_buf) - 1))
		            {
			            initial_datetime_buf[received_chars] = 0;	//terminate string
			            printf("You wrote: %s. Is that correct? [y/Y]\n", initial_datetime_buf);
                        time_date_confirm_begun=true;
		            }
                }
                else
                {   
                    // was a positive answer entered?
                    if (c == 'y' || c == 'Y' || c == 13)
                    {
                        printf("Great! I will load %s into the RTC.\n", initial_datetime_buf);
                        break;
                    }
                    else
                    {
                        printf("OK... Please enter new time and date!\n");
                        // reset flags and counters, we go back to the begining
                        time_date_confirm_begun=false;
                        received_chars=0;
                    }
                }
            }
        }
    }

    // convert the relevant parts of the string to integers
    int initial_day = parse_char_buffer_chunk_to_int(initial_datetime_buf, 0, 2);
    int initial_month = parse_char_buffer_chunk_to_int(initial_datetime_buf, 3, 2);
    int initial_year = parse_char_buffer_chunk_to_int(initial_datetime_buf, 6, 4);

    int initial_weekday = parse_char_buffer_chunk_to_rtc_weekday_int(initial_datetime_buf, 11, 3);    

    int initial_hour = parse_char_buffer_chunk_to_int(initial_datetime_buf, 15, 2);
    int initial_minute = parse_char_buffer_chunk_to_int(initial_datetime_buf, 18, 2);
    int initial_second = parse_char_buffer_chunk_to_int(initial_datetime_buf, 21, 2);

    datetime_t t = {
            .year  = initial_year,
            .month = initial_month,
            .day   = initial_day,
            .dotw  = initial_weekday,
            .hour  = initial_hour,
            .min   = initial_minute,
            .sec   = initial_second
    };
    
    rtc_init();

    // set rtc time to the previously entered string, this will allow us to correlate temperature data to real world time
    rtc_set_datetime(&t);

    printf("Loaded time and date into rtc!\n");

    // clk_sys is >2000x faster than clk_rtc, so datetime is not updated immediately when rtc_get_datetime() is called.
    // tbe delay is up to 3 RTC clock cycles (which is 64us with the default clock settings)
    sleep_us(64);

    /* Initialize hardware AD converter, enable onboard temperature sensor and
     *   select its channel (do this once for efficiency, but beware that this
     *   is a global operation). */
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
        
    while (true) {
        // variable to hold the time and date of the ADC value read operation
        datetime_t adc_read_t;

        // variables to hold the string of the time of date of when the ADC value was obtained
        char adc_read_str_buf[256];
        char *adc_read_str = &adc_read_str_buf[0];
    
        // read the temperature from the ADC, and convert to a float
        float temperature = read_onboard_temperature(TEMPERATURE_UNITS);

        // get the time at which the temperature data was obtained
        rtc_get_datetime(&adc_read_t);
        
        // convert the date and time data to a string
        datetime_to_str(adc_read_str, sizeof(adc_read_str_buf), &adc_read_t);
                
        // send the temperature data along with its timestamp
        printf("Onboard temperature @ %s = %.02f %c\n", adc_read_str_buf, temperature, TEMPERATURE_UNITS);
        
        // flash the LED to indicate that the Pico is running
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(10);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        
        // artificially slow down the temperature reading and sending to a rate of ~0.5 Hz
        sleep_ms(1990);        
    }
}