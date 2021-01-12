#!/bin/bash

# Choices for basename(3):
# USE_POSIX_BASENAME or USE_GNU_BASENAME

set -e

GCCFLAGS=`libgcrypt-config --cflags`
GCLIBS=`libgcrypt-config --libs`

COMMONDIR="../common"
CFLAGS="-Wall -I${COMMONDIR} -DUSE_POSIX_BASENAME ${GCCFLAGS}"
OPTCFLAGS="${CFLAGS} -O2"
DBGCFLAGS="${CFLAGS} -ggdb3 -DDEBUG"

rm -rf *.exe *.dbg

gcc ${OPTCFLAGS} tpad.c tpad_*_cb.c ${COMMONDIR}/{async_zmq_reply,futils,getopts,gchelper,gcryptfile}.c -lzmq -lpthread ${GCLIBS} -o tpad.exe
gcc ${DBGCFLAGS} tpad.c tpad_*_cb.c ${COMMONDIR}/{async_zmq_reply,futils,getopts,gchelper,gcryptfile}.c -lzmq -lpthread ${GCLIBS} -o tpad.dbg

gcc ${OPTCFLAGS} beam.c ${COMMONDIR}/{futils,getopts,gchelper,gcryptfile}.c -lzmq ${GCLIBS} -o beam.exe
gcc ${DBGCFLAGS} beam.c ${COMMONDIR}/{futils,getopts,gchelper,gcryptfile}.c -lzmq ${GCLIBS} -o beam.dbg

gcc ${OPTCFLAGS} absorb.c ${COMMONDIR}/{futils,getopts,gchelper,gcryptfile}.c -lzmq ${GCLIBS} -o absorb.exe
gcc ${DBGCFLAGS} absorb.c ${COMMONDIR}/{futils,getopts,gchelper,gcryptfile}.c -lzmq ${GCLIBS} -o absorb.dbg

strip *.exe
