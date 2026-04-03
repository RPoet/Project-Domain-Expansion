#include "Engine/Assets/TextureAsset.h"

#include "Engine/Common/Container/Vector.h"
#include "Engine/Common/FileStream.h"

static size_t computeTextureAssetExpectedPixelDataSize(
	const uint32 width,
	const uint32 height,
	const uint32 channelCount)
{
	return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channelCount);
}

void TextureAsset::clear()
{
	Asset::clear();
	width = 0;
	height = 0;
	channelCount = 4;
	source.clear();
	pixelData.clear();
}

uint32 TextureAsset::getWidth() const
{
	return width;
}

uint32 TextureAsset::getHeight() const
{
	return height;
}

uint32 TextureAsset::getChannelCount() const
{
	return channelCount;
}

const string& TextureAsset::getSource() const
{
	return source;
}

vector<char>& TextureAsset::getPixelData()
{
	return pixelData;
}

const vector<char>& TextureAsset::getPixelData() const
{
	return pixelData;
}

void TextureAsset::setWidth(const uint32 inWidth)
{
	width = inWidth;
}

void TextureAsset::setHeight(const uint32 inHeight)
{
	height = inHeight;
}

void TextureAsset::setChannelCount(const uint32 inChannelCount)
{
	channelCount = inChannelCount;
}

void TextureAsset::setSource(const string& inSource)
{
	source = inSource;
}

bool TextureAsset::isValid() const
{
	if (width == 0 || height == 0 || channelCount == 0)
	{
		return false;
	}

	return pixelData.size() == computeTextureAssetExpectedPixelDataSize(width, height, channelCount);
}

void TextureAsset::writeAssetProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	xml.writeProperty(fileStream, "version", version);
	xml.writeProperty(fileStream, "source", source);
	xml.writeProperty(fileStream, "width", width);
	xml.writeProperty(fileStream, "height", height);
	xml.writeProperty(fileStream, "channelCount", channelCount);
}

void TextureAsset::readAssetProperty(const XMLKeyValueDocument& document)
{
	XML& xml = XML::get();

	uint32 documentVersion = uint32MaxValue;
	const bool hasVersion = xml.readProperty(document, "deasset.version", documentVersion);
	assert(hasVersion && "[TextureAsset][Assert] reason=document_version_missing");
	assert(documentVersion == version && "[TextureAsset][Assert] reason=document_version_mismatch");

	source.clear();
	width = 0;
	height = 0;
	channelCount = 4;
	pixelData.clear();
	xml.readProperty(document, "deasset.source", source);
	xml.readProperty(document, "deasset.width", width);
	xml.readProperty(document, "deasset.height", height);
	xml.readProperty(document, "deasset.channelCount", channelCount);
	assert(width > 0 && height > 0 && channelCount > 0 && "[TextureAsset][Assert] reason=document_dimension_invalid");
}

void TextureAsset::serialize(OutputFileStream& fileStream) const
{
	assert(isValid() && "[TextureAsset][Assert] reason=serialize_texture_asset_invalid");
	fileStream << pixelData;
}

void TextureAsset::deserialize(InputFileStream& fileStream)
{
	pixelData.clear();
	fileStream >> pixelData;
	assert(isValid() && "[TextureAsset][Assert] reason=deserialize_texture_asset_invalid");
}
