# Vulkan Lab
A modern, lightweight sandbox for learning and experimenting with Vulkan. Primarily built on Linux.

![Image showing window with coloured triangle drawn in the middle.](screenshot.png)

# Requirements
- Vulkan SDK
- CMake >= 3.28
- C++ 23 supporting compiler (e.g. GCC 13+)

# Build instructions
Using the provided platform scripts, for example:
```
# Configure and build (add -d for debug)
./build-linux.sh

# Clean build
./build-linux.sh -c
```

Alternatively, invoke CMake directly, for example:
```
cmake -B build
cmake --build build
```

# Running the demo
### Linux
```
./build/Release/bin/VulkanDemo
```

## License
This project is licensed under the MIT license (see [LICENSE](LICENSE))

This project makes use of the following 3rd party libraries:
- GLFW
- glm
- spdlog
- stb
- tinygltf
