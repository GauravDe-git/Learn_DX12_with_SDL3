//================
// Data Structures
//================

// Incoming vertex data - from the C++ (CPU)
struct VertexIn
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
};

// Outgoing vertex struct - connecting to the Pixel Shader
struct VertexOut
{
    float4 Position : SV_Position;
	float4 Color	: COLOR;
};

//===================
// Vertex Shader (VS)
//===================
// This runs once per vertex (point).
VertexOut VS_Main(VertexIn vs_In)
{
    VertexOut vsOutput;

    vsOutput.Position = float4(vs_In.Position, 1.0f);
    vsOutput.Color    = vs_In.Color;

    return vsOutput;
}

//------------------
// PIXEL SHADER (PS)
//------------------
// Runs once per pixel on the screen that overlaps triangle
// "input" is the data from the Vertex Shader
// GPU automatically blends (interpolates) the values among the vertices

float4 PS_Main(VertexOut input) : SV_Target 
{
    // for now, simply return the color calculated on the vertex shader
    return input.Color;
}

/*float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}*/