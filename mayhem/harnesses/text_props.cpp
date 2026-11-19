// Fuzz React Native's text props parsing (react/renderer/components/text +
// react/renderer/attributedstring/conversions.h): a JSON object of <Text>/<Paragraph> props, as
// the JS renderer sends them, is decoded into RawProps and parsed into ParagraphProps (view props +
// text attributes + paragraph attributes) and TextProps, then turned into TextAttributes. Reaches
// the font weight/style/variant, text decoration, line-break, hyphenation, writing-direction,
// text-shadow and ellipsize conversions.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

#include <folly/json.h>
#include <react/renderer/components/text/ParagraphProps.h>
#include <react/renderer/components/text/TextProps.h>
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

  // RawValue rejects a prop of the wrong type by throwing (folly::TypeError from
  // RawValue::castValue, std::exception subclasses from the other conversions). In an app the
  // props reach RawProps through a JSI host function (UIManagerBinding), and the JS runtime turns
  // any std::exception thrown there into a JS error, so mirror that boundary here. Sanitizer
  // findings are not exceptions and stay fatal.
  try {
    RawPropsParser paragraphParser;
    paragraphParser.prepare<ParagraphProps>();
    RawProps raw(value);
    raw.parse(paragraphParser);
    ParagraphProps paragraph(parserContext, ParagraphProps(), raw);

    RawPropsParser textParser;
    textParser.prepare<TextProps>();
    RawProps rawText(value);
    rawText.parse(textParser);
    TextProps text(parserContext, TextProps(), rawText);

    TextAttributes attrs = TextAttributes::defaultTextAttributes();
    attrs.apply(paragraph.textAttributes);
    attrs.apply(text.textAttributes);
    volatile size_t sink = std::hash<TextAttributes>{}(attrs) + paragraph.paragraphAttributes.maximumNumberOfLines;
    (void)sink;
  } catch (const std::exception&) {
    // rejected by React Native (a JS error in the app)
  }
  return 0;
}
