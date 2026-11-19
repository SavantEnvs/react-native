# Mayhem fuzz + test build for React Native's C++ core (packages/react-native/ReactCommon).
# Upstream's native build is gradle/NDK (Android) or CocoaPods (iOS); neither runs on Linux,
# so this compiles the platform-neutral ReactCommon modules directly, with the same flags
# upstream uses (cmake-utils/react-native-flags.cmake) and the folly subset upstream builds
# (ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt), prebuilt in the image.
cmake_minimum_required(VERSION 3.20)
project(rn_mayhem CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RN_FUZZ "build the fuzz harnesses" ON)
option(RN_TESTS "build upstream's gtest suites" OFF)
set(RN_DEPS "/opt/toolchains/deps" CACHE PATH "prebuilt dependency root")
set(STANDALONE_MAIN "" CACHE FILEPATH "LLVM run-once driver")
set(LSAN_OFF "" CACHE FILEPATH "lsan_off.cc")
set(RN_FUZZING_ENGINE "-fsanitize=fuzzer" CACHE STRING "libFuzzer link flag")

get_filename_component(RC "${CMAKE_CURRENT_SOURCE_DIR}/../packages/react-native/ReactCommon" ABSOLUTE)
set(FOLLY "${RN_DEPS}/folly-2024.11.18.00")
set(FMT "${RN_DEPS}/fmt-12.1.0")
set(FASTFLOAT "${RN_DEPS}/fast_float-8.0.0")

add_compile_options(-fexceptions -frtti -Wno-deprecated-declarations -Wno-everything)
add_compile_definitions(
  FOLLY_NO_CONFIG=1 FOLLY_HAVE_CLOCK_GETTIME=1 FOLLY_CFG_NO_COROUTINES=1 FOLLY_MOBILE=1
  FOLLY_HAVE_RECVMMSG=1 FOLLY_HAVE_PTHREAD=1 RN_BUILDING=1)

# Every ReactCommon module dir that ships a `React/` umbrella-header folder.
set(RC_INCLUDES "${RC}" "${RC}/yoga" "${RC}/jsi" "${RC}/callinvoker" "${RC}/runtimeexecutor"
  "${RC}/react/debug" "${RC}/react/utils" "${RC}/react/timing" "${RC}/react/bridging"
  "${RC}/react/nativemodule/core" "${RC}/cxxreact" "${RC}/react/renderer/mapbuffer"
  "${RC}/react/renderer/debug" "${RC}/react/renderer/graphics" "${RC}/react/renderer/core"
  "${RC}/react/renderer/components/view" "${RC}/react/renderer/componentregistry"
  "${RC}/react/renderer/graphics/platform/cxx" "${RC}/react/renderer/components/view/platform/cxx"
  "${RC}/react/renderer/components/text/platform/cxx")
include_directories(${RC_INCLUDES} "${FOLLY}/src" "${FMT}/include" "${FASTFLOAT}/include")

set(FOLLY_LIBS "${FOLLY}/lib/libfolly_runtime.a" "${FMT}/lib/libfmt.a" glog gflags double-conversion atomic pthread)

# ---- libraries (instrumented with whatever CMAKE_CXX_FLAGS the caller passes) -------------
file(GLOB_RECURSE YOGA_SRC "${RC}/yoga/yoga/*.cpp")
add_library(rn_yoga STATIC ${YOGA_SRC})

add_library(rn_mapbuffer STATIC
  "${RC}/react/renderer/mapbuffer/MapBuffer.cpp" "${RC}/react/renderer/mapbuffer/MapBufferBuilder.cpp")
target_link_libraries(rn_mapbuffer PUBLIC glog)

add_library(rn_cdp STATIC "${RC}/jsinspector-modern/cdp/CdpJson.cpp"
  "${RC}/jsinspector-modern/network/HttpUtils.cpp")
target_link_libraries(rn_cdp PUBLIC ${FOLLY_LIBS})
# HttpUtils.h uses uint16_t without <cstdint>; libc++ (upstream's toolchain) pulls it in
# transitively, libstdc++ does not.
set_source_files_properties("${RC}/jsinspector-modern/network/HttpUtils.cpp" PROPERTIES COMPILE_OPTIONS "-include;cstdint")

add_library(rn_methodcall STATIC "${RC}/cxxreact/MethodCall.cpp")
target_link_libraries(rn_methodcall PUBLIC ${FOLLY_LIBS})

# Props parsing: renderer core + view props + graphics + their support modules.
file(GLOB PROPS_SRC
  "${RC}/react/renderer/core/*.cpp"
  "${RC}/react/renderer/components/view/*.cpp"
  "${RC}/react/renderer/components/view/platform/cxx/react/renderer/components/view/*.cpp"
  "${RC}/react/renderer/graphics/*.cpp"
  "${RC}/react/renderer/graphics/platform/cxx/react/renderer/graphics/*.cpp"
  "${RC}/react/renderer/debug/*.cpp"
  "${RC}/react/featureflags/*.cpp"
  "${RC}/react/utils/*.cpp"
  "${RC}/react/debug/*.cpp"
  "${RC}/logger/*.cpp"
  "${RC}/jsi/jsi/jsi.cpp" "${RC}/jsi/jsi/JSIDynamic.cpp")
