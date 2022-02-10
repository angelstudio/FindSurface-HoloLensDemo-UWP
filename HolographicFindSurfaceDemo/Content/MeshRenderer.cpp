#include "pch.h"
#include "MeshRenderer.h"
#include "Common/DirectXHelper.h"

#include "PrimitiveFactory.h"
#include <cstdint>

using namespace HolographicFindSurfaceDemo;
using namespace DirectX;

// Loads vertex and pixel shaders from files and instantiates the mesh geometry.
MeshRenderer::MeshRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
}

void MeshRenderer::SetCurrentModel(const InstanceConstantBuffer& buffer)
{
	if (!m_loadingComplete) 
	{
		return;
	}

	m_modelTypeIndices[0] = buffer.modelIndex;

	// Copy to constant buffer
	const auto context = m_deviceResources->GetD3DDeviceContext();

	// Update the constant buffer
	context->UpdateSubresource(
		m_modelConstantBuffers[0].Get(),
		0,
		nullptr,
		&buffer,
		0,
		0
	);
}

void MeshRenderer::StoreCurrent()
{
	if (m_modelTypeIndices[0] < 0 || m_modelTypeIndices[0] > 4)
	{
		return;
	}

	if (m_modelCount < MAX_INSTANCE_COUNT) 
	{
		// Stored index starts from 1
		uint32_t index = ++m_modelCount;

		m_modelTypeIndices[index] = m_modelTypeIndices[0];
		m_deviceResources->GetD3DDeviceContext()->CopyResource(m_modelConstantBuffers[index].Get(), m_modelConstantBuffers[0].Get());
	}
}

void MeshRenderer::RemoveLast()
{
	if (m_modelCount > 0) 
	{
		--m_modelCount;
	}
}

void MeshRenderer::RemoveAll()
{
	m_modelCount = 0;
}


// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void MeshRenderer::Render()
{
	if (!m_loadingComplete)
	{
		return;
	}

	if (m_modelTypeIndices[0] < 0 && m_modelCount < 1) 
	{
		return; // No Live Mesh & No Stored Mesh
	}

	const auto context = m_deviceResources->GetD3DDeviceContext();

	const UINT stride = sizeof(float) * 3; // vertex poistion only
	const UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		m_vertexBuffer.GetAddressOf(),
		&stride,
		&offset
	);
	context->IASetIndexBuffer(
		m_indexBuffer.Get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
	);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Attach the vertex shader.
	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
	);

	if (!m_usingVprtShaders)
	{
		// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
		// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
		// a pass-through geometry shader is used to set the render target 
		// array index.
		context->GSSetShader(
			m_geometryShader.Get(),
			nullptr,
			0
		);
	}

	// Attach the pixel shader.
	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
	);

	// Set Render State
	context->RSSetState(m_rasterizeState.Get());

	
	for(uint32_t i = 0; i <= m_modelCount; i++)
	{
		if (m_modelTypeIndices[i] < 0) { continue; }
		uint32_t modelIndex = static_cast<uint32_t>(m_modelTypeIndices[i]);

		// Apply the model constant buffer to the vertex shader.
		context->VSSetConstantBuffers(
			0,
			1,
			m_modelConstantBuffers[i].GetAddressOf()
		);

		context->PSSetConstantBuffers(
			0,
			1,
			m_modelConstantBuffers[i].GetAddressOf()
		);
		
		context->DrawIndexedInstanced(
			m_indexCountList[modelIndex], // Index count per instance.
			2,                            // Instance count.
			m_baseIndexList[modelIndex],  // Start index location.
			m_baseVertexList[modelIndex], // Base vertex location.
			0                             // Start instance location.
		);
	}

	context->RSSetState(nullptr);
}

