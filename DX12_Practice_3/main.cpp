// Include clean windows header FIRST to set the rules
#include "CleanWindows.h"

#include "utility.hpp"

// SDL-Headers
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

// DirectX-Headers
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// Windows Run-Time C++ template Library Header
#include <wrl/client.h>

// Standard C++ Headers

using Microsoft::WRL::ComPtr;

#pragma region Synchronization Helpers

static uint64_t Signal(const ComPtr<ID3D12CommandQueue>& commandQueue, const ComPtr<ID3D12Fence>& fence, uint64_t& fenceValue)
{
	const uint64_t fenceValueForSignal = ++fenceValue;
	ASSERT_SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValueForSignal),"Failed to signal the Fence\n")
	return fenceValueForSignal;
}

static void WaitForFenceValue(const ComPtr<ID3D12Fence>& fence, const uint64_t fenceValue, const HANDLE fenceEvent)
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		ASSERT_SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent),"Wait for fence val: set event failed\n")
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

static void Flush(const ComPtr<ID3D12CommandQueue>& commandQueue, const ComPtr<ID3D12Fence>& fence, uint64_t& fenceValue, const HANDLE fenceEvent)
{
	const uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

#pragma endregion

void Update(const double deltaTime,float outColor[4])
{
	static float totalTime = 0.f;
	totalTime += static_cast<float>(deltaTime);

	// Calculate Pulse (0.5 to 1.0)
	const float pulse = (static_cast<float>(sin(totalTime * 3.0)) * 0.25f) + 0.75f;

	// Apply Pulse to "Hot Pink" (R=1.0, G=0.41, B=0.71)
	outColor[0] = 1.0f * pulse;   // Red
	outColor[1] = 0.41f * pulse;  // Green
	outColor[2] = 0.71f * pulse;  // Blue
	outColor[3] = 1.0f;           // Alpha
}

// ---------------------------------------------------------------------------
// HELPER: Resize
// 1. Flush GPU (Wait for all work to finish)
// 2. Release old back buffers
// 3. Resize Swap Chain
// 4. Recreate RTVs
// ---------------------------------------------------------------------------
static void Resize(
	int width, int height,
	// Objects we need to modify:
	const ComPtr<ID3D12Device2>& device,
	const ComPtr<IDXGISwapChain4>& swapChain,
	const ComPtr<ID3D12DescriptorHeap>& rtvHeap,
	ComPtr<ID3D12Resource> backBuffers[], // Array passed by pointer
	// Synchronization objects for Flushing:
	const ComPtr<ID3D12CommandQueue>& commandQueue,
	const ComPtr<ID3D12Fence>& fence,
	HANDLE fenceEvent,
	uint64_t& currentFenceValue,
	uint64_t frameFenceValues[], // Array
	UINT& currentFrameIndex,
	UINT rtvDescriptorSize
)
{
	// 1. Check for Valid Size (Don't resize if minimized)
	if (width == 0 || height == 0) return;

	// 2. FLUSH THE GPU
	// We CANNOT destroy the back buffers if the GPU is currently drawing to them!
	Flush(commandQueue, fence, currentFenceValue, fenceEvent);

	// 3. RELEASE CURRENT BACK BUFFERS
	// DXGI will fail to resize if we are still holding pointers to the textures.
	for (int i = 0; i < 2; ++i)
	{
		// Calling .Reset() releases the COM pointer (sets it to nullptr)
		backBuffers[i].Reset();

		// Reset the fence values for these frames since we are starting fresh
		frameFenceValues[i] = frameFenceValues[currentFrameIndex];
	}

	// 4. RESIZE THE SWAP CHAIN
	// Use the same format and flags as creation. 
	// BufferCount = 0 means "Preserve existing number of buffers"
	// Width/Height = 0 means "Figure it out from the window size" (But passing explicit is safer)
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	swapChain->GetDesc1(&desc); // Get current desc to preserve flags (like AllowTearing)

	ASSERT_SUCCEEDED(
		swapChain->ResizeBuffers(
			0, // Keep buffer count same
			width, height,
			DXGI_FORMAT_UNKNOWN, // Keep format same
			desc.Flags // Keep flags (Tearing, etc.)
		),
		"Failed to resize swap chain buffers"
	)

	// 5. RE-CREATE RTV DESCRIPTORS
	// This is exactly the same loop as in our initialization
	currentFrameIndex = swapChain->GetCurrentBackBufferIndex();

	// Point handle to start of heap again
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < 2; ++i)
	{
		// 1. Get the NEW resized buffer
		ASSERT_SUCCEEDED(
			swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])),
			"Failed to retrieve resized back buffer"
		);

		// 2. Create the RTV in the heap
		device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle);

		// 3. Move pointer
		rtvHandle.ptr += rtvDescriptorSize;
	}

	SDL_Log("Resized Window to %dx%d", width, height);
}

