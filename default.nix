{ gcc13Stdenv
, cmake
, ninja
, pkg-config
, libX11
, libXft
, libXext

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
  ];
  buildInputs = [
    libX11
    libXft
    libXext
    fmt.dev
    libXrender
    fontconfig
    freetype
    jsoncpp
    pango
    glib
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
