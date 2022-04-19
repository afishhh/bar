{ stdenv
, cmake
, pkg-config
, libX11
, libXft
, libXext

  # libXft deps
, libXrender
, fontconfig
, freetype

, dwmipcpp-src
# For dwmipcpp
, jsoncpp

, configFile ? ./src/config.def.hh
, ...
}:

stdenv.mkDerivation {
  name = "fbar";
  version = "0.1.0";

  src = ./.;
  nativeBuildInputs = [
    cmake
    pkg-config
  ];
  buildInputs = [
    libX11
    libXft
    libXext
    libXrender
    fontconfig
    freetype
    jsoncpp
  ];

  cmakeFlags = [
    "-DDWMIPC=ON"
    "-DFETCHCONTENT_SOURCE_DIR_DWMIPCPP=${dwmipcpp-src}"
  ];

  patchPhase = "ln -s ${configFile} src/config.hh";
  installPhase = "mkdir -p $out/bin/; cp bar $out/bin/fbar";
}
