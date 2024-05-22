struct Vertex_Input {
    float2 position : POSITION;
};

struct Pixel_Input {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

Pixel_Input vs_main(Vertex_Input input) {
    Pixel_Input ps;
    ps.position = float4(input.position, 0.0, 1.0);
    ps.color    = float3(input.position + 0.5, 0.0);
    return ps;
}

float4 ps_main(Pixel_Input input) : SV_TARGET {
    return float4(input.color.x, input.color.y, 0, 1);
}
