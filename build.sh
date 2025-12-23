#!/bin/bash
# Build script - see below for command information.
set -e

# Parse options
PRESET="default"
ARCH_MODE=false

while [[ $# -gt 0 ]]; do
	case $1 in
		--arch)
			PRESET="arch"
			ARCH_MODE=true
			shift
			;;
		*)
			break
			;;
	esac
done

case "${1:-all}" in
	clean)
		echo "Performing ultimate clean..."
		rm -rf _build_
		;;

	configure)
		echo "Configuring project..."
		cmake --preset $PRESET
		;;

	make-clean)
		echo "Cleaning build artifacts..."
		cmake --build --preset $PRESET --target clean
		;;

	make)
		echo "Building project..."
		cmake --build --preset $PRESET
		;;

	arch)
		echo "Building Arch Linux package..."
		rm -rf _build_/arch
		mkdir -p _build_/arch
		PKGDEST="$(pwd)/_build_/arch" BUILDDIR="$(pwd)/_build_/arch" makepkg -f --cleanbuild --syncdeps --noconfirm --needed
		echo "Package (and build artifacts) are in _build_/arch"
		;;

	all)
		echo "Configuring and building project..."
		cmake --workflow --preset $PRESET
		;;

	fresh)
		echo "Fresh build (clean first, then configure and build)..."
		cmake --workflow --preset $PRESET --fresh
		;;

	*)
		echo "Usage: $0 [--arch] [command]"
		echo "Options:"
		echo "  --arch        Use Arch Linux preset (disables zxcvbn)"
		echo "Commands:"
		echo "  clean        - Ultimate clean (rm -rf build)"
		echo "  configure    - Configure only"
		echo "  make-clean   - Clean build artifacts only"
		echo "  make         - Build only"
		echo "  arch         - Build using the Arch preset and create an Arch package"
		echo "  all          - Configure and build (default)"
		echo "  fresh        - Clean first then configure and build"
		exit 1
		;;
esac
