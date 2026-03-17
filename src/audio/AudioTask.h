#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include "../Config.h"
#include "../audio/Engine.h"
#include <Arduino.h>
#include <driver/i2s.h>


// Global sync flags used by ISR
extern std::atomic<bool> tickPending;

void audioTask(void *parameter);
void onTimerISR(); // ISR

#endif
