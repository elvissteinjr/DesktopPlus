Texture2D tx : register( t0 );
SamplerState samLinear : register( s0 );

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	float4 color;
	color = tx.Sample(samLinear, input.Tex);
	color.a = (color.a > 0.0) ? 1.0 : 0.0;   //Enforce 1-bit alpha as on some systems the duplication output isn't opaque for some reason
	return color;
}