// Vertex Data
//-------------

struct Vertex
{
	float position[3];	// x, y, z
	float color[4];		// r, g, b, a
};

int main(int argc, char* argv[])
{
	int  width{640};
	int  height{480};
	bool vsync = true;
	constexpr UINT bufferCount{2};
	// Store the actual resource pointers for the back buffers
	ComPtr<ID3D12Resource> backBuffers[2];

	//--------------------------------
	// Initialize SDL Video Sub-System
	//--------------------------------
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Video init failed: %s \n", SDL_GetError());
		return 1;
	}

	//-----------------------------------------
	// Initialize SDL Window & keep the pointer
	//-----------------------------------------
	SDL_Window* window = SDL_CreateWindow("Hello DX 12", width, height, SDL_WINDOW_RESIZABLE);
	if (!window)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s \n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	//------------------------------------
	// Get the Native HWND (Window Handle)
	//------------------------------------
	HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
	if (!hwnd)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to retrieve Win32 HWND. Is this running on Windows? \n");
		// Clean up the previous resource (Window)
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_Log("The Window's HWND: %p", hwnd);

	//--------------------------------------------------------------------------------
	// 1. DIRECT-X 12: Initialization Begins:
	//--------------------------------------------------------------------------------

#pragma region Block A: The "Hardware" (Infrastructure)

	// 1.1 DX12 Debug Layers
	//-----------------------
	// Define this OUTSIDE the #ifdef so it exists for the Factory later.
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;

	HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));

	if (SUCCEEDED(hr))
	{
		debugController->EnableDebugLayer();
		SDL_Log("Debug Layers enabled \n");

		// Optional: Enable GPU-Based Validation (Heavy but finds more bugs)
		// ComPtr<ID3D12Debug1> debugController1;
		// if (SUCCEEDED(debugController.As(&debugController1))) {
		//     debugController1->SetEnableGPUBasedValidation(true);
		// }

		// Tell DXGI (The Factory) that we are in debug mode too.
		// This enables the debug overlay and validates Swap Chain interactions.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Failed to enable DX12 Debug Layer. \n");
	}
#endif

	// 1.2 Create DXGI Factory
	//------------------------
	ComPtr<IDXGIFactory6> dxgiFactory6;
	ASSERT_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory6)),
		"Critical Error: Failed to initialize DXGI Factory.")

	// 1.3 Find the Best GPU (Adapter)
	//---------------------------------
	ComPtr<IDXGIAdapter4> finalAdapter;
	ComPtr<IDXGIAdapter1> tempAdapter;
	SIZE_T maxVideoMemory{};

	// Loop through all available GPUs (0,1,2...)
	for (UINT i = 0; dxgiFactory6->EnumAdapters1(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		tempAdapter->GetDesc1(&desc);

		// A. Skip the Microsoft Basic Render Driver (WARP)
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		// B. Check if GPU actually supports DX12
		if (FAILED(D3D12CreateDevice(tempAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			continue;
		}

		// C. Is this the strongest GPU? Compare VRAM
		if (desc.DedicatedVideoMemory > maxVideoMemory)
		{
			maxVideoMemory = desc.DedicatedVideoMemory;
			ASSERT_SUCCEEDED(
				tempAdapter.As(&finalAdapter),
				"Failed to cast Adapter1 to Adapter4"
			)

				SDL_Log("Found GPU: %ls (VRAM: %zu MB)", desc.Description, desc.DedicatedVideoMemory / 1024 / 1024);
		}
	}
	if (!finalAdapter)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "No compatible Hardware GPU found! Falling back to WARP (Software).");
		ComPtr<IDXGIAdapter1> warpAdapter;
		ASSERT_SUCCEEDED(
			dxgiFactory6->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)),
			"Failed to load WARP."
		)
			ASSERT_SUCCEEDED(warpAdapter.As(&finalAdapter), "Failed to cast WARP to Adapter4")
	}

	// 1.5 Create the Device
	//-----------------------
	ComPtr<ID3D12Device2> device2;
	ASSERT_SUCCEEDED(D3D12CreateDevice(finalAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device2)),
		"Failed to create DX12 device")

	// 1.6 Create the InfoQueue ( More debugging - post device creation )
	//--------------------------------------------------------------------
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue;
	ASSERT_SUCCEEDED(
		device2.As(&infoQueue),
		"Failed to get InfoQueue. Did the Debug Layer fail to enable in Step 1?"
	)

	// Generate break points on memory corruption, errors and warnings
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

	// Suppress false positives 
	D3D12_MESSAGE_SEVERITY severities[] = {
		D3D12_MESSAGE_SEVERITY_INFO		// mute simple info messages
	};

	D3D12_MESSAGE_ID denyIds[] = {
		D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // Common annoyance
		// Add more IDs here if the console gets spammy
		D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,	// warnings while using capture frame for graphics debugging
		D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
	};

	D3D12_INFO_QUEUE_FILTER filter = {};
	filter.DenyList.NumSeverities = _countof(severities);
	filter.DenyList.pSeverityList = severities;
	filter.DenyList.NumIDs = _countof(denyIds);
	filter.DenyList.pIDList = denyIds;

	ASSERT_SUCCEEDED(infoQueue->PushStorageFilter(&filter), "failed to push info queue storage filters \n")

