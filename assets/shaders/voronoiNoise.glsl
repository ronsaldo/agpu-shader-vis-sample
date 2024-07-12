#version 450

layout(std140, set = 1, binding = 0) uniform ScreenAndUIStateBlock
{
    uvec2 screenSize;
    bool flipVertically;
    float screenScale;
    vec2 screenOffset;
} ScreenAndUIState;

layout(location=0) in vec2 screenCoord;

layout(location=0) out vec4 fragColor;

void main()
{
    float screenAspect = float(ScreenAndUIState.screenSize.y) / float(ScreenAndUIState.screenSize.x);
    vec2 viewPosition = (screenCoord - 0.5)*ScreenAndUIState.screenScale*vec2(1.0, screenAspect) - ScreenAndUIState.screenOffset;
    fragColor = vec4(fract(viewPosition), 0.0, 1.0);
}
