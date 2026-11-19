// Fuzz React Native's CSS value parser (react/renderer/css): the tokenizer, syntax parser and
// every CSS data type a style prop string can carry. Entry point is upstream's public
// parseCSSProperty<Types...>(std::string_view), as used by the props conversions.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <react/renderer/css/CSSAngle.h>
#include <react/renderer/css/CSSBackgroundImage.h>
#include <react/renderer/css/CSSColor.h>
#include <react/renderer/css/CSSFilter.h>
#include <react/renderer/css/CSSFontVariant.h>
#include <react/renderer/css/CSSKeyword.h>
#include <react/renderer/css/CSSLength.h>
#include <react/renderer/css/CSSLengthPercentage.h>
#include <react/renderer/css/CSSNumber.h>
#include <react/renderer/css/CSSPercentage.h>
#include <react/renderer/css/CSSRatio.h>
#include <react/renderer/css/CSSShadow.h>
#include <react/renderer/css/CSSTransform.h>
#include <react/renderer/css/CSSTransformOrigin.h>
#include <react/renderer/css/CSSValueParser.h>

using namespace facebook::react;

template <typename... T>
static void parse(std::string_view s) {
  auto v = parseCSSProperty<T...>(s);
  // Touch the result so the parse is not optimised away.
  volatile bool holds = std::holds_alternative<std::monostate>(v);
  (void)holds;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view s(reinterpret_cast<const char*>(data), size);
  parse<CSSColor>(s);
  parse<CSSFilterList>(s);
  parse<CSSShadowList>(s);
  parse<CSSTransformList>(s);
  parse<CSSTransformOrigin>(s);
  parse<CSSBackgroundImageList>(s);
  parse<CSSFontVariantList>(s);
  parse<CSSRatio, CSSNumber, CSSLengthPercentage>(s);
  parse<CSSAngle, CSSLength, CSSPercentage>(s);
  return 0;
}
