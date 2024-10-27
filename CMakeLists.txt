cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(ezremoteclient)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "ezRemote Client")
set(VITA_TITLEID  "RMTCLI001")
set(VITA_VERSION  "01.06")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wno-psabi")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)

include_directories(
  src
)

add_executable(${PROJECT_NAME}
  src/clients/apache.cpp
  src/clients/archiveorg.cpp
  src/clients/baseclient.cpp
  src/clients/ftpclient.cpp
  src/clients/iis.cpp
  src/clients/myrient.cpp
  src/clients/nfsclient.cpp
  src/clients/nginx.cpp
  src/clients/npxserve.cpp
  src/pugixml/pugixml.cpp
  src/clients/smbclient.cpp
  src/clients/rclone.cpp
  src/clients/webdav.cpp
  src/textures.cpp
  src/fs.cpp
  src/main.cpp
  src/inifile.c
  src/config.cpp
  src/ime_dialog.cpp
  src/net.cpp
  src/gui.cpp
  src/installer.cpp
  src/windows.cpp
  src/actions.cpp
  src/lang.cpp
  src/sfo.cpp
  src/sha1.c
  src/zip.c
  src/ioapi.c
  src/debugScreen.c
  src/zip_util.cpp
)

target_link_libraries(${PROJECT_NAME}
  c
  imgui
  vita2d
  vitaGL
  vitashark
  mathneon
  httpclient
  archive
  bz2
  lzma
  xml2
  stdc++
  lexbor_static
  png
  z
  curl
  ssl
  crypto
  pthread
  nfs
  smb2
  ssh2
  SceCommonDialog_stub
  SceLibKernel_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceSysmodule_stub
  SceVshBridge_stub 
  SceCtrl_stub
  SceTouch_stub
  SceAudio_stub
  SceHid_stub
  SceNet_stub
  SceNetCtl_stub
  SceHttp_stub
  SceSsl_stub
  ScePspnetAdhoc_stub
  ScePromoterUtil_stub
  SceShellSvc_stub
  ScePower_stub
  m
  SceAppMgr_stub
  SceAppUtil_stub
  SceShaccCgExt
  taihen_stub
  SceShaccCg_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/pic0.png sce_sys/pic0.png
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE head.bin head.bin
  FILE icons/connect.png icons/connect.png
  FILE icons/disconnect.png icons/disconnect.png
  FILE icons/search.png icons/search.png
  FILE icons/refresh.png icons/refresh.png
  FILE icons/square.png icons/square.png
  FILE icons/circle.png icons/circle.png
  FILE icons/triangle.png icons/triangle.png
  FILE icons/cross.png icons/cross.png
  FILE icons/folder.png icons/folder.png
  FILE icons/file.png icons/file.png
  FILE icons/update.png icons/update.png
  FILE icons/catalog.png icons/catalog.png
  FILE lang/English.ini lang/English.ini
  FILE lang/Spanish.ini lang/Spanish.ini
  FILE lang/Dutch.ini lang/Dutch.ini
  FILE lang/French.ini lang/French.ini
  FILE lang/German.ini lang/German.ini
  FILE lang/Italiano.ini lang/Italiano.ini
  FILE lang/Polish.ini lang/Polish.ini
  FILE lang/Portuguese_BR.ini lang/Portuguese_BR.ini
  FILE lang/Russian.ini lang/Russian.ini
  FILE lang/Korean.ini lang/Korean.ini
  FILE lang/Chinese_Simplified.ini lang/Chinese_Simplified.ini
  FILE lang/Chinese_Traditional.ini lang/Chinese_Traditional.ini
  FILE lang/Japanese.ini lang/Japanese.ini
  FILE lang/Catalan.ini lang/Catalan.ini
  FILE lang/Euskera.ini lang/Euskera.ini
  FILE lang/Galego.ini lang/Galego.ini
  FILE lang/Croatian.ini lang/Croatian.ini
  FILE lang/Indonesian.ini lang/Indonesian.ini
  FILE lang/Arabic.ini lang/Arabic.ini
  FILE lang/Greek.ini lang/Greek.ini
  FILE lang/Hungarian.ini lang/Hungarian.ini
  FILE lang/Thai.ini lang/Thai.ini
  FILE lang/Turkish.ini lang/Turkish.ini
  FILE lang/Vietnamese.ini lang/Vietnamese.ini
  FILE lang/Roboto_ext.ttf lang/Roboto_ext.ttf
  FILE certs/cacert.pem certs/cacert.pem
  FILE version.txt version.txt
)
