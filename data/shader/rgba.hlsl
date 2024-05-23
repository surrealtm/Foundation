struct Vertex_Input {
    float2 position : POSITION;
    float2 uv : UV;
};

struct Pixel_Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float3 color;

Texture2D albedo;
SamplerState albedo_sampler;

Pixel_Input vs_main(Vertex_Input input) {
    Pixel_Input output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv    = input.uv;
    return output;
}

float4 ps_main(Pixel_Input input) : SV_TARGET {
    return albedo.Sample(albedo_sampler, input.uv) * float4(color, 1);
}
