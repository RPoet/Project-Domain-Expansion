#pragma once

#include "Engine/Assets/TextureAsset.h"
#include "Engine/Common/Singleton.h"

class TextureParser final : public Singleton<TextureParser>
{
public:
	enum class ImportCLIExecutionCode : int32
	{
		succeeded = 0,
		missingPath = -100,
		parseFailed = -101,
		unsupportedExtension = -103,
		fileOpenFailed = -104,
	};

	static void registerCLICommands();
	static bool supportsImportExtension(const string& extension);

	bool importFromFile(
		const string& textureFilePath,
		const string& textureAssetPath,
		TextureAsset& outTextureAsset,
		string& outErrorText) const;

private:
	friend class Singleton<TextureParser>;

	TextureParser();
	~TextureParser() = default;

	bool decodeFromFile(
		const string& textureFilePath,
		TextureAsset& outTextureAsset,
		string& outErrorText) const;
};
