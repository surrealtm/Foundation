struct Vertex_Input {
    float2 position : POSITION;
    float2 uv : UV;
};

struct Pixel_Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer Text_Constants {
    float4x4 projection;
    float3 foreground_color;
};

Texture2D albedo;
SamplerState albedo_sampler;

Pixel_Input vs_main(Vertex_Input input) {
    Pixel_Input output;
    output.position    = mul(projection, float4(input.position, 0.0f, 1.0f));
    output.position.xy = float2(2.f * output.position.x - 1.f, 1.f - 2.f * output.position.y);
    output.uv          = input.uv;
    return output;
}

float4 ps_main(Pixel_Input input) : SV_TARGET {
    return albedo.Sample(albedo_sampler, input.uv) * float4(foreground_color, 1.f);
}
