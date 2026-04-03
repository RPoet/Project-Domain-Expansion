cbuffer DrawConstants : register(b0)
{
	row_major float4x4 worldViewProjection;
	float4 baseColor;
};

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 localPosition : TEXCOORD0;
	float3 normal : TEXCOORD1;
	float2 uv : TEXCOORD2;
};

VSOutput mainVS(VSInput input)
{
	VSOutput output;
	output.position = mul(float4(input.position, 1.0f), worldViewProjection);
	output.localPosition = input.position;
	const float inputNormalLengthSquared = dot(input.normal, input.normal);
	output.normal = inputNormalLengthSquared > 0.0f ? normalize(input.normal) : float3(0.0f, 1.0f, 0.0f);
	output.uv = input.uv;
	/* MATERIAL_VERTEX_EDIT */
	return output;
}

float4 mainPS(VSOutput input) : SV_Target0
{
	float4 materialColor = baseColor;
	/* MATERIAL_PIXEL_EDIT */
	return materialColor;
}
