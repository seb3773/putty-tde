cmake_minimum_required(VERSION 3.16)
project(putty-tde C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)

option(PUTTY_AGGRESSIVE_FLAGS "Enable aggressive -O2/LTO compile flags" ON)

if(PUTTY_AGGRESSIVE_FLAGS)
    # Global flags optimized for minimal size + max inter-procedural speed
    set(_PUTTY_C_FLAGS "-O2 -flto=auto -fstrict-aliasing -ffunction-sections -fdata-sections -fno-ident -fno-plt -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fvisibility=hidden -fmerge-all-constants -fno-asynchronous-unwind-tables -DNDEBUG")
    set(_PUTTY_CXX_EXTRA "-std=c++03 -fno-threadsafe-statics -fno-exceptions -fmerge-all-constants -Wno-deprecated-declarations -fpermissive")
    set(CMAKE_C_FLAGS "${_PUTTY_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_PUTTY_C_FLAGS} ${_PUTTY_CXX_EXTRA}")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -Wl,-z,norelro -flto=auto -s")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
    set(CMAKE_C_FLAGS_MINSIZEREL "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++03>)
endif()

find_program(SSTRIP_EXECUTABLE sstrip)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TQT REQUIRED tqt-mt tqt)

find_program(TQMOC_EXECUTABLE tqmoc REQUIRED)
message(STATUS "Found tqmoc: ${TQMOC_EXECUTABLE}")

macro(TQT_WRAP_CPP outfiles)
    foreach(it ${ARGN})
        get_filename_component(it_abs ${it} ABSOLUTE)
        get_filename_component(it_name ${it} NAME_WE)
        set(outfile "${CMAKE_CURRENT_BINARY_DIR}/moc_${it_name}.cpp")
        add_custom_command(OUTPUT ${outfile}
            COMMAND ${TQMOC_EXECUTABLE} ${it_abs} -o ${outfile}
            DEPENDS ${it_abs}
            COMMENT "Running tqmoc on ${it}"
        )
        list(APPEND ${outfiles} ${outfile})
    endforeach()
endmacro()

add_definitions(-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DTQ_SIGNAL=TQT_SIGNAL -DTQ_SLOT=TQT_SLOT)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/_LEGACY
    ${CMAKE_CURRENT_SOURCE_DIR}/_LEGACY/unix
    ${CMAKE_CURRENT_SOURCE_DIR}/_LEGACY/charset
    ${CMAKE_CURRENT_SOURCE_DIR}/tqt
    ${CMAKE_CURRENT_BINARY_DIR}
    ${TQT_INCLUDE_DIRS}
)

link_directories(${TQT_LIBRARY_DIRS})

