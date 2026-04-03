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
	constexpr explicit Asset(const bool inHasBinary = false)
		: hasBinary(inHasBinary)
	{
	}
	Asset(const Asset&) = default;
	Asset(Asset&&) = default;
	Asset& operator=(const Asset& other)
	{
		if (this == &other)
		{
			return *this;
		}

		assert(hasBinary == other.hasBinary && "[Asset][Assert] reason=asset_binary_layout_assignment_mismatch");
		assetPath = other.assetPath;
		name = other.name;
		guid = other.guid;
		return *this;
	}
	Asset& operator=(Asset&& other)
	{
		if (this == &other)
		{
			return *this;
		}

		assert(hasBinary == other.hasBinary && "[Asset][Assert] reason=asset_binary_layout_assignment_mismatch");
		assetPath = moveValue(other.assetPath);
		name = moveValue(other.name);
		guid = moveValue(other.guid);
		return *this;
	}

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
	virtual bool isDocumentBinaryLayoutCompatible(const XMLKeyValueDocument& document, bool documentHasBinary) const;
	virtual void writeAssetProperty(OutputFileStream& fileStream) const;
	virtual void readAssetProperty(const XMLKeyValueDocument& document);

	string assetPath = "";
	string name = "";
	string guid = "";

	const bool hasBinary = false;
};
