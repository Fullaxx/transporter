#!/bin/bash

if [ -z "${TPADHOST}" ]; then
  echo "$0: <TPADHOST>"
  exit 1
fi

if [ ! -d /beam ]; then
  echo "/beam is not a directory!"
  exit 2
fi

TPUTPORT=${TPUTPORT:-8480}

exec /app/beam.exe -P tcp://${TPADHOST}:${TPUTPORT} -d /beam
