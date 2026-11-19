// Fuzz the Chrome DevTools Protocol front end of React Native's debugger backend
// (jsinspector-modern/cdp): every message that arrives over the inspector websocket goes through
// cdp::preparse() before dispatch. On success the parsed request is re-serialized with the
// response helpers, as HostAgent does when answering it. The same input is also handed to the
// network inspector's helpers for a response it reports to the debugger: the Content-Type parser
// (jsinspector-modern/network/HttpUtils), the UTF-8 truncation of a response body chunk
// (jsinspector-modern/Utf8.h) and the base64 body encoder (react/utils/Base64.h).
#include <cstddef>
#include <cstdint>  // before HttpUtils.h, which uses uint16_t without including it
#include <string>
#include <string_view>
#include <vector>

#include <jsinspector-modern/cdp/CdpJson.h>
#include <jsinspector-modern/Utf8.h>
#include <jsinspector-modern/network/HttpUtils.h>
#include <react/utils/Base64.h>

namespace cdp = facebook::react::jsinspector_modern::cdp;
namespace jm = facebook::react::jsinspector_modern;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view msg(reinterpret_cast<const char*>(data), size);
  try {
    cdp::PreparsedRequest req = cdp::preparse(msg);
    volatile size_t n = cdp::jsonResult(req.id, req.params).size();
    n = cdp::jsonRequest(req.id, req.method, req.params).size();
    n = cdp::jsonError(req.id, cdp::ErrorCode::MethodNotFound, req.method).size();
    (void)n;
    (void)req.toJson();
  } catch (const cdp::ParseError&) {
  } catch (const cdp::TypeError&) {
  }
  // Content-Type header of a network response the inspector reports.
  volatile size_t m = jm::mimeTypeFromHeaders({{"Content-Type", std::string(msg)}}).size();
  (void)m;
  // A response body chunk: cut at a UTF-8 boundary (throws std::runtime_error on an invalid
  // sequence, its documented error path), and base64-encoded for Network.getResponseBody.
  std::vector<char> body(msg.begin(), msg.end());
  try {
    jm::truncateToValidUTF8(body);
  } catch (const std::runtime_error&) {
  }
  m = facebook::react::base64Encode(std::string_view(body.data(), body.size())).size();
  return 0;
}
