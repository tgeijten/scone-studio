#!/usr/bin/env bash

set -xeuo pipefail
source "$( dirname ${BASH_SOURCE} )/build_config"

export SCONE_CREATE_DEBIAN_PACKAGE=ON
${SCONE_REPOSITORY_DIR}/tools/unix_2d_build-scone-studio-hfd
cd "${SCONE_BUILD_DIR}"
cpack
cd ..
  
# export SCONE_CREATE_DEBIAN_PACKAGE=OFF
# ${SCONE_REPOSITORY_DIR}/tools/unix_2d_build-scone-studio-hfd
# cd "${SCONE_BUILD_DIR}"
# cpack
# cd ..
