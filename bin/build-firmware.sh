#!/usr/bin/env bash

set -euo pipefail
export PIP_BREAK_SYSTEM_PACKAGES=1

if [[ ${1:-muzi-base} != muzi-base ]]; then
  echo "Only muzi-base is supported" >&2
  exit 1
fi

exec bin/build-nrf52.sh muzi-base
