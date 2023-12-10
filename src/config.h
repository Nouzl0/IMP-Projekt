#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>

// sample rate for the system
#define SAMPLE_RATE 8000
#define SAMPLES_PER_READ 1024

// I2S Microphone Settings
// Which channel is the I2S microphone on? I2S_CHANNEL_FMT_ONLY_LEFT or I2S_CHANNEL_FMT_ONLY_RIGHT
// Generally they will default to LEFT - but you may need to attach the L/R pin to GND
//#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_13
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_12
#define I2S_MIC_SERIAL_DATA GPIO_NUM_14

// speaker settings
#define I2S_SPEAKER_SERIAL_CLOCK GPIO_NUM_26
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK GPIO_NUM_25
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_27

// Buttons
#define GPIO_BUTTON_RECORD GPIO_NUM_2
#define GPIO_BUTTON_PLAY GPIO_NUM_4
#define GPIO_BUTTON_RIGHT GPIO_NUM_35
#define GPIO_BUTTON_LEFT GPIO_NUM_34

// sdcard (doens't work)
//#define PIN_NUM_MISO GPIO_NUM_19
//#define PIN_NUM_CLK GPIO_NUM_18
//#define PIN_NUM_MOSI GPIO_NUM_23
//#define PIN_NUM_CS GPIO_NUM_5

// i2s config for using the internal ADC
extern i2s_config_t i2s_adc_config;
// i2s config for reading from of I2S
extern i2s_config_t i2s_mic_Config;
// i2s microphone pins
extern i2s_pin_config_t i2s_mic_pins;
// i2s speaker pins
extern i2s_pin_config_t i2s_speaker_pins;