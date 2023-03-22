#!/usr/bin/env bash

set -xeuo pipefail
source "$( dirname ${BASH_SOURCE} )/build_config"

${SCONE_REPOSITORY_DIR}/tools/unix_2d_build-scone-studio-hfd
${SCONE_REPOSITORY_DIR}/tools/mac_3_create-install-dirtree-fix
${SCONE_REPOSITORY_DIR}/tools/mac_4_create-dmg
