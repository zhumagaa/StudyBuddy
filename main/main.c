#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// VARIABLES FOR TIMER
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

int mins = 0;
int secs = 0;

// VARIABLES FOR KEYPAD
#define LOOP_DELAY_MS           10      // Loop sampling time (ms)
#define DEBOUNCE_TIME           40      // Debounce time (ms)
#define NROWS                   4       // Number of keypad rows
#define NCOLS                   3       // Number of keypad columns

#define ACTIVE                  0       // Keypad active state (0 = low (pullup), 1 = high (pulldown))
#define NOPRESS                 '\0'    // NOPRESS character

//fsm states
#define WAIT_FOR_PRESS    0
#define DEBOUNCE          1
#define WAIT_FOR_RELEASE  2

int row_pins[] = {GPIO_NUM_3, GPIO_NUM_8, GPIO_NUM_18, GPIO_NUM_17};     // Pin numbers for rows
int col_pins[] = {GPIO_NUM_16, GPIO_NUM_15, GPIO_NUM_7};   // Pin numbers for columns

//VARIABLES FOR STORING NUMS - KEYPAD AND TIMER COMMUNICATION
#define MAX_DIGITS 4

char digit_buffer[MAX_DIGITS];
int digit_index = 0;
bool input_complete = false;

// SEGMENT MAPS FOR TIMER
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

// SEGMENT MAPS FOR KEYPAD
char keypad_array[NROWS][NCOLS] = {   // Keypad layout
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

// INIT FOR TIMER
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

// INIT FOR KEYPAD
void init_keypad(void) {
    // Configure row pins as outputs
    for (int i=0; i<NROWS; i++){
        gpio_reset_pin(row_pins[i]);
        gpio_set_direction(row_pins[i], GPIO_MODE_OUTPUT);
        //rows stay inactive at the beginning
        if (ACTIVE == 0)
        { gpio_set_level(row_pins[i], 1); }   // inactive = high
        else
        { gpio_set_level(row_pins[i], 0); }   // inactive = low
    }
    // Configure col pins as outputs w pullup or pulldown
    for (int i=0; i<NCOLS; i++){
        gpio_reset_pin(col_pins[i]);
        gpio_set_direction(col_pins[i], GPIO_MODE_INPUT);
        if (ACTIVE == 0)
        {
            // when active low, use pullup
            gpio_pullup_en(col_pins[i]);
            gpio_pulldown_dis(col_pins[i]);
        }
        else
        {
            // when active high, use pulldown
            gpio_pulldown_en(col_pins[i]);
            gpio_pullup_dis(col_pins[i]);
        }
    }
}


//METHODS FOR TIMER
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

void timer_task(void *arg) {
    while (1) { //needs to run continiously! do not put any delays here or it will break
        if (input_complete)
        {
            mins = (digit_buffer[0]-'0')*10 + (digit_buffer[1]-'0');
            secs = (digit_buffer[2]-'0')*10 + (digit_buffer[3]-'0');

            digit_index = 0;
            input_complete = false;

            // printf("Timer set: %02d:%02d\n", mins, secs);
        }

        if (mins > 0 || secs > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (secs==0)
            {
                if (mins>0) {
                    mins--;
                    secs=59;
                }
            } else {
                secs--;
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

//display task
void display_task(void *arg)
{
    while (1)
    {
        display_time(mins, secs);
    }
}

//METHODS FOR KEYPAD
char scan_keypad()
{
    char key = NOPRESS;
    for (int r = 0; r < NROWS; r++)
    {
        //set one row ACTIVE at a time, all others are inactive
        for (int ra = 0; ra < NROWS; ra++)
        {
            if (ra == r)
            { gpio_set_level(row_pins[ra], ACTIVE);}
            else
            { gpio_set_level(row_pins[ra], !ACTIVE);}
        }
        //scan cols & check for any column inputs being ACTIVE
        for (int c = 0; c < NCOLS; c++)
        {
            if (gpio_get_level(col_pins[c]) == ACTIVE) //if key at [r][c] is active
            {
                key = keypad_array[r][c];
                // Restore rows to inactive state before returning
                for (int i = 0; i < NROWS; i++)
                {
                    gpio_set_level(row_pins[i], !ACTIVE);
                }
                return key;
            }
        }
    }
    return key;
}   

void keypad_task(void *arg) {
    int state = WAIT_FOR_PRESS; //state =0
    char new_key = NOPRESS;
    char last_key = NOPRESS;

    int time = 0;
    bool timed_out = false;

    while(1) {
        //fsm inputs update
        new_key = scan_keypad();
        time += LOOP_DELAY_MS;
        timed_out = (time >= DEBOUNCE_TIME);
        
        if (state == WAIT_FOR_PRESS) {
            if (new_key != NOPRESS) {
                last_key = new_key;   // store first detection
                time = 0;             // start debounce timer
                state = DEBOUNCE;
            }
        } else if (state == DEBOUNCE){
            if (timed_out && new_key == last_key) { 
                // key is stable long enough → valid press
                state = WAIT_FOR_RELEASE; 
            }
            else if (timed_out && new_key != last_key) {
                // key changed during debounce → glitch/bounce
                state = WAIT_FOR_PRESS;
            }
        } else if (state == WAIT_FOR_RELEASE) {
            if (new_key == NOPRESS) {
                // key released → output the stored key
                // printf("Key pressed: %c\n", last_key);
                if (digit_index < MAX_DIGITS && last_key >= '0' && last_key <= '9') 
                { 
                    digit_buffer[digit_index++] = last_key; 
                } 
                if (digit_index == MAX_DIGITS) 
                { 
                    input_complete = true; 
                } 
                state = WAIT_FOR_PRESS;
            }
        }
        //loop delay
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}

void app_main(void)
{
    init_keypad();
    gpio_init_all();
    disable_all_digits();

    xTaskCreate(keypad_task, "keypad_task", 2048, NULL, 5, NULL);
    xTaskCreate(timer_task, "timer_task", 2048, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 2048, NULL, 5, NULL);
}