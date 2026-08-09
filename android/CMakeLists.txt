cmake_minimum_required(VERSION 3.24)
project(YkwEditorsAndroid LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)
include(FetchContent)
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cryptopp-cmake GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git GIT_TAG CRYPTOPP_8_9_0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(cryptopp-cmake)
if(TARGET cryptopp::cryptopp)
  set(YKW_CRYPTOPP_TARGET cryptopp::cryptopp)
elseif(TARGET cryptopp-static)
  set(YKW_CRYPTOPP_TARGET cryptopp-static)
elseif(TARGET cryptopp)
  set(YKW_CRYPTOPP_TARGET cryptopp)
else()
  message(FATAL_ERROR "Crypto++ target was not created by cryptopp-cmake")
endif()
set(YKW_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(YKW_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/editor-plugins")
file(MAKE_DIRECTORY "${YKW_PLUGIN_OUTPUT_DIR}")
function(ykw_configure_plugin target source_dir wrapper)
  add_library(${target} SHARED ${ARGN} "${wrapper}")
  target_include_directories(${target} PRIVATE "${source_dir}")
  target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat)
  target_link_options(${target} PRIVATE "-Wl,-Bsymbolic")
  set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES LIBRARY_OUTPUT_DIRECTORY "${YKW_PLUGIN_OUTPUT_DIR}")
endfunction()
set(YW1 "${YKW_ROOT}/source-code/1")
ykw_configure_plugin(ykw_editor1 "${YW1}" "${CMAKE_CURRENT_LIST_DIR}/plugins/yw1_plugin.cpp"
 "${YW1}/mainwindow.cpp" "${YW1}/version_dialog.cpp" "${YW1}/savemanager.cpp" "${YW1}/xorshift.cpp" "${YW1}/ywcipher.cpp" "${YW1}/crc32.cpp" "${YW1}/error.cpp" "${YW1}/tab.cpp" "${YW1}/listtab.cpp" "${YW1}/youkaitab.cpp" "${YW1}/gamedata.cpp" "${YW1}/manualeditdialog.cpp"
 "${YW1}/QHexEdit/qhexedit.cpp" "${YW1}/QHexEdit/qhexeditcomments.cpp" "${YW1}/QHexEdit/qhexeditdata.cpp" "${YW1}/QHexEdit/qhexeditdatadevice.cpp" "${YW1}/QHexEdit/qhexeditdatareader.cpp" "${YW1}/QHexEdit/qhexeditdatawriter.cpp" "${YW1}/QHexEdit/qhexedithighlighter.cpp" "${YW1}/QHexEdit/qhexeditprivate.cpp" "${YW1}/QHexEdit/sparserangemap.cpp"
 "${YW1}/itemtab.cpp" "${YW1}/equipmenttab.cpp" "${YW1}/importanttab.cpp" "${YW1}/section.cpp" "${YW1}/section9tab.cpp" "${YW1}/youkaichecklist.cpp" "${YW1}/medalliumtab.cpp"
 "${YW1}/mainwindow.ui" "${YW1}/version_dialog.ui" "${YW1}/youkaitab.ui" "${YW1}/manualeditdialog.ui" "${YW1}/itemtab.ui" "${YW1}/equipmenttab.ui" "${YW1}/importanttab.ui" "${YW1}/section9tab.ui" "${YW1}/medalliumtab.ui" "${YW1}/resources.qrc")
set(YW2 "${YKW_ROOT}/source-code/2")
ykw_configure_plugin(ykw_editor2 "${YW2}" "${CMAKE_CURRENT_LIST_DIR}/plugins/yw2_plugin.cpp"
 "${YW2}/mainwindow.cpp" "${YW2}/version_dialog.cpp" "${YW2}/savemanager.cpp" "${YW2}/xorshift.cpp" "${YW2}/ywcipher.cpp" "${YW2}/ccmcipher.cpp" "${YW2}/crc32.cpp" "${YW2}/error.cpp" "${YW2}/tab.cpp" "${YW2}/listtab.cpp" "${YW2}/youkaitab.cpp" "${YW2}/gamedata.cpp" "${YW2}/gameconfig.cpp" "${YW2}/manualeditdialog.cpp"
 "${YW2}/QHexEdit/qhexedit.cpp" "${YW2}/QHexEdit/qhexeditcomments.cpp" "${YW2}/QHexEdit/qhexeditdata.cpp" "${YW2}/QHexEdit/qhexeditdatadevice.cpp" "${YW2}/QHexEdit/qhexeditdatareader.cpp" "${YW2}/QHexEdit/qhexeditdatawriter.cpp" "${YW2}/QHexEdit/qhexedithighlighter.cpp" "${YW2}/QHexEdit/qhexeditprivate.cpp" "${YW2}/QHexEdit/sparserangemap.cpp"
 "${YW2}/itemtab.cpp" "${YW2}/equipmenttab.cpp" "${YW2}/importanttab.cpp" "${YW2}/section1tab.cpp" "${YW2}/section.cpp" "${YW2}/section9tab.cpp" "${YW2}/soultab.cpp" "${YW2}/youkaichecklist.cpp" "${YW2}/medalliumtab.cpp"
 "${YW2}/mainwindow.ui" "${YW2}/version_dialog.ui" "${YW2}/manualeditdialog.ui" "${YW2}/section1tab.ui" "${YW2}/section9tab.ui" "${YW2}/equipmenttabform.ui" "${YW2}/importanttabform.ui" "${YW2}/itemtabform.ui" "${YW2}/youkaitabform.ui" "${YW2}/soultabform.ui" "${YW2}/listtab.ui" "${YW2}/medalliumtab.ui" "${YW2}/resources.qrc")
