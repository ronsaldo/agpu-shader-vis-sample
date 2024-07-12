set(ShaderVis_Sources
    ShaderVis.cpp
)

add_executable(ShaderVis ${ShaderVis_Sources})
target_link_libraries(ShaderVis Agpu ${SDL2_LIBRARY})
