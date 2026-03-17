#include "ControlManager.h"

namespace {
constexpr size_t QueueSize = 64;
}

ControlManager CtrlMgr;

ControlManager::ControlManager() : commandQueue(nullptr) {}

void ControlManager::begin() {
  if (commandQueue == nullptr) {
    commandQueue = xQueueCreate(QueueSize, sizeof(IPCCommand));
  }
}

bool ControlManager::sendCommand(const IPCCommand &cmd) {
  if (commandQueue == nullptr) {
    return false;
  }
  return xQueueSend(commandQueue, &cmd, 0) == pdTRUE;
}
