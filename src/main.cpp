#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "I2SSampler.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "SPIFFS.h"
#include "SDCard.h"
#include "WAVFileWriter.h"
#include "WAVFileReader.h"
#include "config.h"
#include "esp_timer.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

/**
 * Callback app_main()
*/
extern "C"
{
  void app_main(void);
}

/**
 * record_audio() - Record audio from the microphone and save it to a file
 * 
 * @param input - I2SSampler - audio sampler
 * @param fname - const char - audio file name
*/
void record_audio(I2SSampler *input, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * SAMPLES_PER_READ);
  ESP_LOGI("MIC", "Start recording");
  input->start();

  // open the file on the sdcard
  FILE *fp = fopen(fname, "wb");

  // create a new wave file writer
  WAVFileWriter *writer = new WAVFileWriter(fp, input->sample_rate());

  // keep writing until the user releases the button
  while (gpio_get_level(GPIO_BUTTON_RECORD) == 1)
  {
    int samples_read = input->read(samples, SAMPLES_PER_READ);
    int64_t start = esp_timer_get_time();
    writer->write(samples, samples_read);
    int64_t end = esp_timer_get_time();
    ESP_LOGI("MIC", "Wrote %d samples in %lld microseconds", samples_read, end - start);
  }

  // stop the input
  input->stop();

  // and finish the writing
  writer->finish();
  fclose(fp);
  delete writer;
  free(samples);
  ESP_LOGI("MIC", "Finished recording\n");
}

/**
 * play_audio() - Play audio from a file
 * 
 * @param output - Read audio from a file
 * @param fname - file name
*/
void play_audio(Output *output, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * SAMPLES_PER_READ);
  
  // open the file on the sdcard
  FILE *fp = fopen(fname, "rb");

  // create a new wave file writer
  WAVFileReader *reader = new WAVFileReader(fp);

  // start playing the audio file
  ESP_LOGI("AMP", "Start playing");
  output->start(reader->sample_rate());
  ESP_LOGI("AMP", "Opened wav file");
  
  // read until theres no more samples
  while (true)
  {
    int samples_read = reader->read(samples, SAMPLES_PER_READ);
    if (samples_read == 0)
    {
      break;
    }
    ESP_LOGI("AMP", "Read %d samples", samples_read);
    output->write(samples, samples_read);
    ESP_LOGI("AMP", "Played samples");
  }
  // stop the input
  output->stop();
  fclose(fp);
  delete reader;
  free(samples);
  ESP_LOGI("AMP", "Finished playing\n");
}

/**
 * display() - Main function
 * 
 * @param dev - SSD1306_t, display, screen
 * @param select_bool - Display selected row
 * @param audio_name  - All audio files
*/
void display(SSD1306_t *dev, bool *select_bool, char **audio_name) {
  ESP_LOGI("OLED", "Displaying text");
  
  ssd1306_display_text(dev, 0, audio_name[0], 20, select_bool[0]);
  ssd1306_display_text(dev, 1, audio_name[1], 20, select_bool[1]);
  ssd1306_display_text(dev, 2, audio_name[2], 20, select_bool[2]);
  ssd1306_display_text(dev, 3, audio_name[3], 20, select_bool[3]);
  ssd1306_display_text(dev, 4, audio_name[4], 20, select_bool[4]);
  ssd1306_display_text(dev, 5, audio_name[5], 20, select_bool[5]);
  ssd1306_display_text(dev, 6, audio_name[6], 20, select_bool[6]);
  ssd1306_display_text(dev, 7, audio_name[7], 20, select_bool[7]);
}

/**
 * btn_up() - Button left/Up, decrement's audio_index and moves the selection
 * 
 * @param audio_index - Selected audio file
 * @param select_bool - Display selected row
*/
void btn_up(int *audio_index, bool *select_bool, int min_index, int max_index) {
  ESP_LOGI("UBTN", "Up button pressed");

  if (*audio_index > min_index) {
    select_bool[*audio_index] = false;
    (*audio_index)--;
    select_bool[*audio_index] = true;
  } else {
    select_bool[*audio_index] = false;
    *audio_index = max_index;
    select_bool[*audio_index] = true;
  }
}

/**
 *  btn_down() - Button right/Down, increment's audio_index and moves the selection
 * 
 * @param audio_index - Selected audio file
 * @param select_bool - Display selected row
 * @param min_index - Minimum index
 * @param max_index - Maximum index
*/
void btn_down(int *audio_index, bool *select_bool, int min_index, int max_index) {
  ESP_LOGI("DBTN", "Down button pressed");

  if (*audio_index < max_index) {
    select_bool[*audio_index] = false;
    (*audio_index)++;
    select_bool[*audio_index] = true;
  } else {
    select_bool[*audio_index] = false;
    *audio_index = min_index;
    select_bool[*audio_index] = true;
  }
}

