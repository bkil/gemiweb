#!/bin/sh

gcc \
 -Werror -std=gnu11\
 -Wno-unused-parameter\
 -Wpedantic\
 -Wwrite-strings\
 -Woverflow\
 -fstrict-overflow\
 -Wstrict-overflow=5 -Wno-error=strict-overflow\
 -Wformat-nonliteral\
 -fstrict-aliasing\
 -Winit-self \
 -fno-common \
 -W \
 -Wabi \
 -Wall \
 -Wcast-align \
 -Wdeprecated \
 -Wdeprecated-declarations \
 -Wextra \
 -Winvalid-pch \
 -Wmissing-format-attribute \
 -Woverflow \
 -Wshadow \
 \
 -Waggregate-return \
 -Wbad-function-cast \
 -Wcast-qual \
 -Wconversion \
 -Wdouble-promotion \
 -Wfloat-equal \
 -Wformat=2 \
 -Winline \
 -Wmissing-declarations \
 -Wno-error=missing-format-attribute \
 -Wmissing-prototypes \
 -Wnested-externs \
 -Wpointer-arith \
 -Wpointer-sign \
 -Wredundant-decls \
 -Wsign-conversion \
 -Wstrict-prototypes \
 -Wsuggest-attribute=pure \
 -Wsuggest-attribute=const \
 -Wsuggest-attribute=noreturn \
 -Wsuggest-attribute=format \
 -Wswitch-default \
 -Wundef \
 -fipa-pta \
 -ffast-math \
 -fmerge-all-constants \
 -Wl,-O1 \
 -Wl,--gc-sections \
 -Wl,--as-needed \
 "$@"
