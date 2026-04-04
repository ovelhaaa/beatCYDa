#include "ControlManager.h"
#include "../storage/PatternStorage.h"
#include "../ui/UiAction.h"

namespace {
constexpr size_t QueueSize = 64;
constexpr size_t StorageQueueSize = 8;
constexpr size_t StorageEventQueueSize = 8;
}

ControlManager CtrlMgr;

ControlManager::ControlManager()
    : commandQueue(nullptr), storageQueue(nullptr), storageEventQueue(nullptr) {}

void ControlManager::begin() {
  if (commandQueue == nullptr) {
    commandQueue = xQueueCreate(QueueSize, sizeof(IPCCommand));
  }
  if (storageQueue == nullptr) {
    storageQueue = xQueueCreate(StorageQueueSize, sizeof(IPCCommand));
  }
  if (storageEventQueue == nullptr) {
    storageEventQueue = xQueueCreate(StorageEventQueueSize, sizeof(StorageEvent));
  }
  static bool workerStarted = false;
  if (!workerStarted && storageQueue != nullptr) {
    xTaskCreatePinnedToCore(storageWorkerTask, "CYD_Storage", 4096, this, 3, nullptr, 0);
    workerStarted = true;
  }
}

bool ControlManager::sendCommand(const IPCCommand &cmd) {
  if (commandQueue == nullptr || storageQueue == nullptr) {
    return false;
  }

  if (cmd.type == CommandType::UI_ACTION) {
    const UiActionType action = static_cast<UiActionType>(cmd.voiceId);
    if (action == UiActionType::SAVE_SLOT || action == UiActionType::LOAD_SLOT) {
      return xQueueSend(storageQueue, &cmd, 0) == pdTRUE;
    }
  }

  return xQueueSend(commandQueue, &cmd, 0) == pdTRUE;
}

bool ControlManager::pollStorageEvent(StorageEvent &event) {
  if (storageEventQueue == nullptr) {
    return false;
  }
  return xQueueReceive(storageEventQueue, &event, 0) == pdTRUE;
}

void ControlManager::storageWorkerTask(void *parameter) {
  auto *self = static_cast<ControlManager *>(parameter);
  IPCCommand cmd{};

  while (true) {
    if (xQueueReceive(self->storageQueue, &cmd, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    const UiActionType action = static_cast<UiActionType>(cmd.voiceId);
    const uint8_t slot = cmd.paramId;
    StorageEvent event{};
    event.slot = slot;

    if (action == UiActionType::SAVE_SLOT) {
      event.type = StorageEventType::SAVE_SLOT_DONE;
      event.success = PatternStore.saveSlot(slot);
    } else if (action == UiActionType::LOAD_SLOT) {
      event.type = StorageEventType::LOAD_SLOT_DONE;
      event.success = PatternStore.loadSlot(slot);
    } else {
      continue;
    }

    if (self->storageEventQueue != nullptr) {
      xQueueSend(self->storageEventQueue, &event, 0);
    }
  }
}
