cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
include(build.cmake)

project(minstall
	VERSION 6.0
	DESCRIPTION "Gazelle Installer for antiX Linux and MX Linux"
	LANGUAGES CXX
)

option(USE_ZXCVBN "Use zxcvbn for password strength estimation" ON)
set(GAZELLE_VERSION_SOURCE "debian_changelog" CACHE STRING
	"Source for VERSION compile definition: debian_changelog or tag"
)
set_property(CACHE GAZELLE_VERSION_SOURCE PROPERTY STRINGS debian_changelog tag)
set(GAZELLE_VERSION_TAG "" CACHE STRING
	"Explicit tag version (used when GAZELLE_VERSION_SOURCE=tag)"
)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent LinguistTools)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
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
	throbber.h throbber.cpp
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
	Qt::Concurrent
)

if(USE_ZXCVBN)
	target_link_libraries(minstall PRIVATE zxcvbn)
	target_compile_definitions(minstall PRIVATE WITH_ZXCVBN)
endif()

build_compilation_setup(minstall)
if(GAZELLE_VERSION_TAG STREQUAL "")
	build_version_definition(minstall VERSION
		SOURCE "${GAZELLE_VERSION_SOURCE}"
	)
else()
	build_version_definition(minstall VERSION
		SOURCE "${GAZELLE_VERSION_SOURCE}"
		TAG "${GAZELLE_VERSION_TAG}"
	)
endif()
build_translation_files(minstall translations)

target_compile_definitions(minstall PRIVATE
	QT_DISABLE_DEPRECATED_UP_TO=0x060800
	CODEBASE_VERSION="${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS minstall
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
