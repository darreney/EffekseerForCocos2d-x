#include "LLGI.RenderPassDX12.h"
#include "LLGI.CommandListDX12.h"
#include "LLGI.DescriptorHeapDX12.h"
#include "LLGI.GraphicsDX12.h"
#include "LLGI.RenderPassPipelineStateDX12.h"
#include "LLGI.TextureDX12.h"

namespace LLGI
{

RenderPassDX12::RenderPassDX12(ID3D12Device* device) : device_(device) { SafeAddRef(device_); }

RenderPassDX12 ::~RenderPassDX12()
{
	for (size_t i = 0; i < numRenderTarget_; i++)
	{
		if (renderTargets_[i].texture_ != nullptr)
			SafeRelease(renderTargets_[i].texture_);
	}
	renderTargets_.clear();

	SafeRelease(device_);
}

bool RenderPassDX12::Initialize(TextureDX12** textures, int numTextures, TextureDX12* depthTexture)
{
	if (textures[0]->Get() == nullptr)
		return false;

	if (!assignRenderTextures((Texture**)textures, numTextures))
	{
		return false;
	}

	if (!assignDepthTexture(depthTexture))
	{
		return false;
	}

	if (!getSize(screenSize_, (const Texture**)textures, numTextures, depthTexture))
	{
		return false;
	}

	renderTargets_.resize(numTextures);
	numRenderTarget_ = numTextures;

	for (size_t i = 0; i < numTextures; i++)
	{
		renderTargets_[i].texture_ = textures[i];
		renderTargets_[i].renderPass_ = textures[i]->Get();
		SafeAddRef(renderTargets_[i].texture_);
	}

	return true;
}

bool RenderPassDX12::ReinitializeRenderTargetViews(CommandListDX12* commandList,
												   DescriptorHeapDX12* rtDescriptorHeap,
												   DescriptorHeapDX12* dtDescriptorHeap)
{
	if (numRenderTarget_ == 0)
		return false;

	handleRTV_.resize(numRenderTarget_);

	for (int i = 0; i < numRenderTarget_; i++)
	{
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = renderTargets_[i].texture_->GetDXGIFormat();
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		auto cpuHandle = rtDescriptorHeap->GetCpuHandle();
		device_->CreateRenderTargetView(renderTargets_[i].renderPass_, &desc, cpuHandle);
		handleRTV_[i] = cpuHandle;
		rtDescriptorHeap->IncrementCpuHandle(1);
		rtDescriptorHeap->IncrementGpuHandle(1);

		// memory barrior to make a rendertarget
		if (renderTargets_[i].texture_->GetType() != TextureType::Screen)
		{
			renderTargets_[i].texture_->ResourceBarrior(commandList->GetCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}

	if (GetHasDepthTexture())
	{
		auto depthTexture = static_cast<TextureDX12*>(GetDepthTexture());
		
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		auto cpuHandle = dtDescriptorHeap->GetCpuHandle();
		device_->CreateDepthStencilView(depthTexture->Get(), &desc, cpuHandle);
		handleDSV_ = cpuHandle;
		dtDescriptorHeap->IncrementCpuHandle(1);
		dtDescriptorHeap->IncrementGpuHandle(1);

		depthTexture->ResourceBarrior(commandList->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	return true;
}

} // namespace LLGI