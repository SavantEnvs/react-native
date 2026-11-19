// Fuzz the legacy bridge's JS->native call-queue decoder (cxxreact/MethodCall.cpp). The JS side
// serializes the batched queue as JSON ([moduleIds, methodIds, params, callId]); NativeToJsBridge
// hands the decoded folly::dynamic to parseMethodCalls(). Exceptions are the documented error
// path (the bridge stops processing the batch), so they are caught here.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

#include <cxxreact/MethodCall.h>
#include <folly/json.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view s(reinterpret_cast<const char*>(data), size);
  try {
    folly::dynamic calls = folly::parseJson(s);
    auto parsed = facebook::react::parseMethodCalls(std::move(calls));
    volatile size_t n = 0;
    for (const auto& c : parsed) {
      n = n + c.moduleId + c.methodId + c.callId + c.arguments.size();
    }
    (void)n;
  } catch (const std::exception&) {
  }
  return 0;
}
