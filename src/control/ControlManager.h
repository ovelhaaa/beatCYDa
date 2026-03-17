#pragma once

#include "../../include/IPC.h"
#include <Arduino.h>

class ControlManager {
public:
  ControlManager();

  void begin();
  bool sendCommand(const IPCCommand &cmd);
  QueueHandle_t getCommandQueue() const { return commandQueue; }

private:
  QueueHandle_t commandQueue;
};

extern ControlManager CtrlMgr;
