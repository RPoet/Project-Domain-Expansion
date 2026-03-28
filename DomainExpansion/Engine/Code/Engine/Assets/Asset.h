#pragma once
#include "Engine/Common/XML/XML.h"
#include "Engine/Platform/PlatformDefine.h"

#define DECLARE_ASSET(assetTypeName) \
	static const char* getStaticAssetTypeName() { return #assetTypeName; } \
	const char* getAssetTypeName() const override { return getStaticAssetTypeName(); }

// Representation of engine file that can be converted runtime data.
// Document data is stored in `.deasset` XML and binary payload is stored separately in `.de`.
class Asset
{
public:
	Asset() = default;

	const string& getAssetPath() const { return assetPath; }
	const string& getName() const { return name; }
	const string& getGUID() const { return guid; }
	void setAssetPath(const string& inAssetPath) { assetPath = inAssetPath; }
	void setName(const string& inName) { name = inName; }
	void setGUID(const string& inGUID) { guid = inGUID; }
	virtual void clear()
	{
		name.clear();
		guid.clear();
	}

	// Each type runtime data is described by the `.deasset` document.
	// Additional binary payload, if any, must be serialized separately through DiskLoaderModule.
	void writeProperty(OutputFileStream& fileStream) const;
	void readProperty(const XMLKeyValueDocument& document);
	virtual void serialize(OutputFileStream& fileStream) const {};
	virtual void deserialize(InputFileStream& fileStream) {};

protected:
	virtual const char* getAssetTypeName() const = 0;
	virtual void writeAssetProperty(OutputFileStream& fileStream) const;
	virtual void readAssetProperty(const XMLKeyValueDocument& document);

	string assetPath = "";
	string name = "";
	string guid = "";

	bool hasBinary = false;
};
