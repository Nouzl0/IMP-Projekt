#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "I2SSampler.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "SDCard.h"
#include "SPIFFS.h"
#include "WAVFileWriter.h"
#include "WAVFileReader.h"
#include "config.h"
#include "esp_timer.h"

#include "ssd1306.h"
#include "font8x8_basic.h"


#define tag "SSD1306"


static const char *TAG = "app";


int audio_index = 0;

char *audio_path[] = { 
  "/sdcard/test0.wav", 
  "/sdcard/test1.wav",   
  "/sdcard/test2.wav",
  "/sdcard/test3.wav",
  "/sdcard/test4.wav"
};

bool select_bool[] = { 
  true, 
  false,   
  false,
  false,
  false
};

char *audio_name[] = { 
  "Recording 0     ", 
  "Recording 1     ",   
  "Recording 2     ",
  "Recording 3     ",
  "Recording 4     "
};


extern "C"
{
  void app_main(void);
}

/*
void wait_for_button_push()
{
  ESP_LOGI(TAG, "Waiting for button push");
  while (gpio_get_level(GPIO_BUTTON) == 0)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
*/

void record(I2SSampler *input, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * 1024);
  ESP_LOGI(TAG, "Start recording");
  input->start();
  // open the file on the sdcard
  FILE *fp = fopen(fname, "wb");
  // create a new wave file writer
  WAVFileWriter *writer = new WAVFileWriter(fp, input->sample_rate());
  // keep writing until the user releases the button
  while (gpio_get_level(GPIO_BUTTON_RECORD) == 1)
  {
    int samples_read = input->read(samples, 1024);
    int64_t start = esp_timer_get_time();
    writer->write(samples, samples_read);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "Wrote %d samples in %lld microseconds", samples_read, end - start);
  }
  // stop the input
  input->stop();
  // and finish the writing
  writer->finish();
  fclose(fp);
  delete writer;
  free(samples);
  ESP_LOGI(TAG, "Finished recording");
}

void play(Output *output, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * 1024);
  // open the file on the sdcard
  FILE *fp = fopen(fname, "rb");
  // create a new wave file writer
  WAVFileReader *reader = new WAVFileReader(fp);
  ESP_LOGI(TAG, "Start playing");
  output->start(reader->sample_rate());
  ESP_LOGI(TAG, "Opened wav file");
  // read until theres no more samples
  while (true)
  {
    int samples_read = reader->read(samples, 1024);
    if (samples_read == 0)
    {
      break;
    }
    ESP_LOGI(TAG, "Read %d samples", samples_read);
    output->write(samples, samples_read);
    ESP_LOGI(TAG, "Played samples");
  }
  // stop the input
  output->stop();
  fclose(fp);
  delete reader;
  free(samples);
  ESP_LOGI(TAG, "Finished playing");
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting up");

// [0] - Initialize SD-CARD or SPIFFS
#ifdef USE_SPIFFS
  ESP_LOGI(TAG, "Mounting SPIFFS on /sdcard");
  new SPIFFS("/sdcard");
#else
  ESP_LOGI(TAG, "Mounting SDCard on /sdcard");
  new SDCard("/sdcard", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
#endif


// [1] Initialize the microphone and speaker
  ESP_LOGI(TAG, "Creating microphone");
#ifdef USE_I2S_MIC_INPUT
  I2SSampler *input = new I2SMEMSSampler(I2S_NUM_0, i2s_mic_pins, i2s_mic_Config);
#else
  I2SSampler *input = new ADCSampler(ADC_UNIT_1, ADC1_CHANNEL_7, i2s_adc_config);
#endif
  I2SOutput *output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);


 // [2] Initialize the buttons
  gpio_set_direction(GPIO_BUTTON_PLAY, GPIO_MODE_INPUT);           // Play button
  gpio_set_pull_mode(GPIO_BUTTON_PLAY, GPIO_PULLDOWN_ONLY);      
  gpio_set_direction(GPIO_BUTTON_RECORD, GPIO_MODE_INPUT);         // Record button
  gpio_set_pull_mode(GPIO_BUTTON_RECORD, GPIO_PULLDOWN_ONLY);
  gpio_set_direction(GPIO_BUTTON_LEFT, GPIO_MODE_INPUT);           // Left button
  gpio_set_pull_mode(GPIO_BUTTON_LEFT, GPIO_PULLDOWN_ONLY);  
  gpio_set_direction(GPIO_BUTTON_RIGHT, GPIO_MODE_INPUT);          // Right button
  gpio_set_pull_mode(GPIO_BUTTON_RIGHT, GPIO_PULLDOWN_ONLY);

 // [3] Initialize the display
  SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

  // display init sequence + pin config
#if CONFIG_I2C_INTERFACE
	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
	ESP_LOGI(tag, "INTERFACE is SPI");
	ESP_LOGI(tag, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(tag, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(tag, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
	ESP_LOGI(tag, "CONFIG_DC_GPIO=%d",CONFIG_DC_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
#endif // CONFIG_SSD1306_128x64
#if CONFIG_SSD1306_128x32
	ESP_LOGI(tag, "Panel is 128x32");
	ssd1306_init(&dev, 128, 32);
#endif // CONFIG_SSD1306_128x32

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text_x3(&dev, 0, "Starting ...", 3, false);
	vTaskDelay(000 / portTICK_PERIOD_MS);


  while (true) {

    ESP_LOGI(TAG, "Waiting for button push");

    // Show display
    #if CONFIG_SSD1306_128x64
    	top = 2;
    	center = 3;
    	bottom = 8;
    	ssd1306_display_text(&dev, 0, audio_name[0], 20, select_bool[0]);
    	ssd1306_display_text(&dev, 1, audio_name[1], 20, select_bool[1]);
    	ssd1306_display_text(&dev, 2, audio_name[2], 20, select_bool[2]);
    	ssd1306_display_text(&dev, 3, audio_name[3], 20, select_bool[3]);
      ssd1306_display_text(&dev, 4, audio_name[4], 20, select_bool[4]);
    #endif // CONFIG_SSD1306_128x64


    while (gpio_get_level(GPIO_BUTTON_LEFT) == 0 && gpio_get_level(GPIO_BUTTON_RIGHT) == 0 && gpio_get_level(GPIO_BUTTON_PLAY) == 0 && gpio_get_level(GPIO_BUTTON_RECORD) == 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }


    // Play button logic
    if (gpio_get_level(GPIO_BUTTON_PLAY) == 1) {
      play(output, "/sdcard/test.wav");
    }

    // Record button logic
    if (gpio_get_level(GPIO_BUTTON_RECORD) == 1) {
      record(input, "/sdcard/test.wav");
    }


    // Left button logic
    if (gpio_get_level(GPIO_BUTTON_RIGHT) == 1) {\

      ESP_LOGI(TAG, "Right button pressed");

      if (audio_index > 0) {
        select_bool[audio_index] = false;
        audio_index = audio_index - 1;
        select_bool[audio_index] = true;
      }
    }


    // Right button logic
    if (gpio_get_level(GPIO_BUTTON_LEFT) == 1) {
      ESP_LOGI(TAG, "Left button pressed");

      if (audio_index < 4) {
        select_bool[audio_index] = false;
        audio_index = audio_index + 1;
        select_bool[audio_index] = true;

      }
    }

     vTaskDelay(pdMS_TO_TICKS(100));
  }
}