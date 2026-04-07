#include "Bridge/MaterialBridge.h"

#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"

MaterialBridge::HandleReference MaterialBridge::createMaterialHandle(const ObjectDesc& objectDesc)
{
	return BridgeType::createObject(objectDesc);
}

bool MaterialBridge::isHandleAlive(const PackedHandle packedHandle) const
{
	return BridgeType::isHandleAlive(packedHandle);
}

const MaterialBridge::StaticData* MaterialBridge::getStaticData(const PackedHandle packedHandle) const
{
	return BridgeType::getStaticProperty(packedHandle);
}

bool MaterialBridge::resolveEffectiveShaders(
	const PackedHandle packedHandle,
	const ShaderTargetPlatform targetPlatform,
	shared_pointer<ShaderObject>& outVertexShader,
	shared_pointer<ShaderObject>& outPixelShader) const
{
	outVertexShader = nullptr;
	outPixelShader = nullptr;

	shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
	assert(shaderPackageModule != nullptr && "[MaterialBridge][Assert] reason=shader_package_module_missing");

	const StaticData* materialStaticData = getStaticData(packedHandle);
	const shared_pointer<MaterialAsset> materialAsset = materialStaticData != nullptr ? materialStaticData->materialAsset : nullptr;
	return MaterialAsset::resolveEffectiveShaders(*shaderPackageModule, materialAsset, targetPlatform, outVertexShader, outPixelShader);
}

void MaterialBridge::processFrame()
{
	BridgeType::processFrame();
}
