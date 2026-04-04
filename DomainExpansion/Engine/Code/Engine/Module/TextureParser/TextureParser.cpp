#include "Engine/Module/TextureParser/TextureParser.h"

#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

static int32 getTextureImportExecutionCode(const string& errorText)
{
	if (errorText == "unsupported_extension")
	{
		return static_cast<int32>(TextureParser::ImportCLIExecutionCode::unsupportedExtension);
	}

	if (errorText == "file_open_failed")
	{
		return static_cast<int32>(TextureParser::ImportCLIExecutionCode::fileOpenFailed);
	}

	return static_cast<int32>(TextureParser::ImportCLIExecutionCode::parseFailed);
}

static int32 textureParserImportCLICommand(const vector<string>& arguments)
{
	if (arguments.size() != 1 || arguments[0].empty())
	{
		return static_cast<int32>(TextureParser::ImportCLIExecutionCode::missingPath);
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[TextureParser][Assert] reason=disk_loader_module_missing");
	if (diskLoaderModule == nullptr)
	{
		return static_cast<int32>(TextureParser::ImportCLIExecutionCode::parseFailed);
	}

	const string textureAssetPath = diskLoaderModule->resolveAssetPath(arguments[0], DiskLoaderModule::AssetFileType::document);

	TextureAsset textureAsset = {};
	string errorText = {};
	if (!TextureParser::get().importFromFile(arguments[0], textureAssetPath, textureAsset, errorText))
	{
		return getTextureImportExecutionCode(errorText);
	}

	return static_cast<int32>(TextureParser::ImportCLIExecutionCode::succeeded);
}

TextureParser::TextureParser()
{
}

void TextureParser::registerCLICommands()
{
	static bool cliCommandsRegistered = false;
	if (cliCommandsRegistered)
	{
		return;
	}

	const bool importRegistered = CLIModule::registerCommand("TextureParser.import", textureParserImportCLICommand);
	assert(importRegistered && "[TextureParser][Assert] reason=texture_parser_import_cli_register_failed");
	unused(importRegistered);
	cliCommandsRegistered = true;
}

bool TextureParser::supportsImportExtension(const string& extension)
{
	string normalizedExtension = extension;
	tolower(normalizedExtension);
	if (!normalizedExtension.empty() && normalizedExtension[0] != '.')
	{
		normalizedExtension.insert(normalizedExtension.begin(), '.');
	}

	return normalizedExtension == ".png"
		|| normalizedExtension == ".jpg"
		|| normalizedExtension == ".jpeg"
		|| normalizedExtension == ".bmp"
		|| normalizedExtension == ".gif"
		|| normalizedExtension == ".tif"
		|| normalizedExtension == ".tiff";
}

bool TextureParser::decodeFromFile(
	const string& textureFilePath,
	TextureAsset& outTextureAsset,
	string& outErrorText) const
{
	outTextureAsset.clear();
	outErrorText.clear();

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[TextureParser][Assert] reason=disk_loader_module_missing");
	if (diskLoaderModule == nullptr)
	{
		outErrorText = "parse_failed";
		return false;
	}

	string resolvedTextureFilePath = textureFilePath;
	string absoluteTextureFilePath = {};
	if (!diskLoaderModule->resolvePathFromResources(textureFilePath, absoluteTextureFilePath))
	{
		if (!diskLoaderModule->resolveAbsolutePathFromResources(textureFilePath, absoluteTextureFilePath))
		{
			outErrorText = "file_open_failed";
			return false;
		}
	}
	resolvedTextureFilePath = absoluteTextureFilePath;

	string extension = filesystem_path(resolvedTextureFilePath).extension().string();
	tolower(extension);
	if (!supportsImportExtension(extension))
	{
		outErrorText = "unsupported_extension";
		return false;
	}

	error_code fileExistsErrorCode = {};
	if (!exists(filesystem_path(resolvedTextureFilePath), fileExistsErrorCode))
	{
		outErrorText = "file_open_failed";
		return false;
	}

	bool shouldUninitializeCOM = false;
	const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (SUCCEEDED(initializeResult))
	{
		shouldUninitializeCOM = true;
	}
	else if (initializeResult != RPC_E_CHANGED_MODE)
	{
		outErrorText = "parse_failed";
		return false;
	}

	const uint32 channelCount = 4;
	uint32 textureWidth = 0;
	uint32 textureHeight = 0;
	const bool decodeSucceeded = [&]()
	{
		com_pointer<IWICImagingFactory> wicFactory = {};
		const HRESULT factoryResult = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IWICImagingFactory),
			reinterpret_cast<void**>(wicFactory.ReleaseAndGetAddressOf()));
		if (FAILED(factoryResult) || wicFactory == nullptr)
		{
			outErrorText = "parse_failed";
			return false;
		}

		const wstring textureFilePathWide = filesystem_path(resolvedTextureFilePath).wstring();
		com_pointer<IWICBitmapDecoder> bitmapDecoder = {};
		const HRESULT decoderResult = wicFactory->CreateDecoderFromFilename(
			textureFilePathWide.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand,
			bitmapDecoder.ReleaseAndGetAddressOf());
		if (FAILED(decoderResult) || bitmapDecoder == nullptr)
		{
			outErrorText = "file_open_failed";
			return false;
		}

		com_pointer<IWICBitmapFrameDecode> bitmapFrame = {};
		const HRESULT frameResult = bitmapDecoder->GetFrame(0, bitmapFrame.ReleaseAndGetAddressOf());
		if (FAILED(frameResult) || bitmapFrame == nullptr)
		{
			outErrorText = "parse_failed";
			return false;
		}

		const HRESULT sizeResult = bitmapFrame->GetSize(&textureWidth, &textureHeight);
		if (FAILED(sizeResult) || textureWidth == 0 || textureHeight == 0)
		{
			outErrorText = "parse_failed";
			return false;
		}

		com_pointer<IWICFormatConverter> formatConverter = {};
		const HRESULT converterCreateResult = wicFactory->CreateFormatConverter(formatConverter.ReleaseAndGetAddressOf());
		if (FAILED(converterCreateResult) || formatConverter == nullptr)
		{
			outErrorText = "parse_failed";
			return false;
		}

		const HRESULT converterInitializeResult = formatConverter->Initialize(
			bitmapFrame.Get(),
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(converterInitializeResult))
		{
			outErrorText = "parse_failed";
			return false;
		}

		const uint32 rowPitchInBytes = textureWidth * channelCount;
		vector<char>& pixelData = outTextureAsset.getPixelData();
		pixelData.resize(static_cast<size_t>(rowPitchInBytes) * static_cast<size_t>(textureHeight));
		const HRESULT copyPixelsResult = formatConverter->CopyPixels(
			nullptr,
			rowPitchInBytes,
			static_cast<uint32>(pixelData.size()),
			reinterpret_cast<BYTE*>(pixelData.data()));
		if (FAILED(copyPixelsResult))
		{
			outErrorText = "parse_failed";
			return false;
		}

		return true;
	}();

	if (shouldUninitializeCOM)
	{
		CoUninitialize();
	}

	if (!decodeSucceeded)
	{
		return false;
	}

	outTextureAsset.setWidth(textureWidth);
	outTextureAsset.setHeight(textureHeight);
	outTextureAsset.setChannelCount(channelCount);
	outTextureAsset.setSource(textureFilePath);
	return outTextureAsset.isValid();
}

bool TextureParser::importFromFile(
	const string& textureFilePath,
	const string& textureAssetPath,
	TextureAsset& outTextureAsset,
	string& outErrorText) const
{
	outTextureAsset.clear();
	outErrorText.clear();

	if (!decodeFromFile(textureFilePath, outTextureAsset, outErrorText))
	{
		return false;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[TextureParser][Assert] reason=disk_loader_module_missing");
	if (diskLoaderModule == nullptr)
	{
		outErrorText = "parse_failed";
		return false;
	}

	outTextureAsset.setName(filesystem_path(textureAssetPath).stem().string());
	outTextureAsset.setAssetPath(textureAssetPath);

	const string textureAssetAbsolutePath = diskLoaderModule->resolveAbsolutePathFromResources(textureAssetPath);
	OutputFileStream textureAssetFileStream = diskLoaderModule->openOutputFileStream(textureAssetAbsolutePath, false, true);
	outTextureAsset.writeProperty(textureAssetFileStream);
	return true;
}
