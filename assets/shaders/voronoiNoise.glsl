#version 450

layout(std140, set = 1, binding = 0) uniform ScreenAndUIStateBlock
{
    uvec2 screenSize;

    bool flipVertically;
    float screenScale;

    vec2 screenOffset;

    float startThreshold;
    float endThreshold;

    float amplitude;
    float octaves;
    float lacunarity;
    float reserved;

    vec4 voronoiFactors;
    vec4 startColor;
    vec4 endColor;
} ScreenAndUIState;


layout(location=0) in vec2 screenCoord;

layout(location=0) out vec4 fragColor;

/**
 * Hash function from: https://nullprogram.com/blog/2018/07/31/ and https://github.com/skeeto/hash-prospector .
 * Released by the original author on the public domain.
 */
int lowbias32(int x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

vec2 randomNoiseVector2(vec2 v)
{
    ivec2 f = ivec2(floor(v));
    ivec2 fh = ivec2(lowbias32(f.x), lowbias32(f.y));
    return vec2(
        lowbias32(fh.x * 27901 + fh.y * 8537),
        lowbias32(fh.x * 6581 + fh.y * 21881)
    ) / 4294967295.0;
}

vec4 voronoiNoiseComponents(vec2 v)
{
    vec4 result = vec4(1.0e10);
    vec2 startCell = floor(v);
    vec2 f = v - startCell;
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            vec2 cellDelta = vec2(x, y);
            vec2 cell = startCell + cellDelta;
            vec2 point = randomNoiseVector2(cell);

            vec2 delta = f - (point + cellDelta);
            float dist2 = dot(delta, delta);
            if(dist2 < result.x)
                result = vec4(dist2, result.xyz);
            else if(dist2 < result.y)
                result = vec4(result.x, dist2, result.yz);
            else if(dist2 < result.z)
                result = vec4(result.xy, dist2, result.z);
            else if(dist2 < result.w)
                result = vec4(result.xyz, dist2);
        }

    }

    return min(sqrt(result), vec4(1.0));
}

void main()
{
    float screenAspect = float(ScreenAndUIState.screenSize.y) / float(ScreenAndUIState.screenSize.x);
    vec2 viewPosition = (screenCoord - 0.5)*ScreenAndUIState.screenScale*vec2(1.0, screenAspect) - ScreenAndUIState.screenOffset;

    vec2 noiseCoordinate = viewPosition;
    float noiseGain = 1.0;
    float totalGain = 0.0;

    vec4 noiseComponents = vec4(0.0);
    int octaves = int(ScreenAndUIState.octaves);
    for(int i = 0; i < octaves; ++i)
    {
        noiseComponents += voronoiNoiseComponents(noiseCoordinate)*noiseGain;
        totalGain += noiseGain;

        noiseCoordinate *= ScreenAndUIState.lacunarity;
        noiseGain /= ScreenAndUIState.lacunarity;
    }

    noiseComponents *= ScreenAndUIState.amplitude / totalGain;

    float noiseValue = dot(noiseComponents, ScreenAndUIState.voronoiFactors);
    if (ScreenAndUIState.startThreshold <= ScreenAndUIState.endThreshold)
        noiseValue = clamp((noiseValue - ScreenAndUIState.startThreshold) / (ScreenAndUIState.endThreshold - ScreenAndUIState.startThreshold), 0.0, 1.0);
    else
        noiseValue = clamp((noiseValue - ScreenAndUIState.endThreshold) / (ScreenAndUIState.startThreshold - ScreenAndUIState.endThreshold), 0.0, 1.0);

    fragColor = mix(ScreenAndUIState.startColor, ScreenAndUIState.endColor, noiseValue);
}
