{ pkgs
, gcc13Stdenv
, cmake
, ninja
, pkg-config

, libuv

, libX11
, libXext
, libXrandr

, fmt

, dwmipcpp-src
  # For dwmipcpp
, jsoncpp

, glib
, pango
, fontconfig
, freetype
, cairo

, glfw
, wayland-scanner
, glad2 ? pkgs.python3Packages.glad2

, libGL

, configFile ? ./src/config.def.hh
, ...
}:

gcc13Stdenv.mkDerivation {
  name = "fbar";
  version = "0.1.0";

  src = ./.;
  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    glad2
  ];
  buildInputs = [
    libuv
    libX11
    libXext
    fmt.dev
    libXrandr
    fontconfig
    freetype
    jsoncpp
    pango
    glib
    (pkgs.enableDebugging (glfw.overrideAttrs (src: {
      patches = [
        # Why is this not a list...
        src.patches
        ./glfw.patch
      ];
    })))
    wayland-scanner.dev
    cairo
  ];

  patchPhase = ''
    ln -s ${configFile} src/config.hh
  '';

  cmakeFlags = [
    "-DDWMIPC=ON"
    "-DFETCHCONTENT_SOURCE_DIR_DWMIPCPP=${dwmipcpp-src}"
    ''-DCMAKE_INSTALL_PREFIX=''${out}''
  ];

  shellHook = ''
    export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [ libGL ]}
  '';
}
