#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#define UART_NUM        UART_NUM_2
#define BAUD_RATE       9600      // Example baud rate
#define TXD_PIN  GPIO_NUM_17
#define RXD_PIN  GPIO_NUM_16

void init_uart(void);  //UART initialization function

void app_main() {
    printf("UART Demo \n");
    printf("System starting up...\n");
    init_uart();
    while(1) {
        uart_write_bytes(UART_NUM, "1", 1);
        printf("Hi\n");
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void init_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver 
    uart_driver_install(UART_NUM, 1024, 0, 0, NULL, 0);

    // Configure UART parameters
    uart_param_config(UART_NUM, &uart_config);

    // Set UART pins (TX, RX, RTS, CTS)
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}