set(PUTTY_CORE_SOURCES
    _LEGACY/terminal.c
    _LEGACY/stripctrl.c
    _LEGACY/wcwidth.c
    _LEGACY/logging.c
    _LEGACY/tree234.c
    _LEGACY/minibidi.c
    _LEGACY/config.c
    _LEGACY/dialog.c
    _LEGACY/conf.c
    _LEGACY/marshal.c
    _LEGACY/unix/uxcfg.c
    _LEGACY/unix/uxucs.c
    _LEGACY/unix/uxprint.c
    _LEGACY/timing.c
    _LEGACY/callback.c
    _LEGACY/miscucs.c
    _LEGACY/sessprep.c
    _LEGACY/unix/uxmisc.c
    _LEGACY/misc.c
    _LEGACY/ldisc.c
    _LEGACY/settings.c
    _LEGACY/unix/uxsel.c
    _LEGACY/unix/uxstore.c
    _LEGACY/unix/uxsignal.c
    _LEGACY/utils.c
    _LEGACY/memory.c
    _LEGACY/cmdline.c
    _LEGACY/charset/sbcsdat.c
    _LEGACY/charset/slookup.c
    _LEGACY/charset/sbcs.c
    _LEGACY/charset/utf8.c
    _LEGACY/charset/toucs.c
    _LEGACY/charset/fromucs.c
    _LEGACY/charset/xenc.c
    _LEGACY/charset/mimeenc.c
    _LEGACY/charset/macenc.c
    _LEGACY/charset/localenc.c
    _LEGACY/telnet.c
    _LEGACY/raw.c
    _LEGACY/rlogin.c
    _LEGACY/supdup.c
    _LEGACY/pinger.c
    _LEGACY/mpint.c
    _LEGACY/ecc.c
    _LEGACY/sshmd5.c
    _LEGACY/sshsha.c
    _LEGACY/sshsh256.c
    _LEGACY/sshsh512.c
    _LEGACY/sshsha3.c
    _LEGACY/sshrsa.c
    _LEGACY/sshdss.c
    _LEGACY/sshecc.c
    _LEGACY/sshdes.c
    _LEGACY/sshblowf.c
    _LEGACY/sshaes.c
    _LEGACY/sshccp.c
    _LEGACY/ssharcf.c
    _LEGACY/sshdh.c
    _LEGACY/sshcrc.c
    _LEGACY/sshcrcda.c
    _LEGACY/sshauxcrypt.c
    _LEGACY/sshhmac.c
    _LEGACY/sshcommon.c
    _LEGACY/sshutils.c
    _LEGACY/sshprng.c
    _LEGACY/sshrand.c
    _LEGACY/sshverstring.c
    _LEGACY/sshpubk.c
    _LEGACY/sshzlib.c
    _LEGACY/sshmac.c
    _LEGACY/nullplug.c
    _LEGACY/sshgssc.c
    _LEGACY/pgssapi.c
    _LEGACY/wildcard.c
    _LEGACY/ssh1censor.c
    _LEGACY/ssh2censor.c
    _LEGACY/ssh2bpp.c
    _LEGACY/ssh2transport.c
    _LEGACY/ssh2transhk.c
    _LEGACY/ssh2connection.c
    _LEGACY/portfwd.c
    _LEGACY/x11fwd.c
    _LEGACY/ssh1connection.c
    _LEGACY/ssh1bpp.c
    _LEGACY/ssh2bpp-bare.c
    _LEGACY/ssh.c
    _LEGACY/ssh1login.c
    _LEGACY/ssh2userauth.c
    _LEGACY/sshshare.c
    _LEGACY/aqsync.c
    _LEGACY/agentf.c
    _LEGACY/mainchan.c
    _LEGACY/ssh2kex-client.c
    _LEGACY/ssh2connection-client.c
    _LEGACY/ssh1connection-client.c
    _LEGACY/unix/uxnoise.c
    _LEGACY/unix/uxagentc.c
    _LEGACY/unix/uxgss.c
    _LEGACY/unix/uxshare.c
    _LEGACY/be_misc.c
    _LEGACY/proxy.c
    _LEGACY/cproxy.c
    _LEGACY/unix/uxpoll.c
    _LEGACY/unix/uxnet.c
    _LEGACY/unix/uxpeer.c
    _LEGACY/time.c
    _LEGACY/unix/uxfdsock.c
    _LEGACY/errsock.c
    _LEGACY/unix/uxproxy.c
    _LEGACY/unix/uxutils.c
    _LEGACY/be_all_s.c
    _LEGACY/unix/uxser.c
    _LEGACY/unix/ux_x11.c
    _LEGACY/version.c
)

set(TQT_MOC_HEADERS
    tqt/tqtcomm.h
    tqt/tqtwin.h
    tqt/tqtdlg.h
)

TQT_WRAP_CPP(TQT_MOC_SOURCES ${TQT_MOC_HEADERS})

find_package(Python3 COMPONENTS Interpreter QUIET)

# Embedded icons generation
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tqt/putty_icons.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/convert_images.py
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons/puttycfg.png
            ${CMAKE_CURRENT_SOURCE_DIR}/icons/putty.png
            ${CMAKE_CURRENT_SOURCE_DIR}/convert_images.py
    COMMENT "Generating embedded icons header tqt/putty_icons.h"
)

set(TQT_SOURCES
    tqt/tqtmain.cpp
    tqt/tqtcomm.cpp
    tqt/tqtwin.cpp
    tqt/tqtdlg.cpp
    tqt/tqtask.cpp
    tqt/tqtglue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tqt/putty_icons.h
    ${TQT_MOC_SOURCES}
)

add_executable(putty-tde ${PUTTY_CORE_SOURCES} ${TQT_SOURCES})

target_compile_options(putty-tde PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter -Wno-sign-compare>
    $<$<COMPILE_LANGUAGE:CXX>:${TQT_CFLAGS_OTHER} -Wno-unused-parameter -fpermissive>
)

target_link_libraries(putty-tde PRIVATE
    ${TQT_LIBRARIES}
    X11
    m
    dl
)

if(PUTTY_AGGRESSIVE_FLAGS)
    set_source_files_properties(
        tqt/tqtmain.cpp
        tqt/tqtcomm.cpp
        tqt/tqtwin.cpp
        tqt/tqtdlg.cpp
        tqt/tqtask.cpp
        tqt/tqtglue.c
        ${TQT_MOC_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Os"
    )

    if(SSTRIP_EXECUTABLE)
        add_custom_command(TARGET putty-tde POST_BUILD
            COMMAND ${SSTRIP_EXECUTABLE} $<TARGET_FILE:putty-tde>
            COMMENT "Aggressively stripping putty-tde with sstrip"
        )
    endif()
endif()
