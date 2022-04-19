{
  description = "A basic flake";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.dwmipcpp-src = {
    url = github:mihirlad55/dwmipcpp/6b6947fd63845c8239f0a895be695bf206eaae6d;
    flake = false;
  };

  outputs = inputs@{ self, nixpkgs, flake-utils, ... }:
    with flake-utils.lib;
    eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      with pkgs.lib; {
        defaultPackage = pkgs.callPackage ./default.nix inputs;
        devShell = self.defaultPackage.${system};
      }) // {
        overlay = final: prev: {
          bar = prev.callPackage ./default.nix inputs;
        };
      };
}
