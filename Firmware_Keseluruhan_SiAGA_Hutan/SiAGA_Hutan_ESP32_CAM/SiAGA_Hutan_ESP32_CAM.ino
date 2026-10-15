/**
 * ====================================================================================================
 * PENGKABELAN KE ESP32-S3 MASTER:
 *   ESP32-CAM U0RXD (RX) <--- ESP32-S3 GPIO 17 (TX2)
 *   ESP32-CAM U0TXD (TX) ---> ESP32-S3 GPIO 18 (RX2)
 *   ESP32-CAM 5V / GND   <--- Suplai 5V Stabil & Common Ground
 * ====================================================================================================
 */

#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"

// Pinout Kamera AI-THINKER ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Pin Flash LED Onboard
#define PIN_FLASH_LED      4

// Status Penyimpanan SD Card
bool sdCardAvailable = false;
uint32_t photoIndex = 0;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Optimasi Resolusi & Frame Buffer
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;  // 640x480 (Ideal untuk bukti visual kebakaran)
    config.jpeg_quality = 12;             // 10-63 (Lebih kecil = kualitas lebih tinggi)
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 15;
    config.fb_count     = 1;
  }

  // Inisialisasi Driver Kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return false;
  }

  // Kalibrasi Khusus Sensor OV3660 (Auto-Tuning Orientasi & Keseimbangan Warna)
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);       // Balik vertikal (mounting lensa OV3660 terbalik secara default)
      s->set_brightness(s, 1);   // Sedikit kompensasi eksposur kecerahan (+1)
      s->set_saturation(s, -2);  // Koreksi saturasi warna bawaan OV3660 agar tampak alami
    }
  }

  return true;
}

void initSDCard() {
  if (SD_MMC.begin("/sdcard", true)) { // 1-bit mode untuk hemat pin
    sdCardAvailable = true;
  } else {
    sdCardAvailable = false;
  }
}

void captureAndProcess() {
  // Nyalakan flash sejenak jika diperlukan
  digitalWrite(PIN_FLASH_LED, HIGH);
  delay(50);

  camera_fb_t *fb = esp_camera_fb_get();
  digitalWrite(PIN_FLASH_LED, LOW);

  if (!fb) {
    Serial.println(F("ACK_ERR:CAPTURE_FAILED"));
    return;
  }

  size_t photoSize = fb->len;
  photoIndex++;

  // Simpan ke SD Card jika modul terpasang
  if (sdCardAvailable) {
    char path[32];
    snprintf(path, sizeof(path), "/fire_alert_%04lu.jpg", (unsigned long)photoIndex);
    fs::FS &fs = SD_MMC;
    File file = fs.open(path, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
    }
  }

  // Kirim konfirmasi ukuran ke ESP32-S3 Master
  Serial.printf("ACK_OK:SIZE=%u:SD=%s:IDX=%lu\n", 
                photoSize, 
                sdCardAvailable ? "SAVED" : "NO_SD", 
                (unsigned long)photoIndex);

  esp_camera_fb_return(fb);
}

void setup() {
  // UART0 terhubung ke GPIO 17 & 18 ESP32-S3
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_FLASH_LED, OUTPUT);
  digitalWrite(PIN_FLASH_LED, LOW);

  bool camOk = initCamera();
  initSDCard();

  if (camOk) {
    sensor_t *s = esp_camera_sensor_get();
    if (s && s->id.PID == OV3660_PID) {
      Serial.println(F("PONG:CAM_READY:OV3660_3MP"));
    } else {
      Serial.println(F("PONG:CAM_READY:OV2640_2MP"));
    }
  } else {
    Serial.println(F("ACK_ERR:CAM_INIT_FAILED"));
  }
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "SNAP_SHOT") {
      captureAndProcess();
    } else if (cmd == "FLASH_ON") {
      digitalWrite(PIN_FLASH_LED, HIGH);
      Serial.println(F("ACK_OK:FLASH_ON"));
    } else if (cmd == "FLASH_OFF") {
      digitalWrite(PIN_FLASH_LED, LOW);
      Serial.println(F("ACK_OK:FLASH_OFF"));
    } else if (cmd == "PING") {
      Serial.println(F("PONG:CAM_READY"));
    }
  }
}
