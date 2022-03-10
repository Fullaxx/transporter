#!/bin/bash

if [ ! -d /tpad ]; then
  echo "/tpad is not a directory!"
  exit 1
fi

TPUTPORT=${TPUTPORT:-8480}
TGETPORT=${TGETPORT:-8471}
TDELPORT=${TDELPORT:-8468}

exec /app/tpad.exe -P tcp://*:${TPUTPORT} -G tcp://*:${TGETPORT} -D tcp://*:${TDELPORT} -d /tpad
