
with import <nixpkgs> {};
gcc16Stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [
    vulkan-headers
    cmake
    gcc16
    mold
  ];

  buildInputs = [
    gdb
    clang_22
    vulkan-loader
    vulkan-tools
    vulkan-validation-layers
    spirv-tools
    shaderc
    wayland
    wayland-protocols
    libxkbcommon
    kdePackages.extra-cmake-modules
    libGL
    libffi
    renderdoc
    libx11
    libx11.dev
    libxrandr.dev
    libxinerama.dev
    libxcursor.dev
    libxi.dev
    libxext.dev
  ];

  shellHook = ''
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${lib.strings.makeLibraryPath [ libGL vulkan-loader libxkbcommon wayland libx11 ]}
  '';
}
