#!/usr/bin/env bash
#
# mayhem/build.sh — build React Native's C++ core (packages/react-native/ReactCommon) fuzz
# harnesses and upstream's gtest suites. Runs inside the commit image as `mayhem` in /mayhem.
#
# Upstream's native build is gradle + Android NDK (or CocoaPods); neither exists on Linux, so
# mayhem/CMakeLists.txt compiles the platform-neutral ReactCommon modules directly with upstream's
# flags. Third-party deps upstream fetches at build time (folly subset, fmt, fast_float — the
# versions pinned in packages/react-native/gradle/libs.versions.toml) are prebuilt in the image
# under /opt/toolchains/deps; glog/gflags/double-conversion/boost come from apt. Nothing is fetched
# here, so the build re-runs offline.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# 1+2) Sanitized, coverage-instrumented libraries + harnesses (fuzzer and -standalone binaries).
#   -fsanitize=fuzzer-no-link instruments the ReactCommon code itself, not just the harness TU.
#   REACT_NATIVE_PRODUCTION is upstream's own release switch (react/debug/flags.h): it compiles
#   react_native_assert out, as in shipped apps, so the fuzzer exercises the release code paths
#   (e.g. MapBuffer's bounds clamping) instead of dev-only asserts. C assert() stays enabled.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
cmake -S mayhem -B mayhem/build-fuzz \
  -DCMAKE_BUILD_TYPE=None \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" \
  -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS -DREACT_NATIVE_PRODUCTION" \
  -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS" \
  -DRN_FUZZING_ENGINE="$LIB_FUZZING_ENGINE" \
  -DSTANDALONE_MAIN="$STANDALONE_FUZZ_MAIN" \
  -DLSAN_OFF="$SRC/mayhem/lsan_off.cc" \
  -DRN_FUZZ=ON -DRN_TESTS=OFF
cmake --build mayhem/build-fuzz -j"$MAYHEM_JOBS"

# 3) Upstream's gtest suites with normal flags (no sanitizers, no fuzz instrumentation); mayhem/test.sh
#    runs them.
cmake -S mayhem -B mayhem/build-tests \
  -DCMAKE_BUILD_TYPE=None \
  -DCMAKE_C_FLAGS="-O1 $COVERAGE_FLAGS" \
  -DCMAKE_CXX_FLAGS="-O1 $COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" \
  -DRN_FUZZ=OFF -DRN_TESTS=ON
cmake --build mayhem/build-tests -j"$MAYHEM_JOBS"
