# Settlers port: builds from the pinned Freeserf sources in ../freeserf.
# PC:    cmake -S port -B build/pc-port -G Ninja
# Amiga: cmake -S port -B build/amiga -DCMAKE_TOOLCHAIN_FILE=port/cmake/amiga-m68k.cmake
cmake_minimum_required(VERSION 3.14)
project(SettlersPort C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FREESERF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../freeserf CACHE PATH "Freeserf sources")
set(FS ${FREESERF_DIR}/src)


add_library(fs_tools STATIC
  ${FS}/debug.cc ${FS}/log.cc ${FS}/configfile.cc ${FS}/buffer.cc)

add_library(fs_game STATIC
  ${FS}/ai.cc ${FS}/ai-core.cc ${FS}/ai-manage.cc ${FS}/ai-rate.cc
  ${FS}/ai-road.cc ${FS}/ai-want.cc ${FS}/building.cc ${FS}/flag.cc
  ${FS}/game.cc ${FS}/integrity.cc ${FS}/inventory.cc ${FS}/map.cc
  ${FS}/map-generator.cc ${FS}/mission.cc ${FS}/player.cc ${FS}/random.cc
  ${FS}/savegame.cc ${FS}/serf.cc ${FS}/game-manager.cc)

# Game data without SDL: the dummy sprite file, font and translation.
add_library(fs_data STATIC
  ${FS}/data.cc ${FS}/data-source-mixed.cc ${FS}/data-source-dos.cc
  ${FS}/data-source-amiga.cc ${FS}/data-source-legacy.cc
  ${FS}/data-source-custom.cc ${FS}/tpwm.cc ${FS}/sfx2wav.cc ${FS}/xmi2mid.cc
  ${FS}/pcm2wav.cc ${FS}/data-source.cc sprite-file-amiga.cc
  ${FS}/font-dummy.cc ${FS}/translation-dummy.cc)

foreach(t fs_tools fs_game fs_data)
  target_include_directories(${t} PUBLIC ${FREESERF_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/..)
endforeach()

if(AMIGA_CPU)
  set(PLATFORM_SRC amiga_time.c)
endif()
add_executable(simtest simtest.cc ${PLATFORM_SRC})
# The map generator (the start menu makes a new map for each change) with
# -O2: it throws no exceptions, which -O2 breaks elsewhere.
set_source_files_properties(${FS}/map-generator.cc PROPERTIES COMPILE_OPTIONS "-O2")
target_link_libraries(simtest fs_game fs_tools)
add_executable(mapgentest mapgentest.cc ${PLATFORM_SRC})
target_link_libraries(mapgentest fs_game fs_tools)

add_executable(datacheck datacheck.cc ${PLATFORM_SRC})
target_link_libraries(datacheck fs_data fs_tools)

# Loading a save, step by step; -fm with AmiXcom's size-class allocator.
add_executable(loadtest loadtest.cc ${PLATFORM_SRC})
target_link_libraries(loadtest fs_game fs_tools)
if(AMIGA_CPU)
  add_executable(loadtest-fm loadtest.cc amiga_fastmalloc.c ${PLATFORM_SRC})
  target_link_libraries(loadtest-fm fs_game fs_tools)
  # The network calls of a LAN game from the Shell.
  add_executable(lantest lantest.c amiga_net.c amiga_extip.c amiga_time.c)
  # The start of a LAN game without the network: the same game twice?
  add_executable(lansim lansim.cc ${FS}/lan-sync.cc ${FS}/lan-command.cc
                ${FS}/pathfinder.cc ${PLATFORM_SRC})
  target_link_libraries(lansim fs_game fs_tools)
endif()

if(AMIGA_CPU)
  # The game: Freeserf's GUI with the Amiga platform layer instead of SDL.
  add_library(fs_gui STATIC
    settings-amiga.cc video-amiga.cc event_loop-amiga.cc
    amiga_screen.c amiga_time.c amiga_sysinfo.c amiga-box.cc lan-amiga.cc amiga_net.c amiga_extip.c
    ${FS}/pathfinder.cc ${FS}/advanced-box.cc ${FS}/keys-box.cc ${FS}/lan-box.cc ${FS}/lan-game.cc ${FS}/lan-sync.cc ${FS}/lan-command.cc ${FS}/box-frame.cc
    ${FS}/gfx.cc ${FS}/viewport.cc ${FS}/minimap.cc ${FS}/interface.cc
    ${FS}/gui.cc ${FS}/popup.cc ${FS}/game-init.cc ${FS}/notification.cc
    ${FS}/panel.cc ${FS}/version.cc ${FS}/text-input.cc ${FS}/list.cc
    ${FS}/command_line.cc ${FS}/video.cc ${FS}/audio.cc ${FS}/event_loop.cc
    audio-amiga.cc amiga_audio.c thirdparty/micromod/micromod.c amiga_wb.c
    intro-player.cc intro-amiga.cc)
  target_include_directories(fs_gui PUBLIC ${FREESERF_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/..
                             ${CMAKE_CURRENT_SOURCE_DIR}/gen)

  # The game music relies on vertical-blank timing (see thirdparty/micromod).
  set_source_files_properties(thirdparty/micromod/micromod.c PROPERTIES
                              COMPILE_DEFINITIONS MICROMOD_VBLANK)

  # AmiXcom's size-class allocator instead of libnix's malloc: loading a
  # save (some 300000 small blocks) 1.0 -> 0.57 s on the Warp (loadtest).
  add_executable(Settlers main-amiga.cc amiga_fastmalloc.c)
  target_link_libraries(Settlers fs_gui fs_game fs_data fs_tools)

  # Diagnostic build: the checking allocator of AmiXcom instead of libnix's
  # malloc, every heap block checked once a frame (RAM:Settlers-heap.txt).
  add_executable(Settlers-hg main-amiga.cc amiga_heapguard.c
                 amiga_heapguard_new.cpp)
  target_link_libraries(Settlers-hg fs_gui fs_game fs_data fs_tools)

  add_executable(modelist modelist.c)
endif()
