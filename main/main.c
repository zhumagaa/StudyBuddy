#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hd44780.h>
#include <esp_idf_lib_helpers.h>
#include "driver/gpio.h"

#define BUZZER GPIO_NUM_19
#define BUTTON GPIO_NUM_1
#define switch_pin GPIO_NUM_20

// Custom characters
uint8_t customCharRight[8] = {0x00,0x1B,0x1F,0x0E,0x04,0x00,0x00,0x00};
uint8_t customCharLeft[8] = {0x00,0x1B,0x1F,0x1F,0x0E,0x00,0x00,0x00};

hd44780_t lcd =
{
 .write_cb = NULL,
 .font = HD44780_FONT_5X8,
 .lines = 4, // 16x4 LCD
 .pins = {
 .rs = GPIO_NUM_38,
 .e = GPIO_NUM_37,
 .d4 = GPIO_NUM_36,
 .d5 = GPIO_NUM_35,
 .d6 = GPIO_NUM_48,
 .d7 = GPIO_NUM_47,
 .bl = HD44780_NOT_USED
 }
};

void buzzer_task(void *pv)
{
 //button configuration 
 gpio_config_t btn_conf = {
 .pin_bit_mask = (1ULL << BUTTON),
 .mode = GPIO_MODE_INPUT,
 .pull_up_en = GPIO_PULLUP_ENABLE,
 .pull_down_en = GPIO_PULLDOWN_DISABLE,
 .intr_type = GPIO_INTR_DISABLE
 };
 gpio_config(&btn_conf);

 //buzzer configuration
 gpio_reset_pin(BUZZER);
 gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);
 gpio_set_level(BUZZER, 0);

 int prev = 1; // button not pressed
 int curr = 1;
 int buzzer_on = 0; // buzzer starts OFF

 while (1) {
 curr = gpio_get_level(BUTTON);

 //detects release 
 if (prev == 0 && curr == 1) {
 buzzer_on = !buzzer_on; // toggle buzzer
 gpio_set_level(BUZZER, buzzer_on);
 }

 prev = curr;
 vTaskDelay(pdMS_TO_TICKS(20)); // debounce
 }
}

//-----------------------------------------------timer and keypad stuff---------------------------------------------------------------------------
//CONSTANT VAR for task input
int HEARTS=0;
bool enable_set_hearts=true;
bool hearts_are_set =false;


// VARIABLES FOR TIMER
// Segment pins
#define SEG_A 42 
#define SEG_B 40
#define SEG_C 9
#define SEG_D 5
#define SEG_E 6 
#define SEG_F 41 
#define SEG_G 10 
#define SEG_P 4 

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

bool timer_started = false; // prevents editing after timer begins
bool timer_finished = false; // flag when timer reaches 00:00

bool invalid_time_entered = false; //invalid when seconds entered are more than 59
int invalid_timer = 0;
bool showing_invalid = false;

// VARIABLES FOR KEYPAD
#define LOOP_DELAY_MS 10 // Loop sampling time (ms)
#define DEBOUNCE_TIME 40 // Debounce time (ms)
#define NROWS 4 // Number of keypad rows
#define NCOLS 3 // Number of keypad columns

#define ACTIVE 0 // Keypad active state (0 = low (pullup), 1 = high (pulldown))
#define NOPRESS '\0' // NOPRESS character

//fsm states
#define WAIT_FOR_PRESS 0
#define DEBOUNCE 1
#define WAIT_FOR_RELEASE 2

int row_pins[] = {GPIO_NUM_3, GPIO_NUM_8, GPIO_NUM_18, GPIO_NUM_17}; // Pin numbers for rows
int col_pins[] = {GPIO_NUM_16, GPIO_NUM_15, GPIO_NUM_7}; // Pin numbers for columns

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
 0b01101111 // 9
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
char keypad_array[NROWS][NCOLS] = { // Keypad layout
 {'1', '2', '3'},
 {'4', '5', '6'},
 {'7', '8', '9'},
 {'*', '0', '#'}
};

