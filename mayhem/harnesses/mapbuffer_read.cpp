// Fuzz React Native's MapBuffer binary format (react/renderer/mapbuffer): construct a MapBuffer
// from raw bytes (as the Android side does when it receives a serialized buffer) and read every
// bucket through the typed getters, recursing into nested maps and map lists.
#include <cstddef>
#include <cstdint>
#include <vector>

#include <react/renderer/mapbuffer/MapBuffer.h>

using facebook::react::MapBuffer;

static void walk(const MapBuffer& mb, int depth) {
  if (depth > 8) {
    return;
  }
  // MapBuffer stores count_ buckets after the header; keys are looked up by binary search.
  const uint8_t* bytes = mb.data();
  size_t n = mb.count();
  for (size_t i = 0; i < n; i++) {
    size_t off = sizeof(MapBuffer::Header) + i * sizeof(MapBuffer::Bucket);
    if (off + sizeof(MapBuffer::Bucket) > mb.size()) {
      break;
    }
    MapBuffer::Key key;
    uint16_t type;
    __builtin_memcpy(&key, bytes + off, sizeof(key));
    __builtin_memcpy(&type, bytes + off + sizeof(key), sizeof(type));
    switch (type) {
      case MapBuffer::Boolean: (void)mb.getBool(key); break;
      case MapBuffer::Int: (void)mb.getInt(key); break;
      case MapBuffer::Double: (void)mb.getDouble(key); break;
      case MapBuffer::String: (void)mb.getString(key); break;
      case MapBuffer::Map: walk(mb.getMapBuffer(key), depth + 1); break;
      case MapBuffer::Long: (void)mb.getLong(key); break;
      case MapBuffer::IntBuffer: (void)mb.getIntBuffer(key); break;
      case MapBuffer::DoubleBuffer: (void)mb.getDoubleBuffer(key); break;
      case MapBuffer::MapBufferList:
        for (const auto& child : mb.getMapBufferList(key)) {
          walk(child, depth + 1);
        }
        break;
      default: break;
    }
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  MapBuffer mb(std::vector<uint8_t>(data, data + size));
  walk(mb, 0);
  return 0;
}
