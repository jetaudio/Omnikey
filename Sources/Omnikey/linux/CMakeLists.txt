cmake_minimum_required(VERSION 3.16)
project(omnikey VERSION 2.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED IMPORTED_TARGET ibus-1.0)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.5)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)
set(OMNIKEY_ENGINE_PATH ${CMAKE_INSTALL_FULL_LIBEXECDIR}/ibus-engine-omnikey)
set(OMNIKEY_SETUP_PATH ${CMAKE_INSTALL_FULL_BINDIR}/omnikey-setup)
set(OMNIKEY_ICON_PATH ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps/io.github.jetaudio.Omnikey.svg)

# The shared Omnikey engine plus the Linux settings store.
add_library(omnikey-core STATIC
    ${ENGINE_DIR}/Engine.cpp
    ${ENGINE_DIR}/Vietnamese.cpp
    ${ENGINE_DIR}/Macro.cpp
    ${ENGINE_DIR}/SmartSwitchKey.cpp
    ${ENGINE_DIR}/ConvertTool.cpp
    src/common/Config.cpp)
target_include_directories(omnikey-core PUBLIC ${ENGINE_DIR} src/common)
target_compile_definitions(omnikey-core PUBLIC LINUX OMNIKEY_VERSION="${PROJECT_VERSION}")
target_compile_options(omnikey-core PRIVATE -Wno-deprecated-declarations)
target_link_libraries(omnikey-core PUBLIC PkgConfig::IBUS)

add_executable(ibus-engine-omnikey src/ibus/main.cpp src/ibus/OmnikeyEngine.cpp src/ibus/VirtualKeyboard.cpp)
target_compile_definitions(ibus-engine-omnikey PRIVATE OMNIKEY_SETUP_PATH="${OMNIKEY_SETUP_PATH}")
target_link_libraries(ibus-engine-omnikey PRIVATE omnikey-core PkgConfig::IBUS)

add_executable(omnikey-setup src/setup/main.cpp)
target_link_libraries(omnikey-setup PRIVATE omnikey-core PkgConfig::ADWAITA)

configure_file(data/omnikey.xml.in ${CMAKE_CURRENT_BINARY_DIR}/omnikey.xml @ONLY)

install(TARGETS ibus-engine-omnikey DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS omnikey-setup DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/omnikey.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component)
install(FILES data/io.github.jetaudio.Omnikey.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/icons/io.github.jetaudio.Omnikey.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
# udev rules live in /usr/lib on Ubuntu, regardless of the install prefix.
install(FILES data/70-omnikey-uinput.rules DESTINATION lib/udev/rules.d)

# cpack -G DEB builds an installable package for Ubuntu.
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME ibus-omnikey)
set(CPACK_PACKAGE_CONTACT "jetaudio <jetaudio.media@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Omnikey Vietnamese input method for IBus")
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "ibus")
# The package was called ibus-openkey before the rename.
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "ibus-openkey")
set(CPACK_DEBIAN_PACKAGE_REPLACES "ibus-openkey")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_CURRENT_SOURCE_DIR}/data/debian/postinst)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
include(CPack)
