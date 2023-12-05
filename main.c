//Implement a program for switching LEDs on/off and dimming them. The program should work as follows:
//Rot_Sw, the push button on the rotary encoder shaft is the on/off button. When button is pressed the state of LEDs is toggled. Program must require the button to be released before the LEDs toggle again. Holding the button may not cause LEDs to toggle multiple times.
//Rotary encoder is used to control brightness of the LEDs. Turning the knob clockwise increases brightness and turning counter-clockwise reduces brightness. If LEDs are in OFF state turning the knob has no effect.
//When LED state is toggled to ON the program must use same brightness of the LEDs they were at when they were switched off. If LEDs were dimmed to 0% then toggling them on will set 50% brightness.
//PWM frequency divider must be configured to output 1 MHz frequency and PWM frequency must be 1 kHz.

#include <stdio.h>
#include <pico/time.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"
#include "hardware/pwm.h"


//define LED pins
#define LED1 22
#define LED2 21
#define LED3 20
//define for PWM
#define CLOCK_DIVIDER 125
#define TOP 999
//define for rotary button
#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12

bool pressed(uint button);
void set_lights_brightness(uint *duty_cycle, uint *slice_nr1, uint *channel1, uint *slice_nr2, uint *channel2, uint *slice_nr3, uint *channel3);
void initializePins(uint *slice_l1, uint *channel_l1, uint *slice_l2, uint *channel_l2, uint *slice_l3, uint *channel_l3, uint *duty_cycle);

static queue_t events;
static uint duty_cycle = 0;

static void rot_brightness_handler(uint gpio, uint32_t event_mask){
    if (gpio == ROT_A && !gpio_get(ROT_B)) {        // Turning clockwise and increase brightness
        if (duty_cycle < 100) {
            duty_cycle+=5;
            queue_try_add(&events, &duty_cycle);
        }
    }else if (gpio== ROT_A && gpio_get(ROT_B)){     // Turning anti-clockwise and decrease brightness
        if(duty_cycle >0){
            duty_cycle-=5;
            queue_try_add(&events, &duty_cycle);
        }
    }
}

int main() {
    uint brightness_state = 0;
    uint slice_l1, slice_l2, slice_l3;
    uint channel_l1, channel_l2, channel_l3;
    bool led_on = false;
    //Initialize the queue for brightness level
    queue_init(&events, sizeof(int), 100);

    initializePins(&slice_l1, &channel_l1, &slice_l2, &channel_l2, &slice_l3, &channel_l3, &duty_cycle);

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_RISE, true, &rot_brightness_handler);

    // Loop forever
    while (true) {
        if(led_on){
            if(queue_try_remove(&events, &brightness_state)){
                duty_cycle = brightness_state;
                set_lights_brightness(&duty_cycle, &slice_l1, &channel_l1, &slice_l2, &channel_l2, &slice_l3, &channel_l3);
            }
        }

        if (!pressed(ROT_SW)) {
            if(!duty_cycle){
                if (brightness_state == 0) {
                    duty_cycle = 50;
                    brightness_state = duty_cycle;
                } else {
                    duty_cycle = brightness_state;
                }
                printf("brightness level is %u & duty cycle turned on %u\n", brightness_state, duty_cycle);
                set_lights_brightness(&duty_cycle, &slice_l1, &channel_l1, &slice_l2, &channel_l2, &slice_l3, &channel_l3);
                led_on = true;
                while(!gpio_get(ROT_SW)){
                    sleep_ms(50);
                };
            }else{
                brightness_state = duty_cycle;
                duty_cycle = 0;
                set_lights_brightness(&duty_cycle, &slice_l1, &channel_l1, &slice_l2, &channel_l2, &slice_l3, &channel_l3);
                printf("Button pressed, Turn off lights\n");
                led_on = false;
                while (!gpio_get(ROT_SW)){
                    sleep_ms(50);
                }
            }
        }
    }

    return 0;
}


bool pressed(uint button) {
    int press = 0;
    int release = 0;
    while (press < 3 && release < 3) {
        if (gpio_get(button)) {
            press++;
            release = 0;
        } else {
            release++;
            press = 0;
        }
        sleep_ms(10);
    }
    if (press > release) return true;
    else return false;
}

void set_lights_brightness(uint *duty_cycle, uint *slice_nr1, uint *channel1, uint *slice_nr2, uint *channel2, uint *slice_nr3, uint *channel3){
    pwm_set_chan_level(*slice_nr1, *channel1, (TOP + 1) * *duty_cycle / 100);
    pwm_set_chan_level(*slice_nr2, *channel2, (TOP + 1) * *duty_cycle / 100);
    pwm_set_chan_level(*slice_nr3, *channel3, (TOP + 1) * *duty_cycle / 100);
}

void initializePins(uint *slice_l1, uint *channel_l1, uint *slice_l2, uint *channel_l2, uint *slice_l3, uint *channel_l3, uint *duty_cycle){
    //Initialize chosen serial port
    stdio_init_all();

    //Initialize led
    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);
    gpio_init(LED3);
    gpio_set_dir(LED3, GPIO_OUT);

    //Initialize the rotary button
    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);

    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);

    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW);

    //Configure PWM led lights
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, TOP);
    pwm_config_set_clkdiv_int(&config, CLOCK_DIVIDER);

    // LED1:
    *slice_l1 = pwm_gpio_to_slice_num(LED1);
    *channel_l1 = pwm_gpio_to_channel(LED1);
    pwm_set_enabled(*slice_l1, false);
    pwm_init(*slice_l1, &config, false);
    pwm_set_chan_level(*slice_l1, *channel_l1, (TOP + 1) * *duty_cycle / 100);
    gpio_set_function(LED1, GPIO_FUNC_PWM);
    pwm_set_enabled(*slice_l1, true);

    // LED2:
    *slice_l2 = pwm_gpio_to_slice_num(LED2);
    *channel_l2 = pwm_gpio_to_channel(LED2);
    pwm_set_enabled(*slice_l2, false);
    pwm_init(*slice_l2, &config, false);
    pwm_set_chan_level(*slice_l2, *channel_l2, (TOP + 1) * *duty_cycle / 100);
    gpio_set_function(LED2, GPIO_FUNC_PWM);
    pwm_set_enabled(*slice_l2, true);

    // LED3:
    *slice_l3 = pwm_gpio_to_slice_num(LED3);
    *channel_l3 = pwm_gpio_to_channel(LED3);
    pwm_set_enabled(*slice_l3, false);
    pwm_init(*slice_l3, &config, false);
    pwm_set_chan_level(*slice_l3, *channel_l3, (TOP + 1) * *duty_cycle / 100);
    gpio_set_function(LED3, GPIO_FUNC_PWM);
    pwm_set_enabled(*slice_l3, true);

}

