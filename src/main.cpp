#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include "Button.hpp"

/* TODO
 *  - Implement automatic deep sleep when no audio is playing for a while
 *  - Implement display communication
 *  - Implement display interface for metadata
 *  - Implement state sound effects (connected, disconnected, deep sleep)
 *  - Implement BLE service for battery level
 *  - Maybe try to add URL streaming support
 */


using namespace audio_tools;

constexpr uint8_t I2S_BCK = D11;    /* Audio data bit clock */
constexpr uint8_t I2S_LRC = D12;    /* Audio data left and right clock */
constexpr uint8_t I2S_DIN = D10;    /* ESP32 audio data output (to speakers) */
constexpr uint8_t BAT_VOLT = A4;    /* Battery voltage measurement */
constexpr uint8_t BUT_LEFT = D6;    /* Left button */
constexpr uint8_t BUT_RIGHT = D5;   /* Right button */
constexpr uint8_t BUT_CENTER = D7;  /* Center button */

constexpr auto META_FLAGS = ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                            ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME;

float batteryVoltage = NAN;
struct {
    esp_avrc_playback_stat_t playing = ESP_AVRC_PLAYBACK_STOPPED;
    String title = "Unknown";
    String artist = "Unknown";
    String album = "Unknown";
    uint32_t playtime = 0;
    uint32_t position = 0;
    uint8_t volume = 0;
} meta{};

I2SStream out{};
BluetoothA2DPSink bt{out};

static void increaseVolume();
static void nextTrack();
static void decreaseVolume();
static void previousTrack();
static void changePlayState();
static void enterPairingMode();

Button left{BUT_LEFT, decreaseVolume, previousTrack};
Button right{BUT_RIGHT, increaseVolume, nextTrack};
Button center{BUT_CENTER, changePlayState, enterPairingMode};

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
    bt.set_avrc_rn_volumechange([](int volume) { meta.volume = static_cast<uint8_t>(volume); });
    bt.set_avrc_rn_play_pos_callback([](uint32_t pos) { meta.position = pos; });
    bt.set_avrc_rn_playstatus_callback([](esp_avrc_playback_stat_t status) { meta.playing = status; });
    bt.set_on_connection_state_changed(connectionStateChangedCallback);
    bt.set_mono_downmix(true);
    bt.start("ESP32 Speaker", true);
}


void loop() {
    measureBattery();

    left.loop();
    right.loop();
    center.loop();

    if (static auto last = millis(); millis() - last > 2000) {
        last = millis();
        log_i("Metadata:"
              "\nBattery voltage: %.3f V"
              "\nPlaying: %s"
              "\nTitle: %s"
              "\nArtist: %s"
              "\nAlbum: %s"
              "\nPlaytime: %lu"
              "\nPosition: %lu"
              "\nVolume: %d",
              batteryVoltage,
              meta.playing == ESP_AVRC_PLAYBACK_PLAYING ? "true" : "false",
              meta.title.c_str(), meta.artist.c_str(), meta.album.c_str(),
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
    const auto string = reinterpret_cast<const char *>(data);
    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            meta.title = String(string);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            meta.artist = String(string);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            meta.album = String(string);
            break;
        case ESP_AVRC_MD_ATTR_PLAYING_TIME:
            meta.playtime = strtol(string, nullptr, 10);
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
    uint8_t volume = static_cast<uint8_t>(bt.get_volume()) + 4;
    bt.set_volume(volume);
    meta.volume = volume;
}

static void nextTrack() {
    log_i("Next track");
    bt.next();
}

static void decreaseVolume() {
    log_i("Decrease volume");
    uint8_t volume = static_cast<uint8_t>(bt.get_volume()) - 4;
    bt.set_volume(volume);
    meta.volume = volume;
}

static void previousTrack() {
    log_i("Previous track");
    bt.previous();
}

static void changePlayState() {
    log_i("Change play state");
    switch (meta.playing) {
        case ESP_AVRC_PLAYBACK_PAUSED:
        case ESP_AVRC_PLAYBACK_STOPPED:
            bt.play();
            break;
        case ESP_AVRC_PLAYBACK_PLAYING:
            bt.pause();
            break;
        default:
            break;
    }
}

static void enterPairingMode() {
    log_i("Enter pairing mode");
    bt.disconnect();
}