target_link_libraries(ykw_editor2 PRIVATE ${YKW_CRYPTOPP_TARGET})
set(YWB1 "${YKW_ROOT}/source-code/B1")
ykw_configure_plugin(ykw_editor_b1 "${YWB1}" "${CMAKE_CURRENT_LIST_DIR}/plugins/ywb1_plugin.cpp"
 "${YWB1}/mainwindow.cpp" "${YWB1}/version_dialog.cpp" "${YWB1}/savemanager.cpp" "${YWB1}/xorshift.cpp" "${YWB1}/ywcipher.cpp" "${YWB1}/ccmcipher.cpp" "${YWB1}/crc32.cpp" "${YWB1}/error.cpp" "${YWB1}/tab.cpp" "${YWB1}/listtab.cpp" "${YWB1}/youkaitab.cpp" "${YWB1}/gamedata.cpp" "${YWB1}/gameconfig.cpp" "${YWB1}/manualeditdialog.cpp"
 "${YWB1}/QHexEdit/qhexedit.cpp" "${YWB1}/QHexEdit/qhexeditcomments.cpp" "${YWB1}/QHexEdit/qhexeditdata.cpp" "${YWB1}/QHexEdit/qhexeditdatadevice.cpp" "${YWB1}/QHexEdit/qhexeditdatareader.cpp" "${YWB1}/QHexEdit/qhexeditdatawriter.cpp" "${YWB1}/QHexEdit/qhexedithighlighter.cpp" "${YWB1}/QHexEdit/qhexeditprivate.cpp" "${YWB1}/QHexEdit/sparserangemap.cpp"
 "${YWB1}/itemtab.cpp" "${YWB1}/equipmenttab.cpp" "${YWB1}/importanttab.cpp" "${YWB1}/section1tab.cpp" "${YWB1}/section.cpp" "${YWB1}/section9tab.cpp" "${YWB1}/auratab.cpp" "${YWB1}/soultab.cpp" "${YWB1}/youkaichecklist.cpp" "${YWB1}/medaliumtab.cpp"
 "${YWB1}/mainwindow.ui" "${YWB1}/version_dialog.ui" "${YWB1}/manualeditdialog.ui" "${YWB1}/section1tab.ui" "${YWB1}/section9tab.ui" "${YWB1}/auratab.ui" "${YWB1}/equipmenttabform.ui" "${YWB1}/importanttabform.ui" "${YWB1}/itemtabform.ui" "${YWB1}/soultabform.ui" "${YWB1}/youkaitabform.ui" "${YWB1}/listtab.ui" "${YWB1}/medaliumtab.ui" "${YWB1}/resources.qrc")
target_link_libraries(ykw_editor_b1 PRIVATE ${YKW_CRYPTOPP_TARGET})
qt_add_executable(ykw_editors_android MANUAL_FINALIZATION "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp" "${CMAKE_CURRENT_LIST_DIR}/src/launcherwindow.cpp" "${CMAKE_CURRENT_LIST_DIR}/src/launcherwindow.h")
target_link_libraries(ykw_editors_android PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
add_dependencies(ykw_editors_android ykw_editor1 ykw_editor2 ykw_editor_b1)
if(ANDROID)
 set_target_properties(ykw_editors_android PROPERTIES QT_ANDROID_PACKAGE_NAME "com.atuy.ykweditors" QT_ANDROID_APP_NAME "Yo-kai Watch Editors" QT_ANDROID_MIN_SDK_VERSION 28 QT_ANDROID_TARGET_SDK_VERSION 35 QT_ANDROID_VERSION_NAME "0.1.0" QT_ANDROID_VERSION_CODE 1 QT_ANDROID_EXTRA_LIBS "${YKW_PLUGIN_OUTPUT_DIR}/libykw_editor1.so;${YKW_PLUGIN_OUTPUT_DIR}/libykw_editor2.so;${YKW_PLUGIN_OUTPUT_DIR}/libykw_editor_b1.so")
endif()
qt_finalize_target(ykw_editors_android)
