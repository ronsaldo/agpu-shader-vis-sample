#version 450

layout(std140, set = 1, binding = 0) uniform ScreenAndUIStateBlock
{
    uvec2 screenSize;
    bool flipVertically;
    float screenScale;
    vec2 screenOffset;
} ScreenAndUIState;

layout(location = 0) out vec2 outTexcoord;

const vec2 screenQuadVertices[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2(-1.0, 3.0),
    vec2(3.0, -1.0)
);

void main()
{
    outTexcoord = screenQuadVertices[gl_VertexIndex]*0.5 + 0.5;
    if(!ScreenAndUIState.flipVertically)
        outTexcoord.y = -outTexcoord.y;
    gl_Position = vec4(screenQuadVertices[gl_VertexIndex], 0.0, 1.0);
}