cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
include(build.cmake)

project(minstall
	VERSION 6.0
	DESCRIPTION "Gazelle Installer for antiX Linux and MX Linux"
	LANGUAGES CXX
)
# TODO: Remove for final release
set(PROJECT_VERSION "${PROJECT_VERSION}-ALPHA")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent LinguistTools Network)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

qt_standard_project_setup()

# Build the real installer (renamed from minstall to minstall-real)
qt_add_executable(minstall-real
	app.cpp
	autopart.cpp autopart.h
	base.cpp base.h
	bootman.cpp bootman.h
	checkmd5.cpp checkmd5.h
	core.cpp core.h
	crypto.cpp crypto.h
	minstall.cpp minstall.h
	mprocess.cpp mprocess.h
	msettings.cpp msettings.h
	mtreeview.cpp mtreeview.h
	oobe.cpp oobe.h
	partman.cpp partman.h
	passedit.cpp passedit.h
	replacer.cpp replacer.h
	swapman.cpp swapman.h
	images.qrc
)
qt_add_ui(minstall-real SOURCES
	meinstall.ui
)

target_link_libraries(minstall-real PRIVATE
	Qt::Core
	Qt::Gui
	Qt::Widgets
	Qt::Concurrent
	zxcvbn
)

build_compilation_setup(minstall-real)
build_version_definition(minstall-real VERSION)
build_translation_files(minstall-real translations)

target_compile_definitions(minstall-real PRIVATE
	QT_DISABLE_DEPRECATED_UP_TO=0x060800
	CODEBASE_VERSION="${PROJECT_VERSION}"
)

# Build the launcher (new minstall binary)
qt_add_executable(minstall
	launcher.cpp
	launcher.h
)

target_link_libraries(minstall PRIVATE
	Qt::Core
	Qt::Gui
	Qt::Widgets
	Qt::Network
)

build_compilation_setup(minstall)
build_version_definition(minstall VERSION)

target_compile_definitions(minstall PRIVATE
	QT_DISABLE_DEPRECATED_UP_TO=0x060800
	CODEBASE_VERSION="${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS minstall minstall-real
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
