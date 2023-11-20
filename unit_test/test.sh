#!/bin/bash

TARGET_DIR=cmake-build-test
cmake -S . -B ${TARGET_DIR}
cmake --build ${TARGET_DIR} || exit 1
./${TARGET_DIR}/test_executable
