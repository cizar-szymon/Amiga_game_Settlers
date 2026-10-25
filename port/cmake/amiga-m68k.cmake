# CMake toolchain for bebbo's m68k-amigaos-gcc 6.5 (Docker image
# sacredbanana/amiga-compiler:m68k-amigaos, see docker/build.sh).
#
# Toolchain traps known from AmiXcom (UFO/AmiXcom/build/build.sh):
#   -O2 breaks C++ exception unwinding        -> -O1
#   -m68040 selects the 68881 multilib         -> -mcpu=68020 -msoft-float
#   gcc -mcpu=68020 divides by constants with 64-bit MULS.L, which the 060
#   traps                                       -> -mtune=68060
#   -lpthread / -lc pull in newlib beside libnix -> never link them
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)

set(CMAKE_C_COMPILER m68k-amigaos-gcc)
set(CMAKE_CXX_COMPILER m68k-amigaos-g++)
set(CMAKE_AR m68k-amigaos-ar CACHE FILEPATH "")
set(CMAKE_RANLIB m68k-amigaos-ranlib CACHE FILEPATH "")

set(AMIGA_CPU "-mcpu=68020 -msoft-float -mtune=68060" CACHE STRING "CPU flags")
set(CMAKE_C_FLAGS_INIT "${AMIGA_CPU} -O1 -noixemul -fomit-frame-pointer -D__AMIGA__")
set(CMAKE_CXX_FLAGS_INIT "${AMIGA_CPU} -O1 -noixemul -fomit-frame-pointer -D__AMIGA__")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${AMIGA_CPU} -noixemul")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
