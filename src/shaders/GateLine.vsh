#pragma pack_matrix(row_major)

struct VS_INPUT
{
	float3 Position : POSITION;
	float Intensity : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float ViewDepth : TEXCOORD0;
	float Intensity : TEXCOORD1;
};

cbuffer cb0 : register(b0)
{
	float4x4 viewProj;
	float4 viewRight;
	float4 viewUp;
	float4 cameraPos;
	float4 sizeParams;
	float4 fadeParams;
	float4 gateParams;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0), viewProj);
	output.ViewDepth = output.Position.w;
	output.Intensity = input.Intensity;
	return output;
}
