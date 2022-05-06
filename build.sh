#!/usr/bin/env bash

# Why is this script here
# I have no idea, I was bored.
# TODO: Allow adding arbitrary extra options to cmake commandline
# TODO: Allow adding arbitrary extra options to cmake --build commandline

set -euo pipefail
IFS=$'\n\t'

function err() {
	echo "$0: $@" >&2
	exit 1
}

opts=$(getopt -a -n "$0" -o "t:,h,j:,g:" -l "type:,help,jobs:,no-dwmipc,generator:" -- "$@")
eval set -- "$opts"

[[ $? -ne 0 ]] && exit 1

type=release
no_dwmipc=false
generator="Unix Makefiles"

if command -v nproc >/dev/null 2>&1; then
	cores=$(nproc)
	has_nproc=1
else
	cores=1
	has_nproc=
fi

while true; do
	case "$1" in
		-t|--type)
			type="$2"
			if [[ "$type" != "debug" && "$type" != "release" ]]; then
				err "invalid build type: $type"
				exit 1
			fi
			shift 2
			;;
		-j|--jobs)
			cores="$2"
			if [[ ! "$cores" =~ ^[0-9]+$ ||
				"$cores" -lt 1 ||
				($has_nproc && $cores -gt $(nproc))
			]]; then	
				err "invalid number of cores: $cores"
				exit 1
			fi
			shift 2
			;;
		--no-dwmipc)
			no_dwmipc=true
			shift
			;;
		-g|--generator)
			generator="$2"
			shift 2
			;;
		-h|--help)
			echo "Usage: $0 [OPTION]..."
			echo "  -t, --type <type>  Specify build type (default: release)"
			echo "  -h, --help         Print this help text"
			echo "  -g, --generator    Specify CMake generator (default: Unix Makefiles)"
			echo "  -j, --jobs <num>   Specify number of jobs to run in parallel (1 <= num$([[ $has_nproc ]] && echo " <= $(nproc)")) (default: $cores)"
			echo "  --no-dwmipc        Disable dwm-ipc patch integration"

			exit 0
			;;
		--)
			shift
			break
			;;
		*)
			err "unhandled option: '$1', this is a bug!"
			break
	esac
done

if [[ $# -ne 0 ]]; then
	quoted_args=()
	while [[ $# -gt 0 ]]; do
		quoted_args+=("'$1'")
		shift
	done
	err "excess positional arguments: ${quoted_args[@]}"
	exit 1
fi

infohash=$(echo "$type$no_dwmipc$generator" | sha1sum)
if [[ -e build && ! -d build ]]; then
	err "build exists but is not a directory"
	exit 1
elif [[ ! -e build ]]; then
	mkdir build
else
	previous_infohash=$(cat build/_buildinfo 2>/dev/null || true)
	if [[ "$previous_infohash" != "$infohash" ]]; then
		rm -rf build/*
	fi
fi
echo "$infohash" > build/_buildinfo

cd build

cmake_opts=()

if [[ "$type" == "debug" ]]; then
	cmake_opts+=(
		-DCMAKE_BUILD_TYPE=Debug
		-DCMAKE_CXX_FLAGS="-pg -g"
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1
		-DCMAKE_BUILD_TYPE=Debug
	)
elif [[ "$type" == "release" ]]; then
	cmake_opts+=(
		-DCMAKE_BUILD_TYPE=Release
	)
fi

[[ $no_dwmipc == false ]] &&
	cmake_opts+=(-DDWMIPC=ON)

cmake -G"$generator" .. ${cmake_opts[@]} ||
	err "cmake generation failed"

[[ $type == "debug" ]] &&
	mv compile_commands.json ..

cmake --build . -j$cores ||
	err "cmake build failed"
