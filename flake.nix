{
  description = "The weather balloon development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; config = {}; overlays = []; };
    in
    {
      packages.${system}.default = pkgs.buildEnv {
        name = "weather-balloon-environment";
        paths = with pkgs; [
          arduino-cli
          arduino-language-server
          clang-tools
          python312
        ];
      };
    };
}
