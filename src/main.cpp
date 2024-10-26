#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include "Button.hpp"

/* TODO
 *  - Implement display communication
 *  - Implement display interface
 *  - Implement state sound effects
 *  - Implement BLE service for battery level
 *  - Maybe try to add URL streaming support
 */


using namespace audio_tools;

constexpr uint8_t I2S_BCK = D11;    /* Audio data bit clock */
constexpr uint8_t I2S_LRC = D12;    /* Audio data left and right clock */
constexpr uint8_t I2S_DIN = D10;    /* ESP32 audio data output (to speakers) */
constexpr uint8_t BAT_VOLT = A4;    /* Battery voltage measurement */
constexpr uint8_t BUT_LEFT = D7;    /* Left button */
constexpr uint8_t BUT_RIGHT = D5;   /* Right button */
constexpr uint8_t BUT_CENTER = D6;  /* Center button */

constexpr auto META_FLAGS = ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                            ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME;

float batteryVoltage{NAN};
struct {
    char title[64] = "Unknown";
    char artist[64] = "Unknown";
    char album[64] = "Unknown";
    uint32_t playtime = 0;
    uint32_t position = 0;
    int volume = 0;
} meta;

audio_tools::I2SStream out{};
BluetoothA2DPSink bt{out};

static void increaseVolume();
static void nextTrack();
static void decreaseVolume();
static void previousTrack();
static void changePlayState();
static void enterPairingMode();

Button left{BUT_LEFT, increaseVolume, nextTrack};
Button center{BUT_CENTER, decreaseVolume, previousTrack};
Button right{BUT_RIGHT, changePlayState, enterPairingMode};

static void measureBattery();
static void metadataCallback(uint8_t id, const uint8_t *data);
static void connectionStateChangedCallback(esp_a2d_connection_state_t state, void *);


void setup() {
    Serial.begin(115200);
    pinMode(BAT_VOLT, INPUT);
    analogSetAttenuation(ADC_0db);

    left.setup();
    right.setup();
    center.setup();

    I2SConfig cfg{TX_MODE};
    cfg.pin_data = I2S_DIN;
    cfg.pin_bck = I2S_BCK;
    cfg.pin_ws = I2S_LRC;
    cfg.i2s_format = I2S_LSB_FORMAT;
    cfg.buffer_count = 8;
    cfg.buffer_size = 1024;
    out.begin(cfg);
    bt.set_avrc_metadata_attribute_mask(META_FLAGS);
    bt.set_avrc_metadata_callback(metadataCallback);
    bt.set_avrc_rn_volumechange([](int volume) { meta.volume = volume; });
    bt.set_avrc_rn_play_pos_callback([](uint32_t pos) { meta.position = pos; });
    bt.set_on_connection_state_changed(connectionStateChangedCallback);
    bt.set_mono_downmix(true);
    bt.start("BTMini", true);
}


void loop() {
    measureBattery();

    left.loop();
    right.loop();
    center.loop();

    if (static uint32_t last = 0; millis() - last > 2000) {
        last = millis();
        log_i("Meta info:\n"
              "Battery voltage: %.3f V\n"
              "Title: %s\n"
              "Artist: %s\n"
              "Album: %s\n"
              "Playtime: %lu\n"
              "Position: %lu\n"
              "Volume: %d",
              batteryVoltage,
              meta.title, meta.artist, meta.album,
              meta.playtime, meta.position, meta.volume);
    }
}


static void measureBattery() {
    constexpr auto factor = 6.9f / (22.0f + 6.9f); // Voltage divider factor
    constexpr auto N = 10000;
    static uint32_t samples[N]{};
    static uint32_t i = 0;
    samples[i] = analogReadMilliVolts(BAT_VOLT);
    if (++i == N) {
        i = 0;
        auto sum = 0.0f;
        for (auto sample: samples) {
            sum += static_cast<float>(sample);
        }
        batteryVoltage = sum / N / factor / 1000.0f;
    }
}

static void metadataCallback(uint8_t id, const uint8_t *data) {
    const auto str = String(reinterpret_cast<const char *>(data));
    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            str.toCharArray(meta.title, sizeof(meta.title));
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            str.toCharArray(meta.artist, sizeof(meta.artist));
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            str.toCharArray(meta.album, sizeof(meta.album));
            break;
        case ESP_AVRC_MD_ATTR_PLAYING_TIME:
            meta.playtime = str.toInt();
            break;
        default:
            break;
    }
}

static void connectionStateChangedCallback(esp_a2d_connection_state_t state, void *) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED: {
            log_i("A2DP connected");
            // TODO: Play connected sound
            break;
        }
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED: {
            log_w("A2DP disconnected");
            // TODO: Play disconnected sound
            break;
        }
        default:
            break;
    }
}

static void increaseVolume() {
    log_i("Increase volume");
    bt.set_volume(static_cast<uint8_t>(bt.get_volume()) + 1);
}

static void nextTrack() {
    log_i("Next track");
    bt.next();
}

static void decreaseVolume() {
    log_i("Decrease volume");
    bt.set_volume(static_cast<uint8_t>(bt.get_volume()) - 1);
}

static void previousTrack() {
    log_i("Previous track");
    bt.previous();
}

static void changePlayState() {
    log_i("Change play state");
    bt.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED ? bt.pause() : bt.play();
}

static void enterPairingMode() {
    log_i("Enter pairing mode");
    bt.disconnect();
}
