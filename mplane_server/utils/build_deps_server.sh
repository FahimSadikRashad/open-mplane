#!/bin/bash

# Parse --dir
while [[ $# -gt 0 ]]; do
    case $1 in
        --dir)
            MPLANE_SERVER_DIR=$2
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

# Default directory
if [[ -z "$MPLANE_SERVER_DIR" ]]; then
    MPLANE_SERVER_DIR=$(pwd)/mplane_server
fi

INSTALL_DIR=$MPLANE_SERVER_DIR/deps/install
mkdir -p $INSTALL_DIR/bin $INSTALL_DIR/lib

cd $MPLANE_SERVER_DIR/deps

# Symlink lib64
[[ ! -d $INSTALL_DIR/lib64 ]] && ln -s $INSTALL_DIR/lib $INSTALL_DIR/lib64

# --- Build libssh ---
mkdir -p libssh-0.9.2/build && cd libssh-0.9.2/build
cmake -D WITH_EXAMPLES=OFF \
      -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR \
      ..
make -j$(nproc)
make install
cd ../../

# --- Build libyang ---
mkdir -p libyang/build && cd libyang/build
cmake -D ENABLE_BUILD_TESTS=OFF \
      -D GEN_LANGUAGE_BINDINGS=ON \
      -D GEN_CPP_BINDINGS=ON \
      -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR \
      ..
make -j$(nproc)
make install
cd ../../

# --- Build libnetconf2 ---
mkdir -p libnetconf2/build && cd libnetconf2/build
cmake -D LIBSSH_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBSSH_LIBRARY=$INSTALL_DIR/lib64/libssh.so \
      -D LIBYANG_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBYANG_LIBRARY=$INSTALL_DIR/lib64/libyang.so \
      -D ENABLE_SSH=ON \
      -D ENABLE_TLS=ON \
      -D ENABLE_BUILD_TESTS=OFF \
      -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR \
      ..
make -j$(nproc)
make install
cd ../../

# --- Build sysrepo ---
mkdir -p sysrepo/build && cd sysrepo/build
cmake -D LIBYANG_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBYANG_LIBRARY=$INSTALL_DIR/lib64/libyang.so \
      -D GEN_LANGUAGE_BINDINGS=ON \
      -D GEN_CPP_BINDINGS=ON \
      -D GEN_PYTHON_BINDINGS=OFF \
      -D ENABLE_PYTHON_TESTS=OFF \
      -D BUILD_EXAMPLES=OFF \
      -D CMAKE_BUILD_TYPE=Release \
      -D ENABLE_TESTS=OFF \
      -D CALL_TARGET_BINS_DIRECTLY=OFF \
      -D CMAKE_PREFIX_PATH=$INSTALL_DIR \
      -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR \
      ..
make -j$(nproc)
make install
cd ../../

# --- Build netopeer2 ---
mkdir -p netopeer2/build && cd netopeer2/build
PATH=$INSTALL_DIR/bin:$PATH \
PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig:$PKG_CONFIG_PATH \
cmake -D LIBSSH_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBSSH_LIBRARY=$INSTALL_DIR/lib64/libssh.so \
      -D LIBYANG_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBYANG_LIBRARY=$INSTALL_DIR/lib64/libyang.so \
      -D SYSREPO_INCLUDE_DIR=$INSTALL_DIR/include \
      -D SYSREPO_LIBRARY=$INSTALL_DIR/lib64/libsysrepo.so \
      -D LIBNETCONF2_INCLUDE_DIR=$INSTALL_DIR/include \
      -D LIBNETCONF2_LIBRARY=$INSTALL_DIR/lib64/libnetconf2.so \
      -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR \
      ..
make -j$(nproc)
PATH=$INSTALL_DIR/bin:$PATH LD_LIBRARY_PATH=$INSTALL_DIR/lib64:$LD_LIBRARY_PATH make install
cd ../../

echo "All dependencies installed to $INSTALL_DIR"
