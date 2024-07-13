#version 450

layout(std140, set = 1, binding = 0) uniform ScreenAndUIStateBlock
{
    uvec2 screenSize;

    bool flipVertically;
    float screenScale;

    vec2 screenOffset;

    vec2 reserved;

    vec4 voronoiFactors;
    vec4 startColor;
    vec4 endColor;
} ScreenAndUIState;

struct UIElementQuad
{
    vec2 position;
    vec2 size;
    vec4 color;
    
    bool isGlyph;
    uint reserved;
    uvec2 reserved2;

    vec2 fontPosition;
    vec2 fontSize;
};

layout(std430, set = 1, binding = 1) buffer UIDataBufferBlock
{
    UIElementQuad UIDataBuffer[];
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outQuadCoord;
layout(location = 2) flat out uint outIsGlyph;
layout(location = 3) out vec2 outGlyphCoord;

const vec2 quadVertices[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

void main()
{
    // Fetch the instance data.
    UIElementQuad quad = UIDataBuffer[gl_InstanceIndex];

    // Pass the instance color
    outColor = quad.color;

    // Fetch the quad vertex
    vec2 quadCoord = quadVertices[gl_VertexIndex];
    outQuadCoord = quadCoord;

    // Position in screen   
    vec2 position = quad.position + quad.size*quadCoord;

    // Glyph for text rendering
    outIsGlyph = quad.isGlyph ? 1 : 0;
    outGlyphCoord = quad.fontPosition + quad.fontSize*quadCoord;

    gl_Position = vec4(position / vec2(ScreenAndUIState.screenSize)*2.0 - 1.0, 0.0, 1.0);
    if(ScreenAndUIState.flipVertically)
        gl_Position.y = -gl_Position.y;
}