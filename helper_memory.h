#ifndef HELPER_MEMORY_H
#define HELPER_MEMORY_H

#include <Arduino.h>

struct MemorySnapshot {
  uint32_t freeHeap;
  uint16_t maxFreeBlock;
  uint8_t fragmentation;
};

inline MemorySnapshot captureMemorySnapshot() {
  MemorySnapshot snapshot;
  snapshot.freeHeap = ESP.getFreeHeap();
  snapshot.maxFreeBlock = ESP.getMaxFreeBlockSize();
  snapshot.fragmentation = ESP.getHeapFragmentation();
  return snapshot;
}

inline void logMemorySnapshot(const char* label, const MemorySnapshot& snapshot) {
  Serial.printf("[MEM] %s: free=%lu B, largest=%u B, fragmentation=%u%%\n",
                label,
                static_cast<unsigned long>(snapshot.freeHeap),
                snapshot.maxFreeBlock,
                snapshot.fragmentation);
}

inline void logMemoryDelta(const MemorySnapshot& before, const MemorySnapshot& after) {
  const int32_t freeHeapDelta =
      static_cast<int32_t>(after.freeHeap) - static_cast<int32_t>(before.freeHeap);
  const int32_t maxBlockDelta =
      static_cast<int32_t>(after.maxFreeBlock) - static_cast<int32_t>(before.maxFreeBlock);

  Serial.printf("[MEM] Price update delta: free=%ld B, largest=%ld B, fragmentation=%u%% -> %u%%\n",
                static_cast<long>(freeHeapDelta),
                static_cast<long>(maxBlockDelta),
                before.fragmentation,
                after.fragmentation);
}

#endif
