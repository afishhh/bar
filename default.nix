{ pkgs
, gcc13Stdenv
, cmake
, ninja
, pkg-config
, libX11
, libXext
, libXrandr

, fmt

  # libXft deps
, libXrender
, fontconfig
, freetype

, dwmipcpp-src
  # For dwmipcpp
, jsoncpp

, pango
, glib
, glfw
, glad2 ? pkgs.python3Packages.glad2
, cairo

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
    libX11
    libXext
    fmt.dev
    libXrender
    libXrandr
    fontconfig
    freetype
    jsoncpp
    pango
    glib
    glfw
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
}
