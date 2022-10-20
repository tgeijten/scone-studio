#!/usr/bin/env bash

script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source "${script_dir}/build_config"

source "${script_dir}/linux_1_get-external-dependencies-fix"
