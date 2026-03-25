#include "AudioTask.h"

#include "../control/ControlManager.h"
#include "VoiceManager.h"
#include <math.h>

std::atomic<bool> tickPending(false);
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

namespace {
bool setupAudioOutput() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = BUFFER_LEN;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;

  if (CYDConfig::UseInternalDac) {
    config.mode = static_cast<i2s_mode_t>(config.mode | I2S_MODE_DAC_BUILT_IN);
  }

  esp_err_t err = i2s_driver_install(CYDConfig::AudioPort, &config, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("I2S install failed: %d\n", err);
    return false;
  }

  if (CYDConfig::UseInternalDac) {
    i2s_set_pin(CYDConfig::AudioPort, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
  } else {
    i2s_pin_config_t pinConfig = {};
    pinConfig.bck_io_num = CYDConfig::ExtI2SBck;
    pinConfig.ws_io_num = CYDConfig::ExtI2SWs;
    pinConfig.data_out_num = CYDConfig::ExtI2SData;
    pinConfig.data_in_num = I2S_PIN_NO_CHANGE;
    err = i2s_set_pin(CYDConfig::AudioPort, &pinConfig);
    if (err != ESP_OK) {
      Serial.printf("I2S pin config failed: %d\n", err);
      return false;
    }
  }

  i2s_zero_dma_buffer(CYDConfig::AudioPort);
  return true;
}
} // namespace

void IRAM_ATTR onTimerISR() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (engine.isPlaying.load(std::memory_order_relaxed)) {
    tickPending.store(true, std::memory_order_release);
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

void audioTask(void *parameter) {
  if (!setupAudioOutput()) {
    vTaskDelete(nullptr);
    return;
  }

  const size_t frameWords = BUFFER_LEN * 2;
  uint16_t *sampleBuffer = static_cast<uint16_t *>(
      heap_caps_malloc(frameWords * sizeof(uint16_t), MALLOC_CAP_DMA));
  if (sampleBuffer == nullptr) {
    Serial.println("Failed to allocate audio DMA buffer");
    vTaskDelete(nullptr);
    return;
  }

  size_t bytesWritten = 0;

  while (true) {
    voiceManager.syncParams();

    const float bufferDurationMs =
        (static_cast<float>(BUFFER_LEN) / static_cast<float>(SAMPLE_RATE)) * 1000.0f;
    engine.bassGroove.process(bufferDurationMs);
    engine.midiOut.process(bufferDurationMs);

    if (tickPending.exchange(false, std::memory_order_acq_rel)) {
      if (engine.lockPattern()) {
        const int curStep = engine.currentStep.load(std::memory_order_relaxed);

        if (!engine.trackMutes[VOICE_BASS].load(std::memory_order_relaxed)) {
          engine.bassGroove.onTick(curStep);
        }

        for (int voice = 0; voice < TRACK_COUNT; ++voice) {
          const int len = engine.tracks[voice].patternLen;
          const bool isMuted = engine.trackMutes[voice].load(std::memory_order_relaxed);
          if (len <= 0 || isMuted) {
            continue;
          }

          const uint8_t patVal = engine.tracks[voice].pattern[curStep % len];
          if (patVal == 0) {
            continue;
          }

          const float velocity =
              (patVal == 1) ? 0.9f : (static_cast<float>(patVal) / 127.0f);

          if (voice == VOICE_KICK) {
            engine.bassGroove.onKick();
            engine.midiOut.triggerDrum(VOICE_KICK, velocity);
            voiceManager.trigger(VOICE_KICK, velocity);
          } else if (voice != VOICE_BASS) {
            engine.midiOut.triggerDrum(static_cast<VoiceID>(voice), velocity);
            voiceManager.trigger(static_cast<VoiceID>(voice), velocity);
          }
        }

        engine.currentStep.store((curStep + 1) % 64, std::memory_order_relaxed);
        engine.unlockPattern();
      }
    }

    QueueHandle_t cmdQueue = CtrlMgr.getCommandQueue();
    if (cmdQueue != nullptr) {
      IPCCommand cmd;
      while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
        case CommandType::NOTE_ON:
          if (cmd.voiceId < VOICE_COUNT) {
            const VoiceID voice = static_cast<VoiceID>(cmd.voiceId);
            if (voice == VOICE_BASS) {
              const int midiNote = constrain(static_cast<int>(lroundf(cmd.value)), 0, 127);
              const float freq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
              const float gateMs =
                  120.0f + (voiceManager.getParams(VOICE_BASS).decay * 480.0f);
              voiceManager.triggerFreq(VOICE_BASS, freq, cmd.auxValue);
              engine.midiOut.triggerBass(static_cast<uint8_t>(midiNote), cmd.auxValue,
                                         gateMs);
            } else {
              if (voice == VOICE_KICK) {
                engine.bassGroove.onKick();
              }
              engine.midiOut.triggerDrum(voice, cmd.auxValue);
              voiceManager.trigger(voice, cmd.auxValue);
            }
          }
          break;
        case CommandType::NOTE_OFF:
          if (cmd.voiceId < VOICE_COUNT) {
            engine.midiOut.noteOffVoice(static_cast<VoiceID>(cmd.voiceId));
          }
          break;
        case CommandType::SET_PARAM:
          voiceManager.setParam(static_cast<VoiceID>(cmd.voiceId),
                                static_cast<VoiceParam>(cmd.paramId), cmd.value);
          break;
        case CommandType::TRANSPORT:
          if (cmd.value > 0.5f) {
            engine.play();
          } else {
            engine.stop();
          }
          break;
        case CommandType::UI_ACTION:
          engine.handleUiAction({static_cast<UiActionType>(cmd.voiceId), cmd.paramId,
                                 static_cast<int>(cmd.value)});
          break;
        default:
          break;
        }
      }
    }

    static int ditherIdx = 0;
    for (int i = 0; i < BUFFER_LEN; ++i) {
      float mix = voiceManager.process();
      mix = fastSoftClip(mix * 0.9f);

      const float dither = ditherLUT[ditherIdx];
      ditherIdx = (ditherIdx + 1) & LUT_MASK;
      const float sample = constrain(mix + dither, -1.0f, 1.0f);

      if (CYDConfig::UseInternalDac) {
        const uint16_t dacWord =
            static_cast<uint16_t>((sample * 127.0f) + 128.0f) << 8;
        sampleBuffer[i * 2] = dacWord;
        sampleBuffer[(i * 2) + 1] = dacWord;
      } else {
        int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);
        sampleBuffer[i * 2] = static_cast<uint16_t>(pcmSample);
        sampleBuffer[(i * 2) + 1] = static_cast<uint16_t>(pcmSample);
      }
    }

    const esp_err_t err = i2s_write(CYDConfig::AudioPort, sampleBuffer,
                                    frameWords * sizeof(uint16_t), &bytesWritten, 1000);
    if (err != ESP_OK || bytesWritten == 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      vTaskDelay(1);
    }
  }
}
