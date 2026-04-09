#include "Engine/Assets/Asset.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

void Asset::clear()
{
	name.reset();
	guid.reset();
}

void Asset::writeProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	xml.writeOpenTag(fileStream, "deasset", "type", getAssetTypeName());
	xml.writeProperty(fileStream, guid);
	xml.writeProperty(fileStream, name);
	xml.writeProperty(fileStream, "hasBinary", hasBinary);
	writeAssetProperty(fileStream);
	xml.writeCloseTag(fileStream, "deasset");

	if (hasBinary)
	{
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		OutputFileStream binaryFileStream = diskLoaderModule->openBinaryAssetOutputFileStream(assetPath, true);
		serialize(binaryFileStream);
	}
}

void Asset::readProperty(const XMLKeyValueDocument& document)
{
	clear();

	XML& xml = XML::get();
	string_view assetTypeName = {};
	const bool foundAssetTypeName = document.tryGetValueView("deasset.@type", assetTypeName);
	assert(foundAssetTypeName && "[Asset][Assert] reason=asset_document_type_missing");
	assert(assetTypeName == getAssetTypeName() && "[Asset][Assert] reason=asset_document_type_mismatch");

	xml.readProperty(document, "deasset", guid);
	xml.readProperty(document, "deasset", name);

	const bool documentHasBinary = xml.readPropertyOrDefault(document, "deasset.hasBinary", false);
	const bool binaryLayoutCompatible = isDocumentBinaryLayoutCompatible(document, documentHasBinary);
	assert(binaryLayoutCompatible && "[Asset][Assert] reason=asset_document_has_binary_mismatch");
	if (!binaryLayoutCompatible)
	{
		return;
	}

	readAssetProperty(document);

	if (hasBinary)
	{
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		InputFileStream binaryFileStream = diskLoaderModule->openBinaryAssetInputFileStream(assetPath);
		deserialize(binaryFileStream);
	}
}

void Asset::writeAssetProperty(OutputFileStream& fileStream) const
{
	unused(fileStream);
}

bool Asset::isDocumentBinaryLayoutCompatible(const XMLKeyValueDocument& document, const bool documentHasBinary) const
{
	unused(document);
	return documentHasBinary == hasBinary;
}

void Asset::readAssetProperty(const XMLKeyValueDocument& document)
{
	unused(document);
}
