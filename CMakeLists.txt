cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
include(build.cmake)

project(minstall
	VERSION 6.0
	DESCRIPTION "Gazelle Installer for antiX Linux and MX Linux"
	LANGUAGES CXX
)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

qt_standard_project_setup()

qt_add_executable(minstall
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
qt_add_ui(minstall SOURCES
	meinstall.ui
)
#target_sources(minstall PRIVATE FILE_SET CXX_MODULES FILES
#	TODO: add files that are C++20 modules.
#)

target_link_libraries(minstall PRIVATE
	Qt::Core
	Qt::Gui
	Qt::Widgets
	zxcvbn
)

build_compilation_setup(minstall)
build_version_definition(minstall VERSION)
build_translation_files(minstall translations)

target_compile_definitions(minstall PRIVATE
	QT_DISABLE_DEPRECATED_UP_TO=0x060800
	CODEBASE_VERSION="${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS minstall
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