#endif

	// 1.7 Create the Command Queue & create description
	//--------------------------------------------------
	ComPtr<ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct = can draw, copy, compute 
	queueDesc.NodeMask = 0;
	ASSERT_SUCCEEDED(device2->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&commandQueue)),
		"Failed to create command queue \n")

	// 1.8 Check for Tearing Support
	//-------------------------------
	BOOL allowTearing = FALSE;
	if (FAILED(dxgiFactory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
	{
		allowTearing = FALSE;
	}
	SDL_Log("Tearing Support: %s", allowTearing ? "ENABLED" : "DISABLED");

	// 1.9 Create the SwapChain
	//-------------------------
	ComPtr<IDXGISwapChain4> swapChain4;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width				= width;
	swapChainDesc.Height			= height;
	swapChainDesc.Format			= DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo			= FALSE;
	swapChainDesc.SampleDesc	 = { 1, 0 };
	swapChainDesc.BufferCount		= bufferCount;
	swapChainDesc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.Scaling			= DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect		= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode			= DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags				= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	// Need a mandatory v1 pointer
	ComPtr<IDXGISwapChain1> swapChain1;
	ASSERT_SUCCEEDED(dxgiFactory6->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1),"Failed to create swap chain\n")
	ASSERT_SUCCEEDED(swapChain1.As(&swapChain4),"Failed to cast swapChain1 to 4\n")

#pragma endregion

#pragma region Block B: The "Memory"

// TODO: Can Put Depth Buffer/ DSV Heap Here Later

	// 1.10.1 Create the RTV Descriptor Heap
	//-------------------------------------
	ComPtr<ID3D12DescriptorHeap> rtvHeap;

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.NumDescriptors = bufferCount; // 2 slots (for each back buffer)
	rtvHeapDesc.Type		   = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags		   = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // RTV Heap not visible to shaders
	rtvHeapDesc.NodeMask	   = 0; // Whats this?

	ASSERT_SUCCEEDED(device2->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)),"Failed to create RTV Descriptor Heap")

	// Need to know size of a single slot of the heap (different based on nvidia, amd etc.)
	UINT rtvDescriptorSize = device2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	SDL_Log("RTV Descriptor Heap Created. Slot Size: %u bytes", rtvDescriptorSize);

	// 1.10.2 Fill the RTV with the SwapChain images (try without d3dx.h helper)
	//--------------------------------------------------------------------------
	// Get the handle (pointer) to the very first slot in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < bufferCount; ++i)
	{
		// Get specific texture from swapchain back buffer (index 0 or 1)
		ASSERT_SUCCEEDED(swapChain4->GetBuffer(i,IID_PPV_ARGS(&backBuffers[i])),"Failed to get back buffer data from swapchain")

		// Create the Render Target View
		device2->CreateRenderTargetView(backBuffers[i].Get(),nullptr,rtvHandle);

		// Manually advance the RTVHandle to next slot by adding slot size (without d3dx helper)
		rtvHandle.ptr += rtvDescriptorSize;
	}
	SDL_Log("RTVs Initialized for %u Back Buffers.", bufferCount);

	// Quad Vertices Data:
	//=====================

	Vertex quadVertices[] = {
		// Triangle 1
		{ {-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }, // Top Left (Red)
		{ { 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} }, // Bottom Right (Green)
		{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} }, // Bottom Left (Blue)

		// Triangle 2
		{ {-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }, // Top Left (Red)
		{ { 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f} }, // Top Right (Yellow)
		{ { 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} }  // Bottom Right (Green)
	};

	const UINT vertexBufferSize = sizeof(quadVertices);

	//------------------------------------------------------
	// Create the Upload Heap Buffer (CPU->GPU Shared space)
	//------------------------------------------------------
	ComPtr<ID3D12Resource> vertexBufferUpload;

	// Describe the "Heap" (Upload = CPU can Write, GPU can Read)
	D3D12_HEAP_PROPERTIES uploadHeapProps = {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask  = 1;

	// Describe the resource (A simple buffer of "X bytes")
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = vertexBufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ASSERT_SUCCEEDED(
		device2->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, // State must be Generic Read for Upload Heaps
			nullptr,
			IID_PPV_ARGS(&vertexBufferUpload)
		),
		"Failed to create Upload Buffer"
	)

	// Copy Data from C++ Array -> Upload Buffer
	//-------------------------------------------
	// "Map" the buffer to get a CPU pointer
	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read on CPU

	ASSERT_SUCCEEDED(
		vertexBufferUpload->Map(0, &readRange, &pMappedData),
		"Failed to Map"
	)

	// NOTE: The d3dx12.h helper library has a function called 'UpdateSubresources'
	// which handles this Map/Memcpy/Unmap/Barrier process automatically.

	// Perform the copy
	memcpy(static_cast<UINT8*>(pMappedData), quadVertices, vertexBufferSize);

	// Unmap tells the driver we are done writing
	vertexBufferUpload->Unmap(0, nullptr);

	//--------------------------------------------
	// Create the "Default Buffer" (GPU VRAM Only)
	//--------------------------------------------
	ComPtr<ID3D12Resource> vertexBufferDefault;

	D3D12_HEAP_PROPERTIES defaultHeapProps = {};
	defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	ASSERT_SUCCEEDED(
		device2->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COMMON, // will implicitly go to copy_dest state
			nullptr,
			IID_PPV_ARGS(&vertexBufferDefault)
		),
		"Failed to create Default Buffer"
	)

	// Define the "View" (VBV)
	// This tells the pipeline HOW to look at this buffer (Stride, Size, Address)
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = vertexBufferDefault->GetGPUVirtualAddress(); // Look at the GPU buffer, not upload!
	vertexBufferView.SizeInBytes = vertexBufferSize;
	vertexBufferView.StrideInBytes = sizeof(Vertex);

