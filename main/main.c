#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//CONSTANT VAR for task input
int HEARTS=0;
bool enable_set_hearts=true;
bool hearts_are_set =false;


//VARIABLES FOR SLIDE SWITCH
#define switch GPIO_NUM_19

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

bool countdown_running = false;
bool time_entered = false;

int entered_mins = 0;
int entered_secs = 0;

bool timer_started = false;     // prevents editing after timer begins
bool timer_finished = false;    // flag when timer reaches 00:00

bool invalid_time_entered = false; //invalid when seconds entered are more than 59

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
    0b01000000, // dash "-"
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

//INIT FOR SWITCH
void init_switch()
{
    gpio_reset_pin(switch);
    gpio_set_direction(switch, GPIO_MODE_INPUT);
    gpio_pullup_en(switch);
}

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

void display_dash() { 
    for(int i=0;i<4;i++) { 
        disable_all_digits(); 
        set_segments(singularSegment[0]); 
        enable_digit(i); 
        vTaskDelay(pdMS_TO_TICKS(2)); 
    } 
}

void timer_countdown_task(void *arg) {
    while (1) { //needs to run continiously! do not put any delays here or it will break
        bool switch_enable = gpio_get_level(switch);

        /* --- timer starts --- */
        if(switch_enable && time_entered && !timer_started) { 
            mins=entered_mins; 
            secs=entered_secs; 
            
            countdown_running = true;
            timer_started = true; 
            timer_finished = false;
 
        } 
        
        /* --- pause timer --- */
        if(!switch_enable && timer_started) 
        {
            countdown_running = false;
        }

        /* --- resume timer --- */
        if(switch_enable && timer_started) 
        {
            countdown_running = true;
        }

        /* --- countdown process --- */
        if(countdown_running && (mins>0 || secs>0)) { 
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            if(secs==0) { 
                if(mins>0) { 
                    mins--; 
                    secs=59; 
                } 
            } else 
            {
                secs--; 
            }
        }

        /* --- timer finished --- */
        if (secs==0 && mins==0 && timer_started) {
            countdown_running = false;
            timer_finished = true;
            // printf("OVER");
        }

        else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

//display task
void display_task(void *arg)
{
    while (1)
    {
        if (invalid_time_entered) {
            display_dash(); 
            continue;
        }
        if(timer_started) {
            display_time(mins, secs);
        } else {
            display_time(entered_mins, entered_secs);
        }
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

void keypad_time_input_task(void *arg) {
    int state = WAIT_FOR_PRESS; //state =0
    char new_key = NOPRESS;
    char last_key = NOPRESS;

    int time = 0;
    bool timed_out = false;

    while(1) {

        /* --- disable any editing after timer starts --- */
        if(timer_started) 
        {
            char key=scan_keypad(); 
            if(key=='#') { 
                mins=0; 
                secs=0; 
                timer_finished=true; 
                countdown_running=false; 
            }

            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }

        //fsm inputs update
        new_key = scan_keypad();
        time += LOOP_DELAY_MS;
        timed_out = (time >= DEBOUNCE_TIME);
        
        if (countdown_running)
        {
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }

        new_key=scan_keypad(); 
        time+=LOOP_DELAY_MS; 
        timed_out=(time>=DEBOUNCE_TIME);

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
                if(last_key=='*') { 
                    invalid_time_entered=true; 
                    vTaskDelay(pdMS_TO_TICKS(1000)); 
                    invalid_time_entered=false; 
                    digit_index=0; 
                    entered_mins=0; 
                    entered_secs=0; 
                    time_entered=false; 
                }
                else if(last_key>='0' && last_key<='9' && digit_index<MAX_DIGITS) { 
                    digit_buffer[digit_index++]=last_key; 
                    
                    int temp[4]={0,0,0,0};
                    for(int i=0;i<digit_index;i++) {
                        temp[i]=digit_buffer[i]-'0'; 
                    }
                    
                    entered_mins=temp[0]*10 + temp[1]; 
                    entered_secs=temp[2]*10 + temp[3];

                    //check invalid seconds
                    if(digit_index==4) {
                        if (entered_secs>59) {
                            invalid_time_entered=true; 
                            vTaskDelay(pdMS_TO_TICKS(1000)); 
                            invalid_time_entered=false; 
                            digit_index=0; 
                            entered_mins=0; 
                            entered_secs=0; 
                            time_entered=false;
                        }
                        else { 
                            time_entered=true; 
                        }
                    }
                }
                state = WAIT_FOR_PRESS;
            }
        }
        //loop delay
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}

void keypad_hearts_input_task(void *arg) {
    int state = WAIT_FOR_PRESS;
    char new_key = NOPRESS;
    char last_key = NOPRESS;

    int time = 0;
    bool timed_out = false;

    while(1) {
        //enter # of hearts
        new_key = scan_keypad();
        time += LOOP_DELAY_MS;
        timed_out = (time >= DEBOUNCE_TIME);

        if (state == WAIT_FOR_PRESS) {
            if (new_key != NOPRESS) {
                last_key = new_key;
                time = 0;
                state = DEBOUNCE;
            }
        }

        else if (state == DEBOUNCE) {
            if (timed_out && new_key == last_key) {
                state = WAIT_FOR_RELEASE;
            }
            else if (timed_out && new_key != last_key) {
                state = WAIT_FOR_PRESS;
            }
        }

        else if (state == WAIT_FOR_RELEASE) {
            if (new_key == NOPRESS) {
                //number is pressed
                if (last_key >= '0' && last_key <= '9') {
                    HEARTS = last_key - '0';
                    // printf("HEARTS set to %d\n", HEARTS);
                }

                //if * pressed - delete 
                else if (last_key == '*') {
                    HEARTS = 0;
                    hearts_are_set = false;
                    enable_set_hearts = true;
                    // printf("HEARTS reset\n");
                }
                //if # pressed - confirm and continue
                else if (last_key == '#') {
                    hearts_are_set = true; 
                    enable_set_hearts = false;
                    // printf("HEARTS confirmed: %d\n", HEARTS);
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
    init_switch();
    init_keypad();
    gpio_init_all();
    disable_all_digits();

    bool state=gpio_get_level(switch);
    // printf("%d", state);

    xTaskCreate(keypad_time_input_task, "keypad_time_input_task", 2048, NULL, 5, NULL);
    xTaskCreate(timer_countdown_task, "timer_countdown_task", 2048, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 2048, NULL, 5, NULL);
}