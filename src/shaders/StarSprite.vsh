// Instanced camera-facing star disc. Corner in stream 0, instance in stream 1.
// View right/up and pixel clamp come from the host constant buffer.

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
};

cbuffer cb0 : register(b0)
{
	float4x4 viewProj;
	float4 viewRight; // xyz = camera right, w = star colour saturation
	float4 viewUp;
	float4 cameraPos;   // xyz, w = orbit distance
	float4 sizeParams;  // x=sizeMul, y=minPx, z=maxPx, w=tanHalfFovY
	float4 fadeParams;  // x=nearGain, y=farGain, z=refDistance, w=brightnessMul
	float4 gateParams;  // x=opacity, y=distAtten, z=viewportW, w=viewportH
	float4 extraParams; // x=depthDesat, y=regionMix
	float4 gateTint;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 clipCenter = mul(float4(input.WorldPos, 1.0), viewProj);
	float viewZ = max(clipCenter.w, 0.05);

	float scale = max(0.1, cameraPos.w / max(fadeParams.z, 1.0));
	float dynNear = 3.0 * scale;
	float dynFar = 275.0 * scale;
	float t = saturate((viewZ - dynNear) / max(dynFar - dynNear, 0.001));
	float depthGain = lerp(fadeParams.x, fadeParams.y, t);

	float unclampedPx = 2.4 * sizeParams.x * input.Size * (fadeParams.z / viewZ);
	float coverage = saturate(unclampedPx / max(sizeParams.y, 0.001));
	float pixels = clamp(unclampedPx, sizeParams.y, sizeParams.z);
	float worldHalf = pixels * viewZ * sizeParams.w / max(gateParams.w * 0.5, 1.0);

	float3 world = input.WorldPos
		+ viewRight.xyz * (input.Corner.x * worldHalf)
		+ viewUp.xyz * (input.Corner.y * worldHalf);

	float lum = dot(input.ColorEmissive.rgb, float3(0.2126, 0.7152, 0.0722));
	float3 chroma = saturate(lum + (input.ColorEmissive.rgb - lum) * viewRight.w);
	chroma = lerp(chroma, input.Region, saturate(extraParams.y));
	chroma = lerp(chroma, lum.xxx, saturate(t * extraParams.x));
	float coreGain = 0.82 + 0.18 * input.ColorEmissive.a;

	output.Position = mul(float4(world, 1.0), viewProj);
	output.Corner = input.Corner;
	output.Color = chroma;
	output.Intensity = fadeParams.w * depthGain * coverage * coreGain;
	return output;
}
