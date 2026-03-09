#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Segment pins
#define SEG_A 42 
#define SEG_B 40
#define SEG_C 9
#define SEG_D 5
#define SEG_E 4 
#define SEG_F 41 
#define SEG_G 10 
#define SEG_P 6 

// Digit pins
#define DIG1 11
#define DIG2 12
#define DIG3 13
#define DIG4 14

// Segment lookup table
const uint8_t segment_map[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

const uint8_t singularSegment[1] = {
    0b10000000, // decimal point
};

gpio_num_t segment_pins[8] = {
    SEG_A, SEG_B, SEG_C, SEG_D,
    SEG_E, SEG_F, SEG_G, SEG_P
};

gpio_num_t digit_pins[4] = {
    DIG1, DIG2, DIG3, DIG4
};

void gpio_init_all()
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << SEG_A) |
            (1ULL << SEG_B) |
            (1ULL << SEG_C) |
            (1ULL << SEG_D) |
            (1ULL << SEG_E) |
            (1ULL << SEG_F) |
            (1ULL << SEG_G) |
            (1ULL << SEG_P) |
            (1ULL << DIG1) |
            (1ULL << DIG2) |
            (1ULL << DIG3) |
            (1ULL << DIG4)
    };
    gpio_config(&io_conf);
}

void set_segments(uint8_t value)
{
    for (int i = 0; i < 8; i++)
    {
        gpio_set_level(segment_pins[i], (value >> i) & 0x01);
    }
}

void disable_all_digits()
{
    for (int i = 0; i < 4; i++)
        gpio_set_level(digit_pins[i], 1);
}

void enable_digit(int digit)
{
    gpio_set_level(digit_pins[digit], 0);
}

void display_time(int min, int sec) {
    int digitsOnDisplay[4];

    digitsOnDisplay[0] = min / 10;
    digitsOnDisplay[1] = min % 10;
    digitsOnDisplay[2] = sec / 10;
    digitsOnDisplay[3] = sec % 10;

    for (int i = 0; i < 4; i++)
    {
        disable_all_digits();
        uint8_t numToDsiplay = segment_map[digitsOnDisplay[i]];

        // add decimal point after minutes
        if (i == 1) {
            numToDsiplay |= 0b10000000;
        }
        set_segments(numToDsiplay);
        enable_digit(i);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

}

void time_display_1s(int min, int sec)
{
    TickType_t start = xTaskGetTickCount();

    while (xTaskGetTickCount() - start < pdMS_TO_TICKS(1000))
    {
        display_time(min,sec);
        // vTaskDelay(1);
    }
}

void app_main(void)
{
    gpio_init_all();
    disable_all_digits();

    while (1) { //needs to run continiously! do not put any delays here or it will break
        for (int min=89; min>=0; min--) {
            for (int sec=59; sec>=0; sec--) {
                time_display_1s(min,sec);
            }
        }
    }
}