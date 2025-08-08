struct Input
{
    float2 vertex_pos : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct Output
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.Position = float4(input.vertex_pos, 0.0f, 1.0f);
    output.Color = input.color;
    return output;
}