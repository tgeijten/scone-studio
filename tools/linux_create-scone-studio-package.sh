#!/usr/bin/env bash

set -xeuo pipefail
script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

export CMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE:-False}

source "${script_dir}/build_config"

export SCONE_CREATE_DEBIAN_PACKAGE=ON
${script_dir}/unix_2d_build-scone-studio-hfd

cd "${SCONE_BUILD_DIR}"
cpack
cd ..
  
export SCONE_CREATE_DEBIAN_PACKAGE=OFF
${script_dir}/unix_2d_build-scone-studio-hfd

cd "${SCONE_BUILD_DIR}"
cpack
cd ..