#pragma endregion

#pragma region Block C: The "Pipeline" (What we are drawing)

	// C.1 Create an Empty Root Signature
	// -----------------------------------
	// The Root Signature defines what data the shader expects (textures, buffers).
	// For this challenge, we hardcode vertices in the shader, so we need NOTHING.

	ComPtr<ID3D12RootSignature> rootSignature;

	// Define the description 
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Serialize
	ComPtr<ID3DBlob> signatureBlob, errorBlob;
	HRESULT serializedRootSig = D3D12SerializeRootSignature(&rootSigDesc,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if (FAILED(serializedRootSig)) {
		// If it fails, the errorBlob contains the text message explaining why
		if (errorBlob) {
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Root Sig Error: %s", static_cast<char*>(errorBlob->GetBufferPointer()));
		}
		return 1;
	}

	// Create it
	ASSERT_SUCCEEDED(device2->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)),"Root Signature creation failed \n")
	SDL_Log("Root Signature Created.");

	// C.2. Compile Shaders
	//========================

	ComPtr<ID3DBlob> vertexShaderBlob;
	ComPtr<ID3DBlob> pixelShaderBlob;
	ComPtr<ID3DBlob> errorShaderBlob;
	UINT compileFlags = 0; 
#ifdef _DEBUG	// Disable optimizations in debug mode
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT shaderCompile = D3DCompileFromFile(
		L"shaders.hlsl", // WString filename
		nullptr, nullptr,
		"VS_Main",        // Entry Point function name in HLSL
		"vs_5_1",        // Target: Vertex Shader model 5.1
		compileFlags, 0,
		&vertexShaderBlob,
		&errorBlob
	);
	if (FAILED(shaderCompile)) {
		if (errorBlob) {
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "VS Compile Error: %s", (char*)errorBlob->GetBufferPointer());
		}
		return 1;
	}
	// Compile Pixel Shader
	shaderCompile = D3DCompileFromFile(
		L"shaders.hlsl",
		nullptr, nullptr,
		"PS_Main",        // Entry Point
		"ps_5_1",        // Target: Pixel Shader model 5.1
		compileFlags, 0,
		&pixelShaderBlob,
		&errorBlob
	);

	if (FAILED(shaderCompile)) {
		if (errorBlob) {
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "PS Compile Error: %s", (char*)errorBlob->GetBufferPointer());
		}
		return 1;
	}
	SDL_Log("Shaders Compiled Successfully.");

	// C.3 The Pipeline State Object (PSO)
	//====================================
	ComPtr<ID3D12PipelineState> pipelineState;

	// Define the input layout 
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.InputLayout = {inputElementDescs,_countof(inputElementDescs)}; 
	psoDesc.pRootSignature = rootSignature.Get();

	// Shaders (Manually filling the byte code struct)
	psoDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
	psoDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
	psoDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
	psoDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();

	// Rasterizer State (The "How to draw" rules)
	// Without CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT), we fill it manually:
	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE; // SAFEST for beginners! (Draws both sides)
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.MultisampleEnable = FALSE;
	rasterDesc.AntialiasedLineEnable = FALSE;
	rasterDesc.ForcedSampleCount = 0;
	rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	psoDesc.RasterizerState = rasterDesc;

	// Blend State (How colors mix)
	// Without CD3DX12_BLEND_DESC(D3D12_DEFAULT), we fill it manually:
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	// We only have 1 Render Target (Index 0)
	blendDesc.RenderTarget[0].BlendEnable = FALSE; // No transparency for now
	blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // Write R, G, B, and A

	psoDesc.BlendState = blendDesc;

	// Depth Stencil State
	// We aren't using a Z-Buffer yet, so disable it.
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;

	// Topology & Formats
	psoDesc.SampleMask = UINT_MAX; // 0xFFFFFFFF
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Must match SwapChain!
	psoDesc.SampleDesc.Count = 1;

	// Create it!
	ASSERT_SUCCEEDED(device2->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)),
		"Failed to create PSO")

