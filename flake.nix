{
  description = "A basic flake";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    with flake-utils.lib;
    eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      with pkgs.lib; {
        devShell = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [ bashInteractive ];
          buildInputs = with pkgs; [
            cmake
            pkg-config
            xorg.libX11.dev
            xorg.libXft

            # libXft deps
            xorg.libXext
            xorg.libXrender
            fontconfig
            freetype
          ];
        };
      });
}
