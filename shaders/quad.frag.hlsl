Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 mix(float4 x, float4 y, float a)
{
    return x * (1.0 - a) + y * a;
}

float4 main(float4 Color : TEXCOORD0, float2 TexCoord : TEXCOORD1) : SV_Target0
{
    float4 sampled = Texture.Sample(Sampler, TexCoord);
    return mix(Color, sampled, 0.5f);
}