#pragma endregion

#pragma region Block D: The "Recording Tools" (Execution)

	// 1.11 Create a Command Allocators (One per back buffer) 
	//------------------------------------------------------
	// We need 2 allocators:
	// Allocator[0] -> Used for Frame 0, 2, 4...
	// Allocator[1] -> Used for Frame 1, 3, 5...
	ComPtr<ID3D12CommandAllocator> commandAllocators[bufferCount];
	for (auto& commandAllocator : commandAllocators)
	{
		ASSERT_SUCCEEDED(
			device2->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&commandAllocator)
			),
			"Failed to create Command Allocator"
		)
	}
	SDL_Log("Created %d Command Allocators.", bufferCount);

	// 1.12 Create a Command List
	//---------------------------
	// The Command List needs an allocator to start with. We give it the first one.
	// It also wants a "Pipeline State" (PSO), but we pass nullptr for now (we set it later).

	ComPtr<ID3D12GraphicsCommandList> commandList;
	ASSERT_SUCCEEDED(
		device2->CreateCommandList(
			0, // Node Mask (0 for single GPU)
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocators[0].Get(), // Initial Allocator
			nullptr, // Initial Pipeline State (PSO)
			IID_PPV_ARGS(&commandList)
		),
		"Failed to create Command List"
	)
	// CLOSE it immediately after creation to keep state consistent.
	ASSERT_SUCCEEDED(commandList->Close(),"Failed to close Command List")
	SDL_Log("Command List Created and Closed.");

	// 1.13.1 Create the Fence
	//-----------------------
	ComPtr<ID3D12Fence1> fence1;
	ASSERT_SUCCEEDED(
		device2->CreateFence(0, // Fence Starts at 0
			D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence1)),
		"Failed to create Fence"
	)

	// The Fence Value that will increment every frame
	uint64_t currentfenceValue = 0;

	// 1.13.2 Create the Event Handle
	//----------------------------------
	// This is a Windows Kernel Object. The CPU sleeps on this line until the GPU wakes it up.
	// Parameters: (Security, ManualReset, InitialState, Name)
	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
	{
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create Fence Event.");
		return -1;
	}

	// =========================================================
	// INITIALIZATION: UPLOAD VERTEX DATA TO GPU
	// =========================================================

	// 1. Reset the allocator and list so we can record commands
	commandAllocators[0]->Reset();
	commandList->Reset(commandAllocators[0].Get(), nullptr);

	// 2. Record the Copy Command
	// Copy from Upload (Source) to Default (Dest)
	commandList->CopyBufferRegion(
		vertexBufferDefault.Get(), 0,
		vertexBufferUpload.Get(), 0,
		vertexBufferSize
	);

	// 3. Transition the Default Buffer state
	// It was COPY_DEST. Now we need it to be a VERTEX_BUFFER so the shaders can read it.
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = vertexBufferDefault.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(1, &barrier);

	// 4. Close and Execute
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, ppCommandLists);

	// 5. WAIT FOR IT TO FINISH (Crucial!)
	// We cannot start the game loop until the vertex data is safely on the GPU.
	Flush(commandQueue, fence1, currentfenceValue, fenceEvent);

	SDL_Log("Vertex Buffer Uploaded to GPU Default Heap.");

