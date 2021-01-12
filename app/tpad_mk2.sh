#!/bin/bash

if [ ! -d /tpad ]; then
  echo "/tpad is not a directory!"
  exit 1
fi

exec /app/tpad.exe -Z tcp://*:8384 -d /tpad/
