#include <sese_inferencing.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_bt.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* BOT_TOKEN = "7878604642:AAGIc5-fucEg4Pu8bN1uBjn1UMnpScE-ais";
const char* CHAT_ID = "5940526891";
const char* TELEGRAM_API_URL = "https://api.telegram.org/bot";

//ESP and I2S Driver Setup
const i2s_port_t I2S_PORT = I2S_NUM_0;
esp_err_t err;

#define I2S_SAMPLE_RATE (16000)

#define SENSOR_CHANNEL ADC1_CHANNEL_6 // KY-039 sensörünün bağlı olduğu GPIO34


// Wi-Fi bağlantı bilgileri
const char* ssid = "Ali SARIMERMER";
const char* password = "gltl1824";
const char* serverURL = "http://your-server.com/data"; // Veri göndereceğiniz sunucu URL'si

unsigned long myTimerStart = 0; // Timer başlangıç zamanı
bool timerRunning = false;      // Timer çalışıyor mu?
int SOS_Counter=0; 
int buzzerPin = 33;
int LED = 19; 
int rLED = 23; 
int lastRec = 0; 
int count = 0;


// Telegram mesaj gönderimi
void sendTelegramMessage(const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(TELEGRAM_API_URL) + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + message;

        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            Serial.println("Telegram mesajı gönderildi.");
        } else {
            Serial.println("Telegram mesajı gönderilemedi.");
        }

        http.end();
    } else {
        Serial.println("Wi-Fi bağlantısı yok.");
    }
}

// The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = I2S_SAMPLE_RATE,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // use right channel
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 8                              // 8 samples per buffer (minimum)
  };

// The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 26,   // Serial Clock (SCK)
      .ws_io_num = 14,    // Word Select (WS)
      .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
      .data_in_num = 32   // Serial Data (SD)
  };

int16_t sampleBuffer[3333]; 
int16_t features[3333];

int I2SRead(){
  size_t bytesRead;
 
  Serial.println(" * Recording Start *"); 
  int count = 0; 
  lastRec = millis(); 
  while(1){
    i2s_read(I2S_PORT, (void*) sampleBuffer, 4, &bytesRead, portMAX_DELAY);
    if(*sampleBuffer < (-40) || millis() - lastRec >= 6000){
      for(int i = 0;i < 3333; i++){
        i2s_read(I2S_PORT, (void*) sampleBuffer, 4, &bytesRead, portMAX_DELAY);
        features[i] = sampleBuffer[0]; 
       }
       break; 
    }
  }
  Serial.println(" * RECORDING ENDED * "); 
  return bytesRead; 
} 

#define SAMP_SIZ 4
#define RISE_THRESHOLD 5

float heartbeat() {
    float reads[SAMP_SIZ] = {0}; // Son ölçümleri tutan dizi
    float sum = 0;
    int ptr = 0; // Dizi göstergesi
    float last = 0, before = 0;
    bool rising = false; // Yükselen kenar algılama
    int rise_count = 0;
    long last_beat = 0; // Son kalp atışı zamanı
    float first = 0, second = 0, third = 0; // BPM için ölçüm geçmişi
    float print_value = -1; // Varsayılan BPM değeri
    int n;
    long now;

    for (int i = 0; i < 200; i++) { // 1000 ölçüm yap
        // ADC'den analog okuma ve ortalama alma
        n = 0;
        long start = millis();
        float reader = 0.0;

        while (millis() < start + 20) { // 20 ms boyunca ADC ölçümü
            reader += adc1_get_raw(SENSOR_CHANNEL);
            n++;
        }
        reader /= n; // Ortalama hesaplama

        // Son ölçüm değerlerini diziye ekle ve toplamı güncelle
        sum -= reads[ptr];
        sum += reader;
        reads[ptr] = reader;
        last = sum / SAMP_SIZ;

        // Yükselen kenar kontrolü
        if (last > before) {
            rise_count++;
            if (!rising && rise_count > RISE_THRESHOLD) {
                rising = true;
                long current_time = millis();
                if (last_beat > 0) {
                    first = current_time - last_beat;
                    print_value = 60000.0 / (0.4 * first + 0.3 * second + 0.3 * third);
                    Serial.printf("Heartbeat Detected: %.2f BPM\n", print_value);
                }
                last_beat = current_time;
                third = second;
                second = first;
            }
        } else {
            rising = false;
            rise_count = 0;
        }

        before = last; // Önceki değeri güncelle
        ptr = (ptr + 1) % SAMP_SIZ; // Dizi göstergeyi döngüsel hale getir
    }

    // Eğer herhangi bir atış algılanmadıysa -1 döner
    if (millis() - last_beat > 2000) {
        Serial.println("No heartbeat detected (timeout)");
        return -1;
    }

    return print_value; // Hesaplanan BPM değeri döner
}
int raw_get_data(size_t offset, size_t length, float *out_ptr) {
    return numpy::int16_to_float(features + offset, out_ptr, length);
}


