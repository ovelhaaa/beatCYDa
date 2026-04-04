#pragma once

#include "../../include/IPC.h"
#include <Arduino.h>

class ControlManager {
public:
  enum class StorageEventType : uint8_t {
    SAVE_SLOT_DONE,
    LOAD_SLOT_DONE,
  };

  struct StorageEvent {
    StorageEventType type;
    uint8_t slot;
    bool success;
  };

  ControlManager();

  void begin();
  bool sendCommand(const IPCCommand &cmd);
  bool pollStorageEvent(StorageEvent &event);
  QueueHandle_t getCommandQueue() const { return commandQueue; }

private:
  static void storageWorkerTask(void *parameter);

  QueueHandle_t commandQueue;
  QueueHandle_t storageQueue;
  QueueHandle_t storageEventQueue;
};

extern ControlManager CtrlMgr;
