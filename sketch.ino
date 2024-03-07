#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

// Prometheus endpoint configuration
const char* prometheusEndpoint = "http://192.168.1.232:9091/metrics";
const char* metricName = "m5stick_decibel_level";

// Wi-Fi configuration
const char* ssid = "asdf";
const char* password = "asdf";

// I2S configuration
#define PIN_CLK     0
#define PIN_DATA    34
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int SAMPLE_RATE = 44100;
const int SAMPLE_BUFFER_SIZE = 1024;

int16_t sampleBuffer[SAMPLE_BUFFER_SIZE];

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 0)
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#else
        .communication_format = I2S_COMM_FORMAT_I2S,
#endif
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = 128,
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE,
    .ws_io_num = PIN_CLK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_DATA
  };

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
#endif

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

void sendToPrometheus(float decibelLevel) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String metricData = String(metricName) + " " + String(decibelLevel);
    
    http.begin(prometheusEndpoint);
    http.addHeader("Content-Type", "text/plain");
    
    int httpResponseCode = http.POST(metricData);
    if (httpResponseCode > 0) {
      Serial.printf("Sent to Prometheus. Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error sending to Prometheus. Error code: %d\n", httpResponseCode);
    }
    
    http.end();
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");
  
  setupI2S();
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, sampleBuffer, SAMPLE_BUFFER_SIZE, &bytesRead, portMAX_DELAY);

  float sum = 0;
  for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
    sum += abs(sampleBuffer[i]);
  }

  float average = sum / SAMPLE_BUFFER_SIZE;
  float decibelLevel = 20 * log10(average / 32767.0) + 94;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("Decibel: %.2f dB", decibelLevel);

  Serial.printf("Decibel level: %.2f dB\n", decibelLevel);

  sendToPrometheus(decibelLevel);

  delay(1000);
}
