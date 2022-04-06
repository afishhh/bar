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
        defaultPackage = pkgs.callPackage ./default.nix { };
        devShell = self.defaultPackage.${system};
      }) // {
        overlay = final: prev: {
          bar = prev.callPackage ./default.nix { };
        };
      };
}
