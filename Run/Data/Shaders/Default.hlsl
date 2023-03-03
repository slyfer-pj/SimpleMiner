struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

struct v2p_t
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

cbuffer CameraConstants : register(b2)
{
	float4x4 projectionMatrix;
	float4x4 viewMatrix;
}

cbuffer ModelConstants : register(b3)
{
	float4x4 modelMatrix;
	float4 modelColor;
}

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition.x, input.localPosition.y, input.localPosition.z, 1.f);
	float4 modelSpacePos = mul(modelMatrix, localPosition);
	float4 viewSpacePos = mul(viewMatrix, modelSpacePos);
	float4 ndcPos = mul(projectionMatrix, viewSpacePos);
	v2p_t v2p;
	v2p.position = ndcPos;
	v2p.color = input.color;
	v2p.uv = input.uv;
	return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
	float4 tint = input.color * modelColor;
	float4 output = float4(diffuseTexture.Sample(diffuseSampler, input.uv) * tint);
	if (output.w == 0)
	{
		discard;
	}
	return output;
}
