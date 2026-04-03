struct VSInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOutput mainVS(VSInput input)
{
	VSOutput output = (VSOutput)0;
	output.position = float4(input.position, 1.0f);
	output.uv = input.uv;
	return output;
}

float4 mainPS(VSOutput input) : SV_Target0
{
	return float4(input.uv.x, input.uv.y, 0.0f, 1.0f);
}

[numthreads(8, 8, 1)]
void mainCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	(void)dispatchThreadId;
}
