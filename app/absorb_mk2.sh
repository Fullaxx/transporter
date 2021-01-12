#!/bin/bash

if [ ! -d /absorb ]; then
  echo "/absorb is not a directory!"
  exit 1
fi

if [ ${METHOD} == "oldest" ]; then
  METHODARG="--oldest"
fi

if [ ${METHOD} == "newest" ]; then
  METHODARG="--newest"
fi

if [ ${METHOD} == "largest" ]; then
  METHODARG="--largest"
fi

if [ ${METHOD} == "smallest" ]; then
  METHODARG="--smallest"
fi

exec /app/absorb.exe -Z tcp://${TPAD} -d /absorb/ ${METHODARG}
