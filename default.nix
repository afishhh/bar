{ stdenv
, cmake
, pkg-config
, libX11
, libXft

  # libXft deps
, libXext
, libXrender
, fontconfig
, freetype

, configFile ? ./src/config.hh
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
  ];

  patchPhase = "rm src/config.hh; ln -s ${configFile} src/config.hh";
  installPhase = "mkdir -p $out/bin/; cp bar $out/bin/fbar";
}
