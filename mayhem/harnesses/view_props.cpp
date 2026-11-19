// Fuzz React Native's props parsing (react/renderer/core + components/view + graphics): a JSON
// object of view props, as the JS renderer sends them, is decoded into RawProps and parsed into
// ViewProps the same way upstream's ViewPropsTest does. This reaches RawPropsParser/RawValue,
// the Yoga style conversions (components/view/conversions.h: dimensions, flex, edges), colors,
// transforms, filters, box shadows, background images and accessibility props.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

#include <folly/json.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/RawProps.h>
#include <react/renderer/core/RawPropsParser.h>
#include <react/utils/ContextContainer.h>

using namespace facebook::react;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view s(reinterpret_cast<const char*>(data), size);
  folly::dynamic value;
  try {
    value = folly::parseJson(s);
  } catch (const std::exception&) {
    return 0;  // not JSON: rejected before React Native sees it
  }
  // Props always arrive from the JS renderer as an object (RawProps(folly::dynamic) iterates
  // its items()); anything else is outside RawProps' contract.
  if (!value.isObject()) {
    return -1;
  }

  ContextContainer contextContainer{};
  PropsParserContext parserContext{-1, contextContainer};
  auto parser = RawPropsParser();
  parser.prepare<ViewProps>();

  // RawValue rejects a prop of the wrong type by throwing (folly::TypeError from
  // RawValue::castValue, std::exception subclasses from the other conversions). In an app the
  // props reach RawProps through a JSI host function (UIManagerBinding), and the JS runtime turns
  // any std::exception thrown there into a JS error, so mirror that boundary here. Sanitizer
  // findings are not exceptions and stay fatal.
  try {
    RawProps raw(value);
    raw.parse(parser);
    ViewProps first(parserContext, ViewProps(), raw);

    // Apply the same props again on top of the result (the update path: props are merged
    // into the previous ViewProps instance).
    RawProps again(value);
    again.parse(parser);
    ViewProps second(parserContext, first, again);
    volatile float sink = second.opacity + second.shadowRadius;
    (void)sink;
  } catch (const std::exception&) {
    // rejected by React Native (a JS error in the app)
  }
  return 0;
}
