cbuffer EditorGridConstants : register(b0)
{
	row_major float4x4 viewProjection;
	float4 minorLineColor;
	float4 majorLineColor;
	float4 axisXColor;
	float4 axisZColor;
	float4 gridParameters;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 worldPosition : TEXCOORD0;
};

float computeGridLineAlpha(float worldCoordinate, float spacing)
{
	const float safeSpacing = max(spacing, 0.0001f);
	const float scaledCoordinate = worldCoordinate / safeSpacing;
	const float derivative = max(fwidth(scaledCoordinate), 0.0001f);
	const float lineDistance = abs(frac(scaledCoordinate - 0.5f) - 0.5f) / derivative;
	return 1.0f - saturate(lineDistance);
}

float computeAxisLineAlpha(float worldCoordinate)
{
	const float derivative = max(fwidth(worldCoordinate), 0.0001f);
	return 1.0f - saturate(abs(worldCoordinate) / derivative);
}

VSOutput mainVS(uint vertexId : SV_VertexID)
{
	const float2 positions[6] = {
		float2(-1.0f, -1.0f),
		float2(-1.0f, 1.0f),
		float2(1.0f, 1.0f),
		float2(-1.0f, -1.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, -1.0f)
	};

	VSOutput output;
	const float halfExtent = max(gridParameters.z, 1.0f);
	output.worldPosition = float3(positions[vertexId].x * halfExtent, 0.0f, positions[vertexId].y * halfExtent);
	output.position = mul(float4(output.worldPosition, 1.0f), viewProjection);
	return output;
}

float4 mainPS(VSOutput input) : SV_Target0
{
	const float minorSpacing = max(gridParameters.x, 0.0001f);
	const float majorSpacing = max(gridParameters.y, minorSpacing);
	const float halfExtent = max(gridParameters.z, 1.0f);
	const float fadeWidth = max(gridParameters.w, 0.0001f);

	const float minorLineAlpha = max(computeGridLineAlpha(input.worldPosition.x, minorSpacing), computeGridLineAlpha(input.worldPosition.z, minorSpacing));
	const float majorLineAlpha = max(computeGridLineAlpha(input.worldPosition.x, majorSpacing), computeGridLineAlpha(input.worldPosition.z, majorSpacing));
	const float axisXAlpha = computeAxisLineAlpha(input.worldPosition.z);
	const float axisZAlpha = computeAxisLineAlpha(input.worldPosition.x);

	const float radialExtent = max(abs(input.worldPosition.x), abs(input.worldPosition.z));
	const float edgeAlpha = 1.0f - smoothstep(halfExtent - fadeWidth, halfExtent, radialExtent);

	float4 color = minorLineColor * minorLineAlpha;
	color = lerp(color, majorLineColor, saturate(majorLineAlpha));
	color = lerp(color, axisXColor, saturate(axisXAlpha));
	color = lerp(color, axisZColor, saturate(axisZAlpha));
	color.a *= edgeAlpha;
	return color;
}
