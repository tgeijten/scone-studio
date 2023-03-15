#!/usr/bin/env bash

set -xeuo pipefail
source "$( dirname ${BASH_SOURCE} )/build_config"

${script_dir}/linux_1_get-external-dependencies
${script_dir}/unix_2a_build-osg-fix
${script_dir}/unix_2b_build-simbody-fix
${script_dir}/unix_2c_build-opensim3-fix
