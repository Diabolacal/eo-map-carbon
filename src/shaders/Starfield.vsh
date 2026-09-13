// TrinityAL vertex shader for the synthetic starfield.
// Constant-buffer bind pattern matches trinityal/tests PositionOnlyWithPerObjectData.vsh.

#pragma pack_matrix(row_major)

struct VS_INPUT
{
	float3 Position : POSITION;
	float Intensity : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float Intensity : TEXCOORD;
};

cbuffer cb0 : register(b0)
{
	float4x4 viewProj;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0), viewProj);
	output.Intensity = input.Intensity;
	return output;
}