std::future<void> MeshRenderer::CreateDeviceDependentResources()
{
	m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

	// On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
	// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
	// we can avoid using a pass-through geometry shader to set the render
	// target array index, thus avoiding any overhead that would be 
	// incurred by setting the geometry shader stage.
	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///MeshVprtVertexShader.cso" : L"ms-appx:///MeshVertexShader.cso";

	// Shaders will be loaded asynchronously.

	// After the vertex shader file is loaded, create the shader and input layout.
	std::vector<byte> vertexShaderFileData = co_await DX::ReadDataAsync(vertexShaderFileName);
	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreateVertexShader(
			vertexShaderFileData.data(),
			vertexShaderFileData.size(),
			nullptr,
			&m_vertexShader
		)
	);

	constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexDesc =
	{ {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	} };

	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreateInputLayout(
			vertexDesc.data(),
			static_cast<UINT>(vertexDesc.size()),
			vertexShaderFileData.data(),
			static_cast<UINT>(vertexShaderFileData.size()),
			&m_inputLayout
		)
	);

	// After the pixel shader file is loaded, create the shader and constant buffer.
	std::vector<byte> pixelShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///MeshPixelShader.cso");
	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreatePixelShader(
			pixelShaderFileData.data(),
			pixelShaderFileData.size(),
			nullptr,
			&m_pixelShader
		)
	);

	if (!m_usingVprtShaders)
	{
		// Load the pass-through geometry shader.
		std::vector<byte> geometryShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///MeshGeometryShader.cso");

		// After the pass-through geometry shader file is loaded, create the shader.
		winrt::check_hresult(
			m_deviceResources->GetD3DDevice()->CreateGeometryShader(
				geometryShaderFileData.data(),
				geometryShaderFileData.size(),
				nullptr,
				&m_geometryShader
			));
	}

	// Constant Buffer
	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(InstanceConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	for (uint32_t i = 0; i < m_modelConstantBuffers.size(); i++) {
		winrt::check_hresult(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&(m_modelConstantBuffers[i])
			)
		);
	}

	// Rasterize State
	D3D11_RASTERIZER_DESC rdesc;
	rdesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;
	rdesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_BACK;
	rdesc.FrontCounterClockwise = true;
	rdesc.DepthBias = 0;
	rdesc.DepthBiasClamp = 0.0f;
	rdesc.SlopeScaledDepthBias = 0.0f;
	rdesc.DepthClipEnable = true;
	rdesc.ScissorEnable = false;
	rdesc.MultisampleEnable = false;
	rdesc.AntialiasedLineEnable = false;

	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreateRasterizerState(
			&rdesc,
			&m_rasterizeState
		)
	);

	// Mesh Geometry Data

	std::vector<float> unitPlaneVertices;
	std::vector<float> unitSphereVertices;
	std::vector<float> unitCylinderVertices;
	std::vector<float> unitConeElementVertices;
	std::vector<float> unitTorusElementVertices;

	std::vector<uint16_t> unitPlaneIndices;
	std::vector<uint16_t> unitSphereIndices;
	std::vector<uint16_t> unitCylinderIndices;
	std::vector<uint16_t> unitConeElementIndices;
	std::vector<uint16_t> unitTorusElementIndices;

	PrimitiveFactory::CreateUnitPlaneXZ(unitPlaneVertices, unitPlaneIndices, 5, 5);
	PrimitiveFactory::CreateUnitSphere(unitSphereVertices, unitSphereIndices, 20, 20);
	PrimitiveFactory::CreateUnitCylinder(unitCylinderVertices, unitCylinderIndices, 20, 3);
	PrimitiveFactory::CreateUnitCylinder(unitConeElementVertices, unitConeElementIndices, 20, 5);
	PrimitiveFactory::CreateUnitCylinder(unitTorusElementVertices, unitTorusElementIndices, 20, 20);

	std::vector<uint32_t> baseVertex(5);

	std::vector<uint32_t> baseIndex(5);
	std::vector<uint32_t> indexCount(5);

	uint32_t tmp1 = 0;
	uint32_t tmp2 = 0;
	uint32_t tmp3 = 0;

	const std::vector<float>* vList[5] = {
		&unitPlaneVertices,
		&unitSphereVertices,
		&unitCylinderVertices,
		&unitConeElementVertices,
		&unitTorusElementVertices
	};

	const std::vector<uint16_t>* iList[5] = {
		&unitPlaneIndices,
		&unitSphereIndices,
		&unitCylinderIndices,
		&unitConeElementIndices,
		&unitTorusElementIndices
	};

	for (uint32_t i = 0; i < 5; i++)
	{
		baseVertex[i] = tmp1;
		tmp1 += static_cast<uint32_t>(vList[i]->size() / 3);
		tmp3 += static_cast<uint32_t>(vList[i]->size());

		baseIndex[i] = tmp2;
		indexCount[i] = static_cast<uint32_t>(iList[i]->size());
		tmp2 += static_cast<uint32_t>(iList[i]->size());
	}

	// Merge Vertices && Indices Buffer
	std::vector<float> mergeVertices;
	std::vector<uint16_t> mergeIndices;

	mergeVertices.reserve(static_cast<size_t>(tmp3));
	mergeIndices.reserve(static_cast<size_t>(tmp2));

	for (uint32_t i = 0; i < 5; i++) {
		mergeVertices.insert( mergeVertices.end(), vList[i]->begin(), vList[i]->end() );
		mergeIndices.insert( mergeIndices.end(), iList[i]->begin(), iList[i]->end() );
	}

	// Create Buffer!!
	D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
	vertexBufferData.pSysMem = mergeVertices.data();
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;
	const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(float) * static_cast<UINT>(mergeVertices.size()), D3D11_BIND_VERTEX_BUFFER);
	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&vertexBufferDesc,
			&vertexBufferData,
			&m_vertexBuffer
		)
	);

	D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
	indexBufferData.pSysMem = mergeIndices.data();
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(uint16_t) * static_cast<UINT>(mergeIndices.size()), D3D11_BIND_INDEX_BUFFER);
	winrt::check_hresult(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&indexBufferDesc,
			&indexBufferData,
			&m_indexBuffer
		)
	);

	m_baseVertexList.swap(baseVertex);
	m_baseIndexList.swap(baseIndex);
	m_indexCountList.swap(indexCount);

	m_modelTypeIndices[0] = -1;
	m_modelCount = 0;

	// the object is ready to be rendered.
	m_loadingComplete = true;
}

void MeshRenderer::ReleaseDeviceDependentResources()
{
	m_modelTypeIndices[0] = -1;
	m_modelCount = 0;

	m_loadingComplete = false;
	m_usingVprtShaders = false;
	m_rasterizeState.Reset();
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_geometryShader.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();

	for (uint32_t i = 0; i < m_modelConstantBuffers.size(); i++) {
		m_modelConstantBuffers[i].Reset();
	}
}