//INIT FOR SWITCH
void init_switch()
{
 gpio_reset_pin(switch_pin);
 gpio_set_direction(switch_pin, GPIO_MODE_INPUT);
 gpio_pullup_en(switch_pin);
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
 { gpio_set_level(row_pins[i], 1); } // inactive = high
 else
 { gpio_set_level(row_pins[i], 0); } // inactive = low
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
 bool switch_enable = gpio_get_level(switch_pin);

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

typedef enum {
 MODE_SET_HEARTS,
 MODE_SET_TIME,
 MODE_TIMER_RUNNING
} keypad_mode_t;

keypad_mode_t keypad_mode = MODE_SET_HEARTS;

void keypad_input_task(void *arg) {
 int state = WAIT_FOR_PRESS;
 char new_key = NOPRESS;
 char last_key = NOPRESS;

 int time = 0;
 bool timed_out = false;

 while (1)
 {
 new_key = scan_keypad();
 time += LOOP_DELAY_MS;
 timed_out = (time >= DEBOUNCE_TIME);

 if (showing_invalid)
 {
 invalid_timer += LOOP_DELAY_MS;

 if (invalid_timer >= 1000)
 {
 invalid_time_entered = false;
 showing_invalid = false;
 }
 }

 /* ---------------- FSM ---------------- */

 if (state == WAIT_FOR_PRESS)
 {
 if (new_key != NOPRESS)
 {
 last_key = new_key;
 time = 0;
 state = DEBOUNCE;
 }
 }
 else if (state == DEBOUNCE)
 {
 if (timed_out && new_key == last_key)
 {
 state = WAIT_FOR_RELEASE;
 }
 else if (timed_out && new_key != last_key)
 {
 state = WAIT_FOR_PRESS;
 }
 }
 else if (state == WAIT_FOR_RELEASE)
 {
 if (new_key == NOPRESS)
 {
 /* ===== KEY CONFIRMED ===== */

 /* ---------------- TIMER RUNNING ---------------- */
 if (countdown_running)
 {
 if (last_key == '#')
 {
 mins = 0;
 secs = 0;
 timer_finished = true;
 countdown_running = false;
 }
 }

 /* ================= SET NUMBER OF TASKS ================= */
 else if (!hearts_are_set)
 {
 if (last_key >= '0' && last_key <= '9')
 {
 HEARTS = last_key - '0';
 }
 else if (last_key == '*')
 {
 HEARTS = 0;
 hearts_are_set = false;
 }
 else if (last_key == '#')
 {
 hearts_are_set = true;
 }
 }

 /* ---------------- TIME EDIT MODE ---------------- */
 else
 {
 /* DELETE (backspace) */
 if (last_key == '*')
 {
 if (digit_index > 0)
 {
 digit_index--;
 }

 /* recompute time from digits */
 int temp[4] = {0,0,0,0};

 for(int i=0;i<digit_index;i++)
 temp[i] = digit_buffer[i] - '0';

 entered_mins = temp[0]*10 + temp[1];
 entered_secs = temp[2]*10 + temp[3];

 time_entered = false;
 }

 /* DIGIT ENTRY */
 else if (last_key >= '0' && last_key <= '9')
 {
 if (digit_index < MAX_DIGITS)
 {
 digit_buffer[digit_index++] = last_key;
 }

 int temp[4] = {0,0,0,0};

 for(int i=0;i<digit_index;i++)
 temp[i] = digit_buffer[i] - '0';

 entered_mins = temp[0]*10 + temp[1];
 entered_secs = temp[2]*10 + temp[3];

 if (digit_index == 4)
 {
 if (entered_secs > 59)
 {
 invalid_time_entered = true;
 showing_invalid = true;
 invalid_timer = 0;

 digit_index = 0;
 entered_mins = 0;
 entered_secs = 0;
 time_entered = false;
 }
 else
 {
 time_entered = true;
 }
 }
 }
 }

 state = WAIT_FOR_PRESS;
 }
 }

 vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
 }
}


//--------------------------------------new tasks----------------------------------------------------------------------
typedef enum {
 LCD_STATE_WELCOME_SCREEN = 0,
 LCD_STATE_ASK_TASK_COUNT,
 LCD_STATE_CONFIRM_TASK_COUNT,
 LCD_STATE_DISPLAY_TASK_ICONS,
 LCD_STATE_ASK_TIME_INPUT,
 LCD_STATE_CONFIRM_TIME,
 LCD_STATE_RUNNING
} lcd_state_t;

#define LCD_CHAR_LEFT_ICON 0
#define LCD_CHAR_RIGHT_ICON 1


void study_manager_task(void *arg)
{
 int hearts_remaining = 0;
 int session;

 /* ---------- LCD custom characters ---------- */
 hd44780_upload_character(&lcd, 0, customCharLeft);
 hd44780_upload_character(&lcd, 1, customCharRight);

 /* ---------- WELCOME ---------- */
 hd44780_clear(&lcd);
 hd44780_gotoxy(&lcd, 2, 1);
 hd44780_puts(&lcd, "Welcome!");
 vTaskDelay(pdMS_TO_TICKS(2000));

 /* ---------- ENTER NUMBER OF HEARTS ---------- */
 hd44780_clear(&lcd);
 hd44780_gotoxy(&lcd,0,0);
 hd44780_puts(&lcd,"Enter sessions");
 hd44780_gotoxy(&lcd,0,1);
 hd44780_puts(&lcd,"Press # to OK");

 while(!hearts_are_set)
 {
 char buf[16];
 sprintf(buf,"Hearts:%d", HEARTS);
 hd44780_gotoxy(&lcd,0,2);
 hd44780_puts(&lcd,buf);
 vTaskDelay(pdMS_TO_TICKS(200));
 }

 hearts_remaining = HEARTS;

 /* ---------- MAIN SESSION LOOP ---------- */
 for(session = 1; session <= HEARTS; session++)
 {
 /* ---------- DISPLAY HEARTS ---------- */
 hd44780_clear(&lcd);
 hd44780_gotoxy(&lcd,0,0);
 char buf[32];
 snprintf(buf,sizeof(buf),"Task %d/%d",session,HEARTS);
 hd44780_puts(&lcd,buf);

 hd44780_gotoxy(&lcd,0,1);

 for(int i=0;i<hearts_remaining;i++)
 hd44780_putc(&lcd,1);

 for(int i=hearts_remaining;i<HEARTS;i++)
 hd44780_putc(&lcd,0);

 vTaskDelay(pdMS_TO_TICKS(2000));

 /* ---------- ENTER TIMER ---------- */
 hd44780_clear(&lcd);
 hd44780_gotoxy(&lcd,0,0);
 hd44780_puts(&lcd,"Enter time MMSS");

 time_entered = false;
 timer_started = false;

 while(!time_entered)
 vTaskDelay(pdMS_TO_TICKS(200));

 /* ---------- READY PROMPT ---------- */
 hd44780_clear(&lcd);
 hd44780_puts(&lcd,"Ready?");
 hd44780_gotoxy(&lcd,0,1);
 hd44780_puts(&lcd,"Flip switch");

 bool ready=false;
 int elapsed=0;

 while(elapsed < 10000)
 {
 if(gpio_get_level(switch_pin))
 {
 ready=true;
 break;
 }

 elapsed+=200;
 vTaskDelay(pdMS_TO_TICKS(200));
 }

 if(!ready)
 {
 hd44780_clear(&lcd);
 hd44780_puts(&lcd,"Not ready");
 session--;
 continue;
 }

 /* ---------- START TIMER ---------- */
 timer_started = true;

 hd44780_clear(&lcd);
 hd44780_gotoxy(&lcd,0,0);
 snprintf(buf,sizeof(buf), "Task %d/%d",session,HEARTS);
 hd44780_puts(&lcd,buf);

 /* ---------- WAIT UNTIL TIMER ENDS ---------- */
 while(!timer_finished)
 {
 /* skip remaining time */
 char key = scan_keypad();
 if(key == '#')
 {
 mins = 0;
 secs = 0;
 timer_finished = true;
 }

 vTaskDelay(pdMS_TO_TICKS(100));
 }

 /* ---------- TASK COMPLETION QUESTION ---------- */
 hd44780_clear(&lcd);
 hd44780_puts(&lcd,"Finished?");
 hd44780_gotoxy(&lcd,0,1);
 hd44780_puts(&lcd,"Press button");

 bool finished=false;

 for(int i=0;i<50;i++)
 {
 if(gpio_get_level(BUTTON)==0)
 {
 finished=true;
 break;
 }

 vTaskDelay(pdMS_TO_TICKS(100));
 }

 if(finished)
 {
 hd44780_clear(&lcd);
 hd44780_puts(&lcd,"Great job!");

 for(int i=0;i<5;i++)
 {
 hd44780_gotoxy(&lcd,i,1);
 hd44780_putc(&lcd,1);
 }

 vTaskDelay(pdMS_TO_TICKS(2000));
 }
 else
 {
 hearts_remaining--;

 hd44780_clear(&lcd);
 hd44780_puts(&lcd,"Task failed");

 for(int i=0;i<5;i++)
 {
 hd44780_gotoxy(&lcd,i,1);
 hd44780_putc(&lcd,0);
 }

 vTaskDelay(pdMS_TO_TICKS(2000));
 }

 timer_finished=false;
 timer_started=false;
 }

 /* ---------- FINAL RESULT ---------- */

 hd44780_clear(&lcd);

 if(hearts_remaining == 0)
 {
 hd44780_puts(&lcd,"Keep trying!");
 }
 else
 {
 hd44780_puts(&lcd,"Well done!");
 }

 hd44780_gotoxy(&lcd,0,1);
 hd44780_puts(&lcd,"Stay motivated!");

 while(1)
 vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main()
{
 init_switch();
 init_keypad();
 gpio_init_all();
 disable_all_digits();

 hd44780_init(&lcd);

 xTaskCreate(keypad_input_task, "keypad_input_task", 4096,NULL,5,NULL);
 xTaskCreate(timer_countdown_task,"timer_task",4096,NULL,5,NULL);
 xTaskCreate(display_task,"7seg_display",4096,NULL,5,NULL);

 xTaskCreate(buzzer_task,"buzzer",2048,NULL,5,NULL);

 xTaskCreate(study_manager_task,"study_manager",8192,NULL,6,NULL);
}
