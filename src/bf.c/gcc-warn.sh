#!/bin/sh

gcc \
 -std=c99 \
 -Werror \
 \
 -ffast-math \
 -fipa-profile \
 -fipa-pta \
 -fira-loop-pressure \
 -fmerge-all-constants \
 -fno-common \
 -fstrict-aliasing \
 -fstrict-overflow \
 -ftree-partial-pre \
 -funsafe-loop-optimizations \
 -pedantic \
 -pedantic-errors \
 -Wabi \
 -Wall \
 -Waggregate-return \
 -Warray-bounds=2 \
 -Wbad-function-cast \
 -Wcast-align \
 -Wcast-qual \
 -Wconversion \
 -Wdate-time \
 -Wdeprecated \
 -Wdeprecated-declarations \
 -Wdisabled-optimization \
 -Wdouble-promotion \
 -Wendif-labels \
 -Wextra \
 -Wfloat-equal \
 -Wformat=2 \
 -Wformat-nonliteral \
 -Wframe-larger-than=256 \
 -Winit-self \
 -Winline \
 -Winvalid-pch \
 -Wjump-misses-init \
 -Wl,-O1 \
 -Wl,--gc-sections \
 -Wl,--as-needed \
 -Wlogical-op \
 -Wmissing-declarations \
 -Wmissing-format-attribute \
 -Wmissing-include-dirs \
 -Wmissing-prototypes \
 -Wnested-externs \
 -Wno-error=padded \
 -Wno-error=strict-overflow \
 -Wno-unused-parameter \
 -Woverflow \
 -Wpacked \
 -Wpadded \
 -Wpedantic \
 -Wpointer-arith \
 -Wpointer-sign \
 -Wredundant-decls \
 -Wshadow \
 -Wsign-conversion \
 -Wstack-usage=384 \
 -Wstrict-aliasing=1 \
 -Wstrict-overflow=5 \
 -Wstrict-prototypes \
 -Wsuggest-attribute=pure \
 -Wsuggest-attribute=const \
 -Wsuggest-attribute=noreturn \
 -Wsuggest-attribute=format \
 -Wswitch-default \
 -Wswitch-enum \
 -Wtrampolines \
 -Wundef \
 -Wuninitialized \
 -Wunsafe-loop-optimizations \
 -Wunsuffixed-float-constants \
 -Wunused-but-set-parameter \
 -Wunused-macros \
 -Wvector-operation-performance \
 -Wwrite-strings \
 "$@"
