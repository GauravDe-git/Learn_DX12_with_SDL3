//================
// Data Structures
//================

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
// We use a trick: SV_VertexID is a counter (0, 1, 2) provided by the GPU.

VertexOut VS_Main(uint vertex_id : SV_VertexID)
{
    VertexOut vsOutput;

    // Define 3 points of the triangle (roughly in center of screen)
	// Coords = X, Y, Z, W
    // -1.0 (left,bottom) to +1.0 (top,right)
    float4 positions[3] =
    {
        float4(0.0f, 0.5f, 0.0f, 1.0f), // Top Center
        float4(0.5f, -0.5f, 0.0f, 1.0f), // Bottom Right
        float4(-0.5f, -0.5f, 0.0f, 1.0f) // Bottom Left
    };

    // Define 3 Colors ( Red, Green, Blue)
    float4 colors[3] =
    {
        float4(1.0f, 0.0f, 0.0f, 1.0f), // Red
        float4(0.0f, 1.0f, 0.0f, 1.0f), // Green
        float4(0.0f, 0.0f, 1.0f, 1.0f) // Blue
    };

    // Based on ID 0,1 or 2 - pick the Pos and Color
    vsOutput.Position = positions[vertex_id];
    vsOutput.Color    = colors[vertex_id];

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