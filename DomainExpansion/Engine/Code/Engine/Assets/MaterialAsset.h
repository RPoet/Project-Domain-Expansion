#pragma once

#include "Asset.h"

enum class ShaderTargetPlatform : uint32;
class ShaderObject;
class ShaderPackageModule;
struct ShaderPackageVariant;

class MaterialAsset : public Asset
{
public:
	DECLARE_ASSET(MaterialAsset);
	constexpr static uint32 version = 2;

	MaterialAsset()
		: Asset(true)
	{
	}

	void clear() override;

	const string& getShaderTemplatePath() const;
	const string& getShaderPackagePath() const;
	const string& getShaderVariantName() const;
	const string& getVertexShaderInjectedCode() const;
	const string& getPixelShaderInjectedCode() const;
	bool hasShaderEdits() const;
	bool buildShaderSourceText(string& outShaderSourceText) const;
	bool getOrCompileEditedShaders(
		ShaderTargetPlatform targetPlatform,
		const ShaderPackageVariant& shaderVariant,
		shared_pointer<ShaderObject>& outVertexShader,
		shared_pointer<ShaderObject>& outPixelShader) const;
	static bool resolveEffectiveShaders(
		ShaderPackageModule& shaderPackageModule,
		const shared_pointer<MaterialAsset>& materialAsset,
		ShaderTargetPlatform targetPlatform,
		shared_pointer<ShaderObject>& outVertexShader,
		shared_pointer<ShaderObject>& outPixelShader);
	void setShaderTemplatePath(const string& inShaderTemplatePath);
	void setShaderPackagePath(const string& inShaderPackagePath);
	void setShaderVariantName(const string& inShaderVariantName);
	void setVertexShaderInjectedCode(const string& inVertexShaderInjectedCode);
	void setPixelShaderInjectedCode(const string& inPixelShaderInjectedCode);

	static const char* getDefaultShaderTemplatePath();
	static const char* getDefaultShaderPackagePath();
	static const char* getDefaultShaderVariantName();
	static const char* getVertexShaderInjectionToken();
	static const char* getPixelShaderInjectionToken();

	void serialize(OutputFileStream& fileStream) const override;
	void deserialize(InputFileStream& fileStream) override;

private:
	bool isDocumentBinaryLayoutCompatible(const XMLKeyValueDocument& document, bool documentHasBinary) const override;
	void clearRuntimeShaderCache() const;
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

	struct RuntimeShaderCache
	{
		ShaderTargetPlatform targetPlatform = static_cast<ShaderTargetPlatform>(0);
		uint64 shaderSourceHash = 0;
		shared_pointer<ShaderObject> vertexShader = nullptr;
		shared_pointer<ShaderObject> pixelShader = nullptr;
	};

	string shaderTemplatePath = {};
	string shaderPackagePath = {};
	string shaderVariantName = {};
	string vertexShaderInjectedCode = {};
	string pixelShaderInjectedCode = {};
	mutable RuntimeShaderCache runtimeShaderCache = {};
};
