# NPR engine
A non-photorealistic real-time rendering engine written in C++ using Vulkan API. Developed as part of a bachelor thesis, this project explores and demonstrates techniques for achieving stylized visuals in 3D graphics.

## Prerequisites 
- **CMake** (3.20+ recommended)
- **Ninja** (recommended build system; or any CMake-supported generator)
- **C++ Compiler** (with C++20 support)
- **Vulkan SDK** (optional, only required for debug builds)
- **glslc** (typically included in Vulkan SDK or standalone)

## Dependencies

- [`vulkan-headers`](https://github.com/KhronosGroup/Vulkan-Headers)
- [`GLFW`](https://github.com/glfw/glfw) (window and input management)
- [`glm`](https://github.com/g-truc/glm) (mathematics library)
- [`Dear ImGui`](https://github.com/ocornut/imgui) (UI toolkit)
- [`tinygltf`](https://github.com/syoyo/tinygltf) (GLTF model loader)
- [`Flecs`](https://github.com/SanderMertens/flecs) (Entity Component System)

Third-party libraries are managed automatically by CMake. Refer to the [CMakeLists.txt](./CMakeLists.txt) for details.

> **Important:**  
> At run-time, the Vulkan loader must be present on your system.  
> - On **Linux**, the Vulkan loader is typically installed via your package manager.
> - On **Windows**, it is usually provided by your graphics driver installation.

## Build & Run Instructions

> **Note:**  
> CMake presets are configured to use the Ninja build system. If you wish to use a different generator, you will need to update or create new CMake presets accordingly.

``` bash
# Debug
cmake --preset debug
cmake --build --preset debug

cd build_debug
./npr_engine

# Release
cmake --preset release
cmake --build --preset release

cd build_release
./npr_engine
```

