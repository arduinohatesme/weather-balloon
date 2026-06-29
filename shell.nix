let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-26.05";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in

pkgs.mkShell {
  buildInputs = [
    pkgs.arduino-cli
    pkgs.arduino-language-server
    pkgs.clang-tools
    pkgs.python312
  ];
}
