#!/bin/bash

unset METHODARG

if [ -z "${TPADHOST}" ]; then
  echo "$0: <TPADHOST>"
  exit 1
fi

if [ ! -d /absorb ]; then
  echo "/absorb is not a directory!"
  exit 2
fi

TGETPORT=${TGETPORT:-8471}
TDELPORT=${TDELPORT:-8468}

exec /app/absorb.exe -G tcp://${TPADHOST}:${TGETPORT} -D tcp://${TPADHOST}:${TDELPORT} -d /absorb
