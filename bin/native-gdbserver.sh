#!/usr/bin/env bash

set -e
pio run --environment superbase-native-tests
gdbserver --once localhost:2345 .pio/build/superbase-native-tests/meshtasticd "$@"
