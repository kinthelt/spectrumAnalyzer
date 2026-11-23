#include "AudioInput.h"

#include <Arduino.h>

#include "../util/Log.h"

bool AudioInput::setupI2s() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 4;
  chanCfg.dma_frame_num = config::kAudioBlockSize;   // one block per DMA buffer
  if (i2s_new_channel(&chanCfg, nullptr, &rx_) != ESP_OK) return false;

  i2s_std_config_t cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config::kSampleRate),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = static_cast<gpio_num_t>(config::kMicBclkPin),
              .ws = static_cast<gpio_num_t>(config::kMicWsPin),
              .dout = I2S_GPIO_UNUSED,
              .din = static_cast<gpio_num_t>(config::kMicDataPin),
              .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
          },
  };
  cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   // mic SEL tied to GND = left channel

  if (i2s_channel_init_std_mode(rx_, &cfg) != ESP_OK) return false;
  return i2s_channel_enable(rx_) == ESP_OK;
}

bool AudioInput::begin() {
  if (!bank_.begin(config::kSampleRate, config::kBandLowHz, config::kBandHighHz,
                   config::kBandQScale, config::kMinBandwidthHz)) {
    LOG_ERROR("audio", "filter bank settings need more decimation stages than the block allows");
    return false;
  }
  float centres[FilterBank::kBands];
  for (int i = 0; i < FilterBank::kBands; ++i) centres[i] = bank_.centreHz(i);
  tracker_.begin(centres);

  queue_ = xQueueCreate(1, sizeof(AudioFrame));
  if (queue_ == nullptr) {
    LOG_ERROR("audio", "could not create frame queue");
    return false;
  }
  if (!setupI2s()) {
    LOG_ERROR("audio", "I2S setup failed - check mic wiring and pins in Config.h");
    return false;
  }
  if (xTaskCreatePinnedToCore(taskEntry, "audio", config::kAudioTaskStackBytes, this,
                              config::kAudioTaskPriority, nullptr,
                              config::kAudioTaskCore) != pdPASS) {
    LOG_ERROR("audio", "could not start audio task");
    return false;
  }

  LOG_INFO("audio", "%d bands %.0f-%.0f Hz, %d stages, %.2fM band-samples/s, mic on GPIO %d/%d/%d",
           FilterBank::kBands, bank_.centreHz(0), bank_.centreHz(FilterBank::kBands - 1),
           bank_.stageCount(), bank_.bandSamplesPerSecond() / 1e6, config::kMicBclkPin,
           config::kMicWsPin, config::kMicDataPin);
  return true;
}

bool AudioInput::latest(AudioFrame* out) {
  return queue_ != nullptr && xQueueReceive(queue_, out, 0) == pdTRUE;
}

void AudioInput::taskEntry(void* self) { static_cast<AudioInput*>(self)->run(); }

void AudioInput::run() {
  constexpr float kBlockSeconds = static_cast<float>(config::kAudioBlockSize) / config::kSampleRate;
  // The mic sends 24-bit samples left-aligned in 32-bit words; scale to +/-1.0.
  constexpr float kScale = 1.0f / 2147483648.0f;

  for (;;) {
    size_t bytesRead = 0;
    // Blocks until a full block has arrived, which paces this task.
    const esp_err_t err = i2s_channel_read(rx_, raw_, sizeof(raw_), &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead != sizeof(raw_)) {
      ++readErrors_;
      continue;
    }

    const uint32_t start = micros();
    for (int i = 0; i < config::kAudioBlockSize; ++i) samples_[i] = raw_[i] * kScale;
    bank_.process(samples_);

    float power[FilterBank::kBands];
    for (int i = 0; i < FilterBank::kBands; ++i) power[i] = bank_.power(i);
    if (learnRequested_.exchange(false)) tracker_.learnNoise();
    tracker_.update(power, kBlockSeconds, &frame_);

    frame_.dspMicros = micros() - start;
    ++frame_.sequence;
    xQueueOverwrite(queue_, &frame_);
  }
}
