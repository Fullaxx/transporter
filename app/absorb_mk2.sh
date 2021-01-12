#!/bin/bash

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
