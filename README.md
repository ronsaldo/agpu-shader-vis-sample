# AbstractGPU - Shader Visualization Sample

## Building

For building on Linux the Vulkan header files and the SDL2 development must be installed. This project must be cloned with the submodules in order to build the bundled version of abstract-gpu. A similar procedure is needed for building on Windows and Mac, where the SDL2 library must be specified. 

For recursive cloning with submodules, the following script can be used:

```bash
git clone --recurse https://github.com/ronsaldo/agpu-shader-vis-sample
```

The following bash script can be used for building on Linux:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Running

The building process produces the following build artifacts:

- *dist/ShaderVis* The shader visualization sample that displays an interactive Voronoi noise.
