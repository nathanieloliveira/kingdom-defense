struct Input
{
    float2 xy_pos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct Output
{
    float4 Position : SV_Position;
    float4 Color : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

Output main(Input input)
{
    Output output;
    output.Color = input.color;
    output.UV = input.uv;
    output.Position = float4(input.xy_pos, 0.0f, 1.0f);
    return output;
}
