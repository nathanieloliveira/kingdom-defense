Texture2DArray myTexture;
SamplerState mySampler;

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    uint ArraySliceIndex : TEXCOORD1; // Pass the desired array slice index
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    // Sample the texture array using the provided texture coordinates and array slice index
    float4 color = myTexture.Sample(mySampler, float3(input.TexCoord, input.ArraySliceIndex));
    return color;
}