#pragma endregion

	//======================
	// THE EVENT LOOP
	//======================
	bool isRunning = true;
	SDL_Event event;
	UINT frameIndex = swapChain4->GetCurrentBackBufferIndex();
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Track the fence value for each back buffer
	uint64_t frameFenceValues[bufferCount] = {}; 

	// Timing Setup
	//------------------
	// Get the speed of the CPU counter (Ticks per Second)
	const uint64_t performanceFrequency = SDL_GetPerformanceFrequency();

	// Get the starting time
	uint64_t lastCounter = SDL_GetPerformanceCounter();

	while (isRunning)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				isRunning = false;
			}
			else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				// 1. Update the variables that the Viewport uses!
				width = event.window.data1;
				height = event.window.data2;

				Resize(
					width, height,
					device2, swapChain4, rtvHeap, backBuffers,
					commandQueue, fence1, fenceEvent, currentfenceValue,
					frameFenceValues, frameIndex, rtvDescriptorSize
				);
			}
		}
		// Update (Time Calculation )
		//============================
		// Get the Time Now
		uint64_t currentCounter = SDL_GetPerformanceCounter();

		// Calculate difference (ticks elapsed)
		uint64_t counterElapsed = currentCounter - lastCounter;

		// Convert to seconds (dT = (now - last)/ frequency
		double deltaTime = static_cast<double>(counterElapsed) / static_cast<double>(performanceFrequency);

		// Update last for next frame
		lastCounter = currentCounter;

		// UPDATE Game Logic
		Update(deltaTime,clearColor);
		
		//=====================================================
		// 2. DX12 Render Loop ( Wait->Record->Present->Signal)
		//=====================================================
		 /*
			Wait	(CPU sleeps if GPU is slow).
			Reset	(Safe to wipe memory now).
			Record	(Barriers, Clear).
			Execute	(Hand work to GPU).
			Present	(Swap buffers).
			Signal	(Mark this frame as "Pending Completion").
		 */

		// 2.0 WAIT (Synchronization)
		//---------------------------
		// Before we reset the allocator for this frame, we must ensure the GPU 
		// is finished with the commands we recorded into it last time.
		WaitForFenceValue(fence1.Get(), frameFenceValues[frameIndex], fenceEvent);

		// 2.1 Get the pointers for the current frame
		//-------------------------------------------
		auto& commandAllocator = commandAllocators[frameIndex];
		auto& backBuffer				 = backBuffers[frameIndex];

		// 2.2 Reset the Command Allocator and Command List (Prepare for next Frame)
		//--------------------------------
		// This reclaims the memory used by commands from the previous time we used this frame.
		// We know it is safe because we WaitedForFenceValue() just before this.
		commandAllocator->Reset();

		// Puts the "pen" back on top of the page (analogy?)
		// We pass nullptr for PSO (Pipeline State) because we aren't drawing triangles yet, just clearing.
		ASSERT_SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr), "Failed to reset command list")

		//******//
		//RENDER//
		//******//
		// 2.3 Transition RESOURCE BARRIER (From Present to Render Target)
		// We need to transition the back buffer so we can clear it. (Doing it without CD3DX helper)
		D3D12_RESOURCE_BARRIER resourceBarrier = {};
		resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		resourceBarrier.Transition.pResource = backBuffer.Get();
		resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;      // Where it is now
		resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // Where we want it
		resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Record this command into the command list
		commandList->ResourceBarrier(1,&resourceBarrier);

		// 2.4 Clear Screen Logic
		//=======================
		// 2.4.1 Calculate the Handle (Pointer) to the current RTV
		// Logic: Start of Heap + (Frame Index * Size of one Slot)
		D3D12_CPU_DESCRIPTOR_HANDLE currentRtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		currentRtvHandle.ptr += (static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize);

		// 2.4.2 clear the RTV
		commandList->ClearRenderTargetView(currentRtvHandle,clearColor,0,nullptr);

		// ==========================================================
		// CHALLENGE 2: DRAW THE TRIANGLE
		// ==========================================================
		// 1. Bind the Root Signature & PSO
		// --------------------------------
		// Tell the GPU: "Use this contract (Root Sig) and these compiled shaders (PSO)"
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(pipelineState.Get());

		// 2. Setup Input Assembly
		// -----------------------
		// Tell the GPU how to interpret the vertices. 
		// TRIANGLELIST = Every 3 vertices make a distinct triangle.
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Bind the buffers
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

		// 3. Setup Viewport & Scissor Rect
		// --------------------------------
		// VIEWPORT: Maps the clip space (-1 to +1) to pixel coordinates (0 to 640).
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// SCISSOR RECT: Pixels outside this box are killed. (We want the whole screen).
		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = width;
		scissorRect.bottom = height;

		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);

		// 4. Output Merger (Bind Render Target)
		// -------------------------------------
		// CRITICAL: ClearRenderTargetView just clears memory. It does NOT set the 
		// render target for drawing. We must strictly tell the GPU: "Draw to this handle."
		commandList->OMSetRenderTargets(1, &currentRtvHandle, FALSE, nullptr);

		// 5. THE DRAW CALL
		// ----------------
		// VertexCount: 6 (2 Triangles to make the Quad)
		// InstanceCount: 1 (Just one triangle)
		// StartVertex: 0
		// StartInstance: 0
		commandList->DrawInstanced(6, 1, 0, 0);

		// ==========================================================

		//*******//
		//PRESENT//
		//*******//
		// 2.5 Transition RESOURCE BARRIER (Back to Present)
		resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList->ResourceBarrier(1, &resourceBarrier);

		// 2.6 Close List
		ASSERT_SUCCEEDED(commandList->Close(), "Failed to close command list")

		// 2.7 EXECUTE The Command List
		//------------------------------
		ID3D12CommandList* const commandLists[] = {commandList.Get()};
		commandQueue->ExecuteCommandLists(_countof(commandLists),commandLists);

		// 2.8 Present to swapchain (after checking for vsync)
		//----------------------------------------------------
		UINT syncInterval = vsync ? 1 : 0;
		UINT presentFlags = allowTearing && !vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ASSERT_SUCCEEDED(swapChain4->Present(syncInterval, presentFlags),"SwapChain Present failed \n")

		// 2.9 SIGNAL (Synchronization)
		//-----------------------------
		// Signal the GPU to write the NEXT value when it finishes this frame
		const uint64_t fenceValue = Signal(commandQueue.Get(), fence1.Get(), currentfenceValue);

		// Remember this value! We must wait for THIS value next time we use this specific buffer.
		frameFenceValues[frameIndex] = fenceValue;

		// Update the frame index for the next loop
		frameIndex = swapChain4->GetCurrentBackBufferIndex();
	}

	// CleanUp Dx 12
	// Wait for the GPU to finish the very last frame.
	Flush(commandQueue.Get(), fence1.Get(), currentfenceValue, fenceEvent);

	CloseHandle(fenceEvent);

	//---------------
	// CleanUp SDL
	//---------------
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
