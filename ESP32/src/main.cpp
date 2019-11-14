#include <Arduino.h>
#include "driver/rtc_io.h"
#include "esp_camera.h"

#define LOGLEVEL 6

#include "Logging.h"
#include "wbutton.h"
#include "setup_ap.h"
#include "transmit.h"

#define LED_PIN    GPIO_NUM_4
#define CAMERA_PIN GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_7

//WROVER-KIT PIN Map
#define CAM_PIN_PWDN    -1 //power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    21
#define CAM_PIN_SIOD    26   //sda
#define CAM_PIN_SIOC    27   //scl

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39  //svn
#define CAM_PIN_D4      36  //svp
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22 //dclk red module

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1 //if more than one, i2s runs in continuous mode. Use only with JPEG
};


void setup()
{
    
    gpio_pad_select_gpio(GPIO_NUM_12);
    gpio_set_direction(GPIO_NUM_12, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_FLOATING);

    gpio_pad_select_gpio(GPIO_NUM_14);
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_14, GPIO_FLOATING);

    for (int i=32; i<40; i++) {
        gpio_pad_select_gpio(i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_INPUT);
        if (i < 34) gpio_set_pull_mode((gpio_num_t)i, GPIO_FLOATING);
    }

    esp_sleep_enable_timer_wakeup(60 * 1000000);
    esp_deep_sleep_disable_rom_logging();
    esp_deep_sleep_start();
}

void loop()
{ }