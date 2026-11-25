#include "AudioInput.h"

#include <Arduino.h>

#include "../util/Log.h"

bool AudioInput::setupI2s() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  // 8 blocks of buffering (~85 ms): rides out the ~55 ms core-0 stall seen
  // while Wi-Fi connects and Matter pairs (Wi-Fi/Matter spike).
  chanCfg.dma_desc_num = 8;
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

bool AudioInput::allocate(const StripSetup& setup) {
  if (!bank_.begin(setup.ledCount, config::kSampleRate, setup.lowHz, setup.highHz, config::kBandQScale,
                   config::kMinBandwidthHz)) {
    LOG_ERROR("audio", "filter bank for %u bands %.0f-%.0f Hz: out of memory or too many stages",
              (unsigned)setup.ledCount, setup.lowHz, setup.highHz);
    return false;
  }
  if (!tracker_.begin(setup.ledCount, bank_.centres()) || !mailbox_.begin(setup.ledCount)) {
    LOG_ERROR("audio", "out of memory for %u bands", (unsigned)setup.ledCount);
    return false;
  }
  return true;
}

bool AudioInput::start() {
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

  const int n = bank_.bandCount();
  LOG_INFO("audio", "%d bands %.0f-%.0f Hz, %d stages, %.2fM band-samples/s, mic on GPIO %d/%d/%d", n,
           bank_.centreHz(0), bank_.centreHz(n - 1), bank_.stageCount(), bank_.bandSamplesPerSecond() / 1e6,
           config::kMicBclkPin, config::kMicWsPin, config::kMicDataPin);
  return true;
}

void AudioInput::taskEntry(void* self) { static_cast<AudioInput*>(self)->run(); }

void AudioInput::run() {
  constexpr float kBlockSeconds = static_cast<float>(config::kAudioBlockSize) / config::kSampleRate;
  // The mic sends 24-bit samples left-aligned in 32-bit words; scale to +/-1.0.
  constexpr float kScale = 1.0f / 2147483648.0f;
  constexpr uint32_t kBlockMicros =
      static_cast<uint32_t>(1000000ULL * config::kAudioBlockSize / config::kSampleRate);
  constexpr uint32_t kLoadLimitMicros = static_cast<uint32_t>(kBlockMicros * config::kAudioLoadLimit);

  for (;;) {
    size_t bytesRead = 0;
    // Blocks until a full block has arrived, which paces this task.
    const esp_err_t err = i2s_channel_read(rx_, raw_, sizeof(raw_), &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead != sizeof(raw_)) {
      ++readErrors_;
      continue;
    }
    if (paused_.load()) continue;   // block read and dropped: costs almost nothing

    const uint32_t start = micros();
    for (int i = 0; i < config::kAudioBlockSize; ++i) samples_[i] = raw_[i] * kScale;
    bank_.process(samples_);

    if (learnRequested_.exchange(false)) tracker_.learnNoise();
    tracker_.update(bank_.powers(), kBlockSeconds, &frame_);

    frame_.dspMicros = micros() - start;
    ++frame_.sequence;
    mailbox_.publish(frame_, tracker_.levels());

    // Too many LEDs for the chip: let core 0's other work (and its idle task,
    // which the watchdog checks) run before the next block.
    if (frame_.dspMicros > kLoadLimitMicros) vTaskDelay(1);
  }
}
