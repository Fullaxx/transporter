#!/bin/bash

if [ ! -d /beam ]; then
  echo "/beam is not a directory!"
  exit 1
fi

exec /app/beam.exe -Z tcp://${TPAD} -d /beam/
