# syntax=docker/dockerfile:1.7
#
# Commit image for React Native's C++ core (packages/react-native/ReactCommon).
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t rn .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Build deps from apt: glog/gflags/double-conversion/boost (folly's and ReactCommon's deps),
# gtest/gmock for upstream's unit tests, cmake.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
      cmake libgoogle-glog-dev libgflags-dev libdouble-conversion-dev libboost-dev \
      libgtest-dev libgmock-dev \
 && rm -rf /var/lib/apt/lists/*

# Third-party code upstream's gradle build downloads, at the versions it pins in
# packages/react-native/gradle/libs.versions.toml (fast_float 8.0.0, fmt 12.1.0,
# folly 2024.11.18.00), prebuilt once into /opt/toolchains/deps (SPEC §6.2 item 19).
# Folly: only the subset React Native builds (the source list and flags of
# ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt), as a static library.
ARG FASTFLOAT_VER=8.0.0
ARG FASTFLOAT_SHA256=f312f2dc34c61e665f4b132c0307d6f70ad9420185fa831911bc24408acf625d
ARG FMT_VER=12.1.0
ARG FMT_SHA256=ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea
ARG FOLLY_VER=2024.11.18.00
ARG FOLLY_SHA256=b2c6879ba8ba625218d1ab9eefcc1611a9003d05bc2cb1a38f3bae21892e5167
RUN set -eux; \
    d=/opt/toolchains/deps/fast_float-${FASTFLOAT_VER}; mkdir -p "$d/src" "$d/include"; \
    curl -fsSL -o /tmp/ff.tgz https://github.com/fastfloat/fast_float/archive/v${FASTFLOAT_VER}.tar.gz; \
    echo "${FASTFLOAT_SHA256}  /tmp/ff.tgz" | sha256sum -c -; \
    tar -xzf /tmp/ff.tgz --strip-components=1 -C "$d/src"; cp -r "$d/src/include/fast_float" "$d/include/"; \
    \
    d=/opt/toolchains/deps/fmt-${FMT_VER}; mkdir -p "$d/src" /tmp/fmt-build; \
    curl -fsSL -o /tmp/fmt.tgz https://github.com/fmtlib/fmt/archive/${FMT_VER}.tar.gz; \
    echo "${FMT_SHA256}  /tmp/fmt.tgz" | sha256sum -c -; \
    tar -xzf /tmp/fmt.tgz --strip-components=1 -C "$d/src"; \
    cmake -S "$d/src" -B /tmp/fmt-build -DCMAKE_BUILD_TYPE=None -DCMAKE_CXX_FLAGS="-O2 -g -gdwarf-3" \
      -DFMT_TEST=OFF -DFMT_DOC=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_INSTALL_LIBDIR=lib; \
    cmake --build /tmp/fmt-build -j"$(nproc)"; cmake --install /tmp/fmt-build; \
    \
    d=/opt/toolchains/deps/folly-${FOLLY_VER}; mkdir -p "$d/src" "$d/lib" /tmp/folly-build; \
    curl -fsSL -o /tmp/folly.tgz https://github.com/facebook/folly/archive/v${FOLLY_VER}.tar.gz; \
    echo "${FOLLY_SHA256}  /tmp/folly.tgz" | sha256sum -c -; \
    tar -xzf /tmp/folly.tgz --strip-components=1 -C "$d/src"; \
    cd /tmp/folly-build; \
    for s in Conv Demangle FileUtil Format ScopeGuard SharedMutex String Unicode \
             concurrency/CacheLocality container/detail/F14Table detail/FileUtilDetail detail/Futex \
             detail/SplitStringSimd detail/UniqueInstance hash/SpookyHashV2 json/dynamic json/json_pointer \
             json/json lang/CString lang/Exception lang/SafeAssert lang/ToAscii memory/detail/MallocImpl \
             net/NetOps portability/SysUio synchronization/SanitizeThread synchronization/ParkingLot \
             system/AtFork system/ThreadId; do \
      clang++ -std=c++20 -O2 -g -gdwarf-3 -fexceptions -frtti -Wno-sign-compare -Wno-deprecated-declarations \
        -DFOLLY_NO_CONFIG=1 -DFOLLY_HAVE_CLOCK_GETTIME=1 -DFOLLY_CFG_NO_COROUTINES=1 -DFOLLY_MOBILE=1 \
        -DFOLLY_HAVE_RECVMMSG=1 -DFOLLY_HAVE_PTHREAD=1 \
        -I"$d/src" -I/opt/toolchains/deps/fast_float-${FASTFLOAT_VER}/include -I/opt/toolchains/deps/fmt-${FMT_VER}/include \
        -c "$d/src/folly/$s.cpp" -o "$(echo "$s" | tr / _).o" & \
    done; wait; \
    [ "$(ls *.o | wc -l)" -eq 29 ]; \
    ar rcs "$d/lib/libfolly_runtime.a" *.o; \
    cd /; rm -rf /tmp/fmt-build /tmp/folly-build /tmp/*.tgz; chmod -R a+rX /opt/toolchains/deps

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
