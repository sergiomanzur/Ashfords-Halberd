# ctest "settings": every recvx.ini key in config.c (keys[]) is edited by a row of the settings
# table (settings.c g_settings[]), which both menus show (F1 and OPTION > ENHANCED SETTINGS), or
# is listed here with where it is set instead. A new setting without a menu row fails.
#   cmake -DSRC=<pc dir> -P check_settings.cmake
set(NOT_IN_TABLE
    tps_key      # CONTROLS bindings, "Third person on/off" (bindings.c, its first keyboard slot)
    borderless_extra   # ini only: the Windows borderless window's overhang past the monitor (settings.c), a driver workaround
    settings_preset   # Experience preset rows delegate their marker and validation to presets.c
)
file(READ "${SRC}/src/host/config.c" config)
file(READ "${SRC}/src/host/settings.c" table)   # rows, and the get/set helpers of rows such as Window size
string(REGEX MATCHALL "{ \"[a-z_0-9]+\", (\\(int\\*\\))?&[A-Za-z_0-9.]+" entries "${config}")
list(LENGTH entries n)
if(n LESS 40)
    message(FATAL_ERROR "settings: only ${n} ini keys found in config.c; update this check")
endif()
set(missing "")
foreach(e ${entries})
    string(REGEX REPLACE "{ \"([a-z_0-9]+)\".*" "\\1" key "${e}")
    string(REGEX REPLACE ".*&([A-Za-z_.]+)" "\\1" var "${e}")
    list(FIND NOT_IN_TABLE ${key} skip)
    string(REPLACE "." "\\." re "${var}")
    string(REGEX MATCH "[^A-Za-z_0-9.]${re}[^A-Za-z_0-9]" found "${table}")
    if(skip EQUAL -1 AND NOT found)
        list(APPEND missing "${key} (${var})")
    endif()
endforeach()
if(NOT table MATCHES "\"Experience preset\"[^\n]*preset_get[^\n]*preset_set")
    message(FATAL_ERROR "settings: missing shared Experience preset getter/setter row")
endif()
if(missing)
    message(FATAL_ERROR "settings: ini keys without a row in g_settings[] (pc/src/host/settings.c), so missing from "
                        "F1 and ENHANCED SETTINGS: ${missing}. Add a row, or list the key in ${CMAKE_CURRENT_LIST_FILE} with where it is set.")
endif()
message(STATUS "settings: ${n} ini keys, all in the settings table or listed")

# Menu parity: a shared table only guarantees parity when both frontends render it without
# per-frontend row flags, and both retain the same binding editor.
file(READ "${SRC}/src/host/overlay.cpp" overlay)
file(READ "${SRC}/glue/options_glue.c" native)
file(READ "${SRC}/src/host/settings.h" settings_header)
file(READ "${SRC}/src/host/ashford_panel.cpp" panel)
foreach(source overlay native table settings_header panel)
    if("${${source}}" MATCHES "SP_COOP|SP_EXTRAS|SET_F1")
        message(FATAL_ERROR "settings parity: obsolete/hidden page or frontend flag in ${source}")
    endif()
endforeach()
foreach(pair "overlay;settings_imgui\\(page\\)" "native;page_rows\\(\\)" "native;bind_set\\(" "overlay;bind_set\\(" "settings_header;SP_CHEATS" "overlay;panel_select_page\\(SP_CHEATS\\)")
    list(GET pair 0 source)
    list(GET pair 1 required)
    if(NOT "${${source}}" MATCHES "${required}")
        message(FATAL_ERROR "settings parity: ${source} missing ${required}")
    endif()
endforeach()
string(FIND "${panel}" "IM_ARRAYSIZE(titles) == SP_COUNT + 2" page_count_guard)
if(page_count_guard EQUAL -1)
    message(FATAL_ERROR "settings parity: frontend page count must track the shared pages")
endif()
message(STATUS "settings parity: every shared page and both binding editors retained")
