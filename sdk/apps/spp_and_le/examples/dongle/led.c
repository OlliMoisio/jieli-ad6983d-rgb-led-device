#include "system/timer.h"
#include "led.h"

static u8 led_spi_buffer[TOTAL_BUFF_LEN] __attribute__((aligned(4)));

Led leds[NUM_LEDS] = {0};

// are blinking leds currently on ?
bool blinking_currently_on = true;

// 1 second timer for blinking leds
static u16 blinking_timer_id = 0;

// SPI settings
const struct spi_platform_data spi1_p_data = {
    // map MOSI (data out) to PB6 (pin 3)
    .port = { (u8)-1, IO_PORTB_06, (u8)-1 },
    .mode = SPI_MODE_UNIDIR_1BIT, // We only need 1 wire output
    .role = SPI_ROLE_MASTER,
    .clk  = SPI_FREQ,
};

// called once every second
static void blinking_timer_callback(void *priv)
{
    blinking_currently_on = !blinking_currently_on;
    ws2812_update();
}

// handles 64 bytes of data from PC
u8 led_data_handler(u8 *rx_data, u32 rx_len)
{

    // loop through 64 bytes of LED data, in groups of 4
    for (int i = 0; i < 64; i += 4) {

        uint8_t header = rx_data[i];

        // if entire header byte is 0, skip to next group of 4
        if (header == 0) { continue; }

        // extract led index (lowest 5 bits)
        uint8_t led_index = header & MASK_INDEX;

        // extract power state (bits 5 and 6)
        // shift right by 5 to move bits 5-6 to position 0-1, then mask.
        uint8_t power_state = (header >> 5) & MASK_STATE;

        if (led_index < NUM_LEDS) {

            leds[led_index].color.red   = rx_data[i + 1];
            leds[led_index].color.green = rx_data[i + 2];
            leds[led_index].color.blue  = rx_data[i + 3];

            leds[led_index].state = (LedPowerState)power_state;

        }

    }

    ws2812_update();

}

void led_reset()
{
    memset(leds, 0, NUM_LEDS * sizeof(Led));
}

void led_init()
{
    blinking_timer_id = sys_timer_add(NULL, blinking_timer_callback, 1000);

    led_reset();
    spi_open(1);

}

// return led color, taking its power state into account
Color get_led_color(i)
{

    if (i >= NUM_LEDS) return (Color){ 0, 0, 0 };

    Led led = leds[i];

    switch(led.state) {

        case LED_OFF:
            return (Color){ 0, 0, 0 };

        case LED_ON:
            return led.color;

        case LED_BLINKING:

            if (blinking_currently_on) {
                return led.color;
            } else {
                return (Color){ 0, 0, 0 };
            }

        default:
            return (Color){ 0, 0, 0 };

    }

}

void ws2812_update() {

    // make sure any previous SPI operation is done
    while (JL_SPI1->CON & BIT(13));

    // make sure USB interrupts don't mess up the update
    OS_ENTER_CRITICAL();

    memset(led_spi_buffer, 0, sizeof(led_spi_buffer));

    // start inserting data after the preamble
    u8 *p = &led_spi_buffer[PREAMBLE_LEN];

    for (int i = 0; i < NUM_LEDS; i++) {

        Color led_color = get_led_color(i);

        // convert from RGB to GRB for ws2812

        u8 color_bytes[3] = {
            led_color.green,
            led_color.red,
            led_color.blue
        };

        for (int c = 0; c < 3; c++) {
            u8 val = color_bytes[c];
            // process every bit of the color byte
            for (int bit = 7; bit >= 0; bit--) {
                // encode at 6.4 mhz
                // if bit is 1: send 0xF8 (11111000) -> 0.78us High / 0.47us Low
                // if bit is 0: send 0xC0 (11000000) -> 0.31us High / 0.93us Low
                if ((val >> bit) & 1) {
                    *p++ = 0xF8;
                } else {
                    *p++ = 0xC0;
                }
            }
        }
    }

    asm("csync");

    // send complete buffer
    spi_dma_send(1, led_spi_buffer, TOTAL_BUFF_LEN);

    OS_EXIT_CRITICAL();

    // make sure PB6 output is stays low after data
    gpio_set_direction(IO_PORTB_06, 0);
    gpio_set_output_value(IO_PORTB_06, 0);

}
