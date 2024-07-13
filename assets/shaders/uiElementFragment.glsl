#version 450

layout (set=0, binding = 0) uniform sampler textureSampler;
layout (set=1, binding = 2) uniform texture2D fontTexture;

layout(location=0) flat in vec4 inColor;
layout(location=1) in vec2 inQuadCoord;
layout(location = 2) flat in uint inIsGlyph;
layout(location = 3) in vec2 inGlyphCoord;

layout(location=0) out vec4 fragColor;

void main()
{
    // Premultiplied alpha
    vec4 color = vec4(inColor.rgb * inColor.a, inColor.a);

    if(inIsGlyph != 0)
    {
        vec4 glyphColor = texture(sampler2D(fontTexture, textureSampler), inGlyphCoord);
        color *= glyphColor;
    }

    fragColor = color;
}
