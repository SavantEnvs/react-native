#!/usr/bin/env bash
#
# mayhem/test.sh — run upstream's own gtest suites for the ReactCommon modules the fuzz targets
# cover (CSS parser, MapBuffer, Yoga, MethodCall, graphics/view/core props conversions,
# attributedstring/text props), built by
# mayhem/build.sh into mayhem/build-tests with normal flags. Each suite asserts parsed values
# (EXPECT_EQ on colors, lengths, transforms, props...), so a neutered program fails. Counts come
# from gtest's own summary lines; a binary that prints no summary counts as a failure.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

SUITES="rn_css_tests rn_mapbuffer_tests rn_yoga_tests rn_methodcall_tests rn_props_tests rn_text_tests"
passed=0 failed=0 skipped=0
for s in $SUITES; do
  bin="mayhem/build-tests/$s"
  if [ ! -x "$bin" ]; then
    echo "missing test binary $bin (mayhem/build.sh should have built it)" >&2
    failed=$((failed + 1)); continue
  fi
  out="$("$bin" --gtest_color=no 2>&1)"; rc=$?
  printf '%s\n' "$out" | grep -E '^\[  (PASSED|FAILED|SKIPPED) |^\[==========\]' | sed "s/^/$s: /"
  p=$(printf '%s\n' "$out" | sed -nE 's/^\[  PASSED  \] ([0-9]+) tests?\.?.*/\1/p' | tail -1)
  f=$(printf '%s\n' "$out" | sed -nE 's/^\[  FAILED  \] ([0-9]+) tests?, listed below.*/\1/p' | tail -1)
  k=$(printf '%s\n' "$out" | sed -nE 's/^\[  SKIPPED \] ([0-9]+) tests?, listed below.*/\1/p' | tail -1)
  if [ -z "$p" ]; then
    echo "$s: no gtest summary (rc=$rc) — counted as failed" >&2
    failed=$((failed + 1)); continue
  fi
  passed=$((passed + p)); failed=$((failed + ${f:-0})); skipped=$((skipped + ${k:-0}))
  [ "$rc" -eq 0 ] || [ -n "$f" ] || failed=$((failed + 1))
done

emit_ctrf "gtest" "$passed" "$failed" "$skipped"