/**
 * app_main() - Main function
*/
void app_main(void)
{
  // [1] INITIALIZE BUTTONS WITH DISPLAY
  // enable buttons
  ESP_LOGI("MAIN", "Starting up");
  gpio_set_direction(GPIO_BUTTON_PLAY, GPIO_MODE_INPUT);           // Play button
  gpio_set_pull_mode(GPIO_BUTTON_PLAY, GPIO_PULLDOWN_ONLY);      
  gpio_set_direction(GPIO_BUTTON_RECORD, GPIO_MODE_INPUT);         // Record button
  gpio_set_pull_mode(GPIO_BUTTON_RECORD, GPIO_PULLDOWN_ONLY);
  gpio_set_direction(GPIO_BUTTON_LEFT, GPIO_MODE_INPUT);           // Left button
  gpio_set_pull_mode(GPIO_BUTTON_LEFT, GPIO_PULLDOWN_ONLY);  
  gpio_set_direction(GPIO_BUTTON_RIGHT, GPIO_MODE_INPUT);          // Right button
  gpio_set_pull_mode(GPIO_BUTTON_RIGHT, GPIO_PULLDOWN_ONLY);
  ESP_LOGI("MAIN", "Buttons enabled");

  // initialize display
  SSD1306_t dev;    
	char lineChar[20];

  // display init sequence + pin config
  ESP_LOGI("OLED", "INTERFACE is i2c");
  ESP_LOGI("OLED", "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
  ESP_LOGI("OLED", "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
  ESP_LOGI("OLED", "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
  i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
  
  // panel size
  ESP_LOGI("OLED", "Panel is 128x64");
  ssd1306_init(&dev, 128, 64);

  // configure screen text
  ssd1306_clear_screen(&dev, false);
  ssd1306_contrast(&dev, 0xff);
  ssd1306_display_text_x3(&dev, 0, "Init", 20, false);
  vTaskDelay(000 / portTICK_PERIOD_MS);
  ESP_LOGI("MAIN", "Configured SSD1306 Adafruit display");


  /* [2] - INITIALIZE SDCARD (doens't work)
  //ESP_LOGI("MAIN", "Mounting SDCard on /audio");
  //new SDCard("/audio", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

  Error message:
  E (3366) sdmmc_common: sdmmc_init_ocr: send_op_cond (1) returned 0x107
  E (3366) vfs_fat_sdmmc: sdmmc_card_init failed (0x107).
  E (3376) SDC: Failed to initialize the card (ESP_ERR_TIMEOUT). Make sure SD card lines have pull-up resistors in place.
  */

  // [2] - INITIALIZE SPIFFS
  ESP_LOGI("MAIN", "Mounting SPIFFS on /audio");
  new SPIFFS("/audio");

  // [3] INITIALIZE I2S
  ESP_LOGI("MAIN", "Creating microphone");
  I2SSampler *input = new I2SMEMSSampler(I2S_NUM_0, i2s_mic_pins, i2s_mic_Config);
  I2SOutput *output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);


  // [4] - INITIALIZE AUDIO VARIABLES
  int audio_index = 0;  // Selected audio file
  bool select_bool[] = { true, false, false, false, false, false, false, false }; // Display selected row

  // Audio files
  char *audio_path[] = { 
    "/audio/rec0.wav", 
    "/audio/rec1.wav",   
    "/audio/rec2.wav",
    "/audio/rec3.wav",
    "/audio/rec4.wav",
    "/audio/rec5.wav",
    "/audio/rec6.wav",
    "/audio/rec7.wav"
  };

  // Names of audio files
  char *audio_name[] = { 
    " [0] - rec0.wav ", 
    " [1] - rec1.wav ",   
    " [2] - rec2.wav ",
    " [3] - rec3.wav ",
    " [4] - rec4.wav ",
    " [5] - rec5.wav ",
    " [6] - rec6.wav ",
    " [7] - rec7.wav ",
  };


  // [5] MAIN LOOP
  while (true) {
    ESP_LOGI("MAIN", "Waiting for button push");

    // Show display
    display(&dev, select_bool, audio_name);

    // Wait for button push
    while (gpio_get_level(GPIO_BUTTON_LEFT) == 0 && gpio_get_level(GPIO_BUTTON_RIGHT) == 0 && gpio_get_level(GPIO_BUTTON_PLAY) == 0 && gpio_get_level(GPIO_BUTTON_RECORD) == 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    // play audio file
    if (gpio_get_level(GPIO_BUTTON_PLAY) == 1) {
      play_audio(output, audio_path[audio_index]);
    }

    // record audio file
    if (gpio_get_level(GPIO_BUTTON_RECORD) == 1) {
      record_audio(input, audio_path[audio_index]);
    }

    // Left/Up button logic
    if (gpio_get_level(GPIO_BUTTON_RIGHT) == 1) {\
      btn_up(&audio_index, select_bool, 0, 7);
    }

    // Right/Down button logic
    if (gpio_get_level(GPIO_BUTTON_LEFT) == 1) {
      btn_down(&audio_index, select_bool, 0, 7);
    }

  vTaskDelay(pdMS_TO_TICKS(100));
  }
}