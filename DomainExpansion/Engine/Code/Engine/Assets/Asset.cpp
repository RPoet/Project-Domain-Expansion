#include "Engine/Assets/Asset.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

void Asset::writeProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	xml.writeOpenTag(fileStream, "deasset", "type", getAssetTypeName());
	xml.writeProperty(fileStream, "guid", guid);
	xml.writeProperty(fileStream, "name", name);
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
	const string* assetTypeName = document.find("deasset.@type");
	assert(assetTypeName != nullptr && "[Asset][Assert] reason=asset_document_type_missing");
	assert(*assetTypeName == getAssetTypeName() && "[Asset][Assert] reason=asset_document_type_mismatch");

	xml.readProperty(document, "deasset.guid", guid);
	xml.readProperty(document, "deasset.name", name);
	xml.readProperty(document, "deasset.hasBinary", hasBinary);

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

void Asset::readAssetProperty(const XMLKeyValueDocument& document)
{
	unused(document);
}
