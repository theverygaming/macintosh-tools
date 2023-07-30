with import <nixpkgs> {};
let
  gccForLibs = stdenv.cc.cc;
in stdenv.mkDerivation {
  name = "macintosh-tools";
  buildInputs = [
    ninja
    cmake
  ];
}