void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);

    // Wi-Fi Bağlantısı
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }

    Serial.println("Wi-Fi connected!");
    Serial.println("IP Address: ");
    Serial.println(WiFi.localIP());



    Serial.println("Edge Impulse Inferencing Demo");

    err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);

    pinMode(33, OUTPUT); 
    pinMode(27, INPUT); 
    pinMode(25, OUTPUT); 
    pinMode(LED, OUTPUT); 
    pinMode(rLED, OUTPUT); 

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SENSOR_CHANNEL, ADC_ATTEN_DB_11); 

}


void loop()
{
    ei_printf("Edge Impulse standalone inferencing (Arduino)\n");
    
    float bpm = heartbeat();

    if((bpm<40 && bpm>10) || bpm > 5000){
        sendTelegramMessage("Bu bir acil durumdur! Kalp atışında anormallik tespit edildi.");
        digitalWrite(rLED, HIGH); 
        delay(100);
        digitalWrite(rLED, LOW);
    }

    //Record Audio
    int bytesRead = I2SRead();


    ei_impulse_result_t result = { 0 };

    // the features are stored into flash, and we don't want to load everything into RAM
    signal_t signal;
    signal.total_length = 3333; 
    signal.get_data = &raw_get_data; 


    // invoke the impulse
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
    ei_printf("run_classifier returned: %d\n", res);

    if (res != 0) return;

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    ei_printf("[");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("%.5f", result.classification[ix].value);
#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf(", ");
#else
        if (ix != EI_CLASSIFIER_LABEL_COUNT - 1) {
            ei_printf(", ");
        }
#endif
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("%.3f", result.anomaly);
#endif
    ei_printf("]\n");

    // human-readable predictions
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif


if (result.classification[1].value && result.classification[3].value > 0.85) {
    if (!timerRunning || SOS_Counter==1) {
        // Timer'ı başlat
        myTimerStart = millis();
        timerRunning = true;
    }
      
    if (millis() - myTimerStart <= 15000) { // 10 saniye içinde
        SOS_Counter++;

        digitalWrite(LED, HIGH); 
        delay(100);
        digitalWrite(LED, LOW);
        if (SOS_Counter >= 3) {
            // SOS durumu
            
            digitalWrite(buzzerPin, HIGH);
            delay(1000);
            sendTelegramMessage("Bu bir acil durumdur! Bir yardım isteği tespit edildi.");

             
            digitalWrite(buzzerPin, LOW);
            SOS_Counter = 0;   // Sayaç sıfırlanır
            timerRunning = false; // Timer durdurulur
        }
    } else {
        // 10 saniye geçti, SOS_Counter sıfırlanır
        SOS_Counter = 0;
        timerRunning = false; // Timer sıfırlanır
    }
}


}

/**
 * @brief      Printf function uses vsnprintf and output using Arduino Serial
 *
 * @param[in]  format     Variable argument list
 */
void ei_printf(const char *format, ...) {
    static char print_buf[1024] = { 0 };

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0) {
        Serial.write(print_buf);
    }
}