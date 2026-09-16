#pragma pack_matrix(row_major)

struct VS_INPUT
{
	float2 Corner : TEXCOORD0;
	float3 WorldPos : POSITION;
	float Size : TEXCOORD1;
	float4 ColorEmissive : COLOR;
	float3 Region : TEXCOORD2;
};

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float2 Corner : TEXCOORD0;
	float3 Color : TEXCOORD1;
	float Intensity : TEXCOORD2;
	float Chroma : TEXCOORD3;
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
	float4 extraParams;
	float4 gateTint;
};

cbuffer cb1 : register(b1)
{
	float4 glowParams;
	float4 flareParams; // intensity, threshold, length, chroma
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = 0;
	output.Corner = input.Corner;
	output.Color = 0;
	output.Intensity = 0;
	output.Chroma = flareParams.w;

	float4 clipCenter = mul(float4(input.WorldPos, 1.0), viewProj);
	float viewZ = max(clipCenter.w, 0.05);
	float scale = max(0.1, cameraPos.w / max(fadeParams.z, 1.0));
	float dynNear = 3.0 * scale;
	float dynFar = 275.0 * scale;
	float t = saturate((viewZ - dynNear) / max(dynFar - dynNear, 0.001));
	float depthGain = lerp(fadeParams.x, fadeParams.y, t);
	float close = saturate(1.0 - t);

	float rank = input.ColorEmissive.a * fadeParams.w * depthGain * close;
	if (rank < flareParams.y || close < 0.18)
	{
		output.Position = float4(2.0, 2.0, 2.0, 1.0);
		return output;
	}

	float unclampedPx = 2.4 * sizeParams.x * input.Size * (fadeParams.z / viewZ);
	float pixels = clamp(unclampedPx, sizeParams.y, sizeParams.z);
	float worldHalf = pixels * viewZ * sizeParams.w / max(gateParams.w * 0.5, 1.0);
	float stretch = max(flareParams.z, 0.5);

	float3 world = input.WorldPos
		+ viewRight.xyz * (input.Corner.x * worldHalf * stretch)
		+ viewUp.xyz * (input.Corner.y * worldHalf * 0.22);

	float lum = dot(input.ColorEmissive.rgb, float3(0.2126, 0.7152, 0.0722));
	float3 chroma = saturate(lum + (input.ColorEmissive.rgb - lum) * viewRight.w);
	chroma = lerp(chroma, input.Region, saturate(extraParams.y));

	output.Position = mul(float4(world, 1.0), viewProj);
	output.Corner = input.Corner;
	output.Color = chroma;
	output.Intensity = fadeParams.w * depthGain * close * flareParams.x * (0.20 + 0.15 * input.ColorEmissive.a);
	return output;
}
