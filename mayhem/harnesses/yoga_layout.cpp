// Fuzz the Yoga layout engine (ReactCommon/yoga) through its public C API: build a bounded node
// tree whose style (flex/grid properties, edges, sizes, percentages, auto) comes from the input,
// then run YGNodeCalculateLayout and read the results back. This is the code every RN view's
// style props end up in.
#include <cstddef>
#include <cstdint>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>
#include <yoga/Yoga.h>

namespace {

constexpr size_t kMaxNodes = 32;

template <typename E>
E pick(FuzzedDataProvider& p, int count) {
  return static_cast<E>(p.ConsumeIntegralInRange<int>(0, count - 1));
}

float value(FuzzedDataProvider& p) {
  switch (p.ConsumeIntegralInRange<int>(0, 3)) {
    case 0: return p.ConsumeFloatingPoint<float>();
    case 1: return static_cast<float>(p.ConsumeIntegralInRange<int>(-50, 1000));
    case 2: return p.ConsumeFloatingPointInRange<float>(0.0f, 100.0f);
    default: return YGUndefined;
  }
}

YGEdge edge(FuzzedDataProvider& p) { return pick<YGEdge>(p, 9); }

void applyStyle(FuzzedDataProvider& p, YGNodeRef n) {
  int props = p.ConsumeIntegralInRange<int>(0, 24);
  for (int i = 0; i < props && p.remaining_bytes() > 0; i++) {
    switch (p.ConsumeIntegralInRange<int>(0, 47)) {
      case 0: YGNodeStyleSetDirection(n, pick<YGDirection>(p, 3)); break;
      case 1: YGNodeStyleSetFlexDirection(n, pick<YGFlexDirection>(p, 4)); break;
      case 2: YGNodeStyleSetJustifyContent(n, pick<YGJustify>(p, 10)); break;
      case 3: YGNodeStyleSetAlignContent(n, pick<YGAlign>(p, 11)); break;
      case 4: YGNodeStyleSetAlignItems(n, pick<YGAlign>(p, 11)); break;
      case 5: YGNodeStyleSetAlignSelf(n, pick<YGAlign>(p, 11)); break;
      case 6: YGNodeStyleSetPositionType(n, pick<YGPositionType>(p, 3)); break;
      case 7: YGNodeStyleSetFlexWrap(n, pick<YGWrap>(p, 3)); break;
      case 8: YGNodeStyleSetOverflow(n, pick<YGOverflow>(p, 3)); break;
      case 9: YGNodeStyleSetDisplay(n, pick<YGDisplay>(p, 4)); break;
      case 10: YGNodeStyleSetFlex(n, value(p)); break;
      case 11: YGNodeStyleSetFlexGrow(n, value(p)); break;
      case 12: YGNodeStyleSetFlexShrink(n, value(p)); break;
      case 13: YGNodeStyleSetFlexBasis(n, value(p)); break;
      case 14: YGNodeStyleSetFlexBasisPercent(n, value(p)); break;
      case 15: YGNodeStyleSetFlexBasisAuto(n); break;
      case 16: YGNodeStyleSetPosition(n, edge(p), value(p)); break;
      case 17: YGNodeStyleSetPositionPercent(n, edge(p), value(p)); break;
      case 18: YGNodeStyleSetMargin(n, edge(p), value(p)); break;
      case 19: YGNodeStyleSetMarginPercent(n, edge(p), value(p)); break;
      case 20: YGNodeStyleSetMarginAuto(n, edge(p)); break;
      case 21: YGNodeStyleSetPadding(n, edge(p), value(p)); break;
      case 22: YGNodeStyleSetPaddingPercent(n, edge(p), value(p)); break;
      case 23: YGNodeStyleSetBorder(n, edge(p), value(p)); break;
      case 24: YGNodeStyleSetGap(n, pick<YGGutter>(p, 3), value(p)); break;
      case 25: YGNodeStyleSetGapPercent(n, pick<YGGutter>(p, 3), value(p)); break;
      case 26: YGNodeStyleSetBoxSizing(n, pick<YGBoxSizing>(p, 2)); break;
      case 27: YGNodeStyleSetWidth(n, value(p)); break;
      case 28: YGNodeStyleSetWidthPercent(n, value(p)); break;
      case 29: YGNodeStyleSetWidthAuto(n); break;
      case 30: YGNodeStyleSetHeight(n, value(p)); break;
      case 31: YGNodeStyleSetHeightPercent(n, value(p)); break;
      case 32: YGNodeStyleSetHeightAuto(n); break;
      case 33: YGNodeStyleSetMinWidth(n, value(p)); break;
      case 34: YGNodeStyleSetMinWidthPercent(n, value(p)); break;
      case 35: YGNodeStyleSetMinHeight(n, value(p)); break;
      case 36: YGNodeStyleSetMinHeightPercent(n, value(p)); break;
      case 37: YGNodeStyleSetMaxWidth(n, value(p)); break;
      case 38: YGNodeStyleSetMaxWidthPercent(n, value(p)); break;
      case 39: YGNodeStyleSetMaxHeight(n, value(p)); break;
      case 40: YGNodeStyleSetMaxHeightPercent(n, value(p)); break;
      case 41: YGNodeStyleSetAspectRatio(n, value(p)); break;
      case 42: YGNodeStyleSetWidthMaxContent(n); break;
      case 43: YGNodeStyleSetWidthFitContent(n); break;
      case 44: YGNodeStyleSetHeightStretch(n); break;
      case 45: YGNodeStyleSetJustifyItems(n, pick<YGJustify>(p, 10)); break;
      case 46: YGNodeStyleSetJustifySelf(n, pick<YGJustify>(p, 10)); break;
      case 47: YGNodeSetNodeType(n, pick<YGNodeType>(p, 2)); break;
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider p(data, size);
  YGConfigRef config = YGConfigNew();
  YGConfigSetPointScaleFactor(config, p.ConsumeIntegralInRange<int>(0, 4));
  YGConfigSetErrata(config, static_cast<YGErrata>(p.ConsumeIntegralInRange<int>(0, 31)));
  YGConfigSetExperimentalFeatureEnabled(config, YGExperimentalFeatureWebFlexBasis, p.ConsumeBool());
  YGConfigSetExperimentalFeatureEnabled(
      config, YGExperimentalFeatureFixFlexBasisFitContent, p.ConsumeBool());

  std::vector<YGNodeRef> nodes;
  nodes.push_back(YGNodeNewWithConfig(config));
  applyStyle(p, nodes[0]);
  size_t count = p.ConsumeIntegralInRange<size_t>(0, kMaxNodes - 1);
  for (size_t i = 0; i < count && p.remaining_bytes() > 0; i++) {
    YGNodeRef parent = nodes[p.ConsumeIntegralInRange<size_t>(0, nodes.size() - 1)];
    YGNodeRef child = YGNodeNewWithConfig(config);
    applyStyle(p, child);
    YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
    nodes.push_back(child);
  }

  float w = value(p);
  float h = value(p);
  YGNodeCalculateLayout(nodes[0], w, h, pick<YGDirection>(p, 3));
  // Re-layout with a different size to exercise the layout cache.
  if (p.ConsumeBool()) {
    YGNodeCalculateLayout(nodes[0], value(p), value(p), pick<YGDirection>(p, 3));
  }
  volatile float sink = 0;
  for (YGNodeRef n : nodes) {
    sink = sink + YGNodeLayoutGetLeft(n) + YGNodeLayoutGetTop(n) + YGNodeLayoutGetWidth(n) +
        YGNodeLayoutGetHeight(n);
  }
  YGNodeFreeRecursive(nodes[0]);
  YGConfigFree(config);
  return 0;
}