add_library(rn_props STATIC ${PROPS_SRC})
target_link_libraries(rn_props PUBLIC rn_yoga ${FOLLY_LIBS})

# Text props: attributedstring conversions (fonts, decorations, paragraph attributes) on top of
# the view props. Only the props sources: shadow nodes need the platform text layout manager.
file(GLOB TEXT_SRC "${RC}/react/renderer/attributedstring/*.cpp")
add_library(rn_text STATIC ${TEXT_SRC}
  "${RC}/react/renderer/components/text/BaseParagraphProps.cpp"
  "${RC}/react/renderer/components/text/BaseTextProps.cpp"
  "${RC}/react/renderer/components/text/TextProps.cpp")
target_link_libraries(rn_text PUBLIC rn_props)

# ---- fuzz harnesses -----------------------------------------------------------------------
if(RN_FUZZ)
  # The run-once driver is C: compiled as C so its LLVMFuzzerTestOneInput reference is unmangled.
  set_source_files_properties("${STANDALONE_MAIN}" PROPERTIES LANGUAGE C)
  add_library(rn_standalone_main OBJECT "${STANDALONE_MAIN}" "${LSAN_OFF}")
  add_library(rn_lsan_off OBJECT "${LSAN_OFF}")
  function(rn_fuzzer name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/harnesses/${name}.cpp" $<TARGET_OBJECTS:rn_lsan_off>)
    target_link_libraries(${name} PRIVATE ${ARGN})
    separate_arguments(_engine UNIX_COMMAND "${RN_FUZZING_ENGINE}")
    target_link_options(${name} PRIVATE ${_engine})
    add_executable(${name}-standalone "${CMAKE_CURRENT_SOURCE_DIR}/harnesses/${name}.cpp" $<TARGET_OBJECTS:rn_standalone_main>)
    target_link_libraries(${name}-standalone PRIVATE ${ARGN})
  endfunction()
  rn_fuzzer(css_parse)
  rn_fuzzer(mapbuffer_read rn_mapbuffer)
  rn_fuzzer(yoga_layout rn_yoga)
  rn_fuzzer(cdp_preparse rn_cdp)
  rn_fuzzer(method_calls rn_methodcall)
  rn_fuzzer(view_props rn_props)
  rn_fuzzer(text_props rn_text)
endif()

# ---- upstream's own gtest suites (the functional oracle; built with normal flags) -----------
if(RN_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  function(rn_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${T_LIBS} GTest::gmock GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
  endfunction()
  file(GLOB CSS_TESTS "${RC}/react/renderer/css/tests/*.cpp")
  rn_test(rn_css_tests SOURCES ${CSS_TESTS})
  rn_test(rn_mapbuffer_tests SOURCES "${RC}/react/renderer/mapbuffer/tests/MapBufferTest.cpp" LIBS rn_mapbuffer)
  rn_test(rn_yoga_tests SOURCES "${RC}/yoga/test/BaselineTest.cpp" LIBS rn_yoga)
  rn_test(rn_methodcall_tests SOURCES "${RC}/cxxreact/tests/methodcall.cpp" LIBS rn_methodcall)
  file(GLOB GRAPHICS_TESTS "${RC}/react/renderer/graphics/tests/*.cpp")
  rn_test(rn_props_tests SOURCES ${GRAPHICS_TESTS}
    "${RC}/react/renderer/components/view/tests/AccessibilityPropsTest.cpp"
    "${RC}/react/renderer/components/view/tests/ConversionsTest.cpp"
    "${RC}/react/renderer/components/view/tests/ViewPropsTest.cpp"
    "${RC}/react/renderer/components/view/tests/ResolveTransformTest.cpp"
    # core/tests/RawPropsTest.cpp and RawValueTest.cpp need a Hermes runtime (not built here).
    "${RC}/react/renderer/core/tests/PrimitivesTest.cpp"
    "${RC}/react/renderer/core/tests/DynamicPropsUtilitiesTest.cpp"
    LIBS rn_props)
  rn_test(rn_text_tests SOURCES
    "${RC}/react/renderer/attributedstring/tests/AttributedStringBoxTest.cpp"
    "${RC}/react/renderer/attributedstring/tests/ParagraphAttributesTest.cpp"
    "${RC}/react/renderer/attributedstring/tests/TextAttributesHashTest.cpp"
    "${RC}/react/renderer/attributedstring/tests/TextAttributesLayoutTest.cpp"
    "${RC}/react/renderer/components/text/tests/ParagraphPropsDiffTest.cpp"
    LIBS rn_text)
endif()
