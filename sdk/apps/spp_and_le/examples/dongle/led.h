#include "asm/spi.h"
#include "asm/gpio.h"
#include "os/os_api.h"
#include <string.h>

#define NUM_LEDS        32
#define BYTES_PER_LED   24
#define PREAMBLE_LEN    4
#define POST_PAD_LEN    250
#define SPI_FREQ        6400000

#define DATA_LEN        (NUM_LEDS * BYTES_PER_LED)
#define TOTAL_BUFF_LEN  (PREAMBLE_LEN + DATA_LEN + POST_PAD_LEN)

// Binary: 0001 1111 -> Hex: 0x1F
// = lowest 5 bits of the first byte are the LED index
#define MASK_INDEX 0x1F

// Binary: 0000 0011 -> Hex: 0x03 (applied after shifting)
// = third highest and second highest bits are the LED power state
#define MASK_STATE 0x03

// 1. Color Struct
// Using uint8_t ensures the values are exactly 8 bits (0-255)
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

// 2. Power State Enum
// An enum is best here to clearly define mutually exclusive states.
typedef enum {
    LED_OFF = 0,
    LED_ON = 1,
    LED_BLINKING = 2
} LedPowerState;

// 3. LED Struct
// Combines the color and the power state
typedef struct {
    Color color;
    LedPowerState state;
} Led;

void led_reset();
void led_init();
void led_set(u8 led_num, u8 red, u8 green, u8 blue);
u8 led_data_handler(u8 *rx_data, u32 rx_len);

void ws2812_update();
