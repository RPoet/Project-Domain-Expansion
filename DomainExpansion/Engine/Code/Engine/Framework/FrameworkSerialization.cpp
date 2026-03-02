#include "Engine/Framework/FrameworkSerialization.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Asset/MeshStreaming.h"

#include <fstream>
#include <sstream>

enum class ParseSection : uint32
{
	none = 0,
	world = 1,
	entity = 2,
};

struct EntityRecord
{
	int32 sourceId = -1;
	string type = "entity";
	bool active = true;
	int32 parentId = -1;
	Transform transform = {};
	string meshPath = {};
	uint32 lodLevel = 0;
	bool visible = true;
};

static string trimText(const string& text)
{
	size_t beginIndex = 0;
	while (beginIndex < text.length())
	{
		const char character = text[beginIndex];
		if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
		{
			break;
		}

		++beginIndex;
	}

	size_t endIndex = text.length();
	while (endIndex > beginIndex)
	{
		const char character = text[endIndex - 1];
		if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
		{
			break;
		}

		--endIndex;
	}

	return text.substr(beginIndex, endIndex - beginIndex);
}

static wstring toWideText(const string& text)
{
	wstring result = {};
	result.reserve(text.length());
	for (size_t index = 0; index < text.length(); ++index)
	{
		result.push_back(static_cast<wide_character>(static_cast<unsigned char>(text[index])));
	}

	return result;
}

static string toNarrowText(const wstring& text)
{
	string result = {};
	result.reserve(text.length());
	for (size_t index = 0; index < text.length(); ++index)
	{
		const wide_character character = text[index];
		result.push_back(character >= 0 && character <= 127
			? static_cast<char>(character)
			: '?');
	}

	return result;
}

static bool parseBoolText(const string& text, bool& outValue)
{
	if (text == "1" || text == "true" || text == "TRUE" || text == "True")
	{
		outValue = true;
		return true;
	}

	if (text == "0" || text == "false" || text == "FALSE" || text == "False")
	{
		outValue = false;
		return true;
	}

	return false;
}

static bool parseIntText(const string& text, int32& outValue)
{
	string_input_stream parser(text);
	int32 parsedValue = 0;
	parser >> parsedValue;
	if (!parser || !parser.eof())
	{
		return false;
	}

	outValue = parsedValue;
	return true;
}

static bool parseFloat3Text(
	const string& text,
	float& outX,
	float& outY,
	float& outZ)
{
	string_input_stream parser(text);
	char separatorA = 0;
	char separatorB = 0;
	parser >> outX >> separatorA >> outY >> separatorB >> outZ;
	if (!parser || !parser.eof() || separatorA != ',' || separatorB != ',')
	{
		return false;
	}

	return true;
}

static bool failParse(
	const string& worldFilePath,
	const uint32 lineNumber,
	const string& reason,
	string& outErrorText)
{
	outErrorText = reason;
	error << "[FrameworkSerialization][Error] path=" << worldFilePath
		  << " line=" << lineNumber
		  << " reason=" << outErrorText << lineBreak;
	return false;
}

bool frameworkSerializationLoadWorldFromFile(
	const string& worldFilePath,
	unique_pointer<World>& outWorld,
	string& outErrorText)
{
	outWorld.reset();
	outErrorText.clear();

	input_file_stream fileStream(worldFilePath);
	if (!fileStream.is_open())
	{
		outErrorText = "file_open_failed";
		error << "[FrameworkSerialization][Error] path=" << worldFilePath
			  << " reason=" << outErrorText << lineBreak;
		return false;
	}

	vector<EntityRecord> entityRecords;
	EntityRecord currentRecord = {};
	bool currentEntityValid = false;
	string worldName = {};
	ParseSection currentSection = ParseSection::none;

	string lineText = {};
	uint32 lineNumber = 0;
	while (std::getline(fileStream, lineText))
	{
		++lineNumber;
		const size_t commentIndex = lineText.find('#');
		if (commentIndex != string::npos)
		{
			lineText = lineText.substr(0, commentIndex);
		}

		lineText = trimText(lineText);
		if (lineText.empty())
		{
			continue;
		}

		if (lineText.front() == '[' && lineText.back() == ']')
		{
			if (currentSection == ParseSection::entity && currentEntityValid)
			{
				entityRecords.push_back(currentRecord);
				currentRecord = {};
				currentEntityValid = false;
			}

			const string sectionName = trimText(lineText.substr(1, lineText.length() - 2));
			if (sectionName == "World")
			{
				currentSection = ParseSection::world;
			}
			else if (sectionName == "Entity")
			{
				currentSection = ParseSection::entity;
				currentRecord = {};
				currentEntityValid = true;
			}
			else
			{
				currentSection = ParseSection::none;
			}

			continue;
		}

		const size_t delimiterIndex = lineText.find('=');
		if (delimiterIndex == string::npos)
		{
			return failParse(worldFilePath, lineNumber, "invalid_key_value", outErrorText);
		}

		const string key = trimText(lineText.substr(0, delimiterIndex));
		const string value = trimText(lineText.substr(delimiterIndex + 1));
		if (currentSection == ParseSection::world)
		{
			if (key == "worldName")
			{
				worldName = value;
			}
			continue;
		}

		if (currentSection != ParseSection::entity || !currentEntityValid)
		{
			continue;
		}

		if (key == "id")
		{
			if (!parseIntText(value, currentRecord.sourceId))
			{
				return failParse(worldFilePath, lineNumber, "invalid_entity_id", outErrorText);
			}
			continue;
		}

		if (key == "type")
		{
			currentRecord.type = value;
			continue;
		}

		if (key == "active")
		{
			if (!parseBoolText(value, currentRecord.active))
			{
				return failParse(worldFilePath, lineNumber, "invalid_active_state", outErrorText);
			}
			continue;
		}

		if (key == "parent")
		{
			if (!parseIntText(value, currentRecord.parentId))
			{
				return failParse(worldFilePath, lineNumber, "invalid_parent_id", outErrorText);
			}
			continue;
		}

		if (key == "position")
		{
			if (!parseFloat3Text(
				value,
				currentRecord.transform.positionX,
				currentRecord.transform.positionY,
				currentRecord.transform.positionZ))
			{
				return failParse(worldFilePath, lineNumber, "invalid_position", outErrorText);
			}
			continue;
		}

		if (key == "rotation")
		{
			if (!parseFloat3Text(
				value,
				currentRecord.transform.rotationPitch,
				currentRecord.transform.rotationYaw,
				currentRecord.transform.rotationRoll))
			{
				return failParse(worldFilePath, lineNumber, "invalid_rotation", outErrorText);
			}
			continue;
		}

		if (key == "scale")
		{
			if (!parseFloat3Text(
				value,
				currentRecord.transform.scaleX,
				currentRecord.transform.scaleY,
				currentRecord.transform.scaleZ))
			{
				return failParse(worldFilePath, lineNumber, "invalid_scale", outErrorText);
			}
			continue;
		}

		if (key == "mesh")
		{
			currentRecord.meshPath = value;
			continue;
		}

		if (key == "lod")
		{
			int32 parsedLodLevel = 0;
			if (!parseIntText(value, parsedLodLevel) || parsedLodLevel < 0)
			{
				return failParse(worldFilePath, lineNumber, "invalid_lod_level", outErrorText);
			}

			currentRecord.lodLevel = static_cast<uint32>(parsedLodLevel);
			continue;
		}

		if (key == "visible")
		{
			if (!parseBoolText(value, currentRecord.visible))
			{
				return failParse(worldFilePath, lineNumber, "invalid_mesh_visibility", outErrorText);
			}
			continue;
		}
	}

	if (currentSection == ParseSection::entity && currentEntityValid)
	{
		entityRecords.push_back(currentRecord);
	}

	if (worldName.empty())
	{
		worldName = filesystem_path(worldFilePath).stem().string();
	}

	unique_pointer<World> loadedWorld(new World(toWideText(worldName)));
	unordered_map<int32, uint32> sourceIdToEntityIndex;
	sourceIdToEntityIndex.reserve(entityRecords.size());

	for (uint32 recordIndex = 0; recordIndex < static_cast<uint32>(entityRecords.size()); ++recordIndex)
	{
		const EntityRecord& record = entityRecords[recordIndex];
		const bool createPlaceableEntity = record.type == "placeable" || record.type == "Placeable";
		const uint32 entityIndex = createPlaceableEntity
			? loadedWorld->createPlaceableEntity()
			: loadedWorld->createEntity();

		Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			outErrorText = "entity_create_failed";
			return false;
		}

		entity->activeState = record.active;
		if (createPlaceableEntity)
		{
			PlaceableEntity* placeableEntity = dynamic_cast<PlaceableEntity*>(entity);
			if (placeableEntity != nullptr)
			{
				placeableEntity->transform = record.transform;
			}
		}

		const int32 sourceId = record.sourceId >= 0
			? record.sourceId
			: static_cast<int32>(recordIndex);
		sourceIdToEntityIndex[sourceId] = entityIndex;
	}

	for (uint32 recordIndex = 0; recordIndex < static_cast<uint32>(entityRecords.size()); ++recordIndex)
	{
		const EntityRecord& record = entityRecords[recordIndex];
		const int32 sourceId = record.sourceId >= 0
			? record.sourceId
			: static_cast<int32>(recordIndex);
		const auto childEntityIt = sourceIdToEntityIndex.find(sourceId);
		if (childEntityIt == sourceIdToEntityIndex.end())
		{
			continue;
		}

		if (record.parentId < 0)
		{
			continue;
		}

		const auto parentEntityIt = sourceIdToEntityIndex.find(record.parentId);
		if (parentEntityIt == sourceIdToEntityIndex.end())
		{
			continue;
		}

		loadedWorld->addChildEntity(parentEntityIt->second, childEntityIt->second);
	}

	for (uint32 recordIndex = 0; recordIndex < static_cast<uint32>(entityRecords.size()); ++recordIndex)
	{
		const EntityRecord& record = entityRecords[recordIndex];
		if (record.meshPath.empty())
		{
			continue;
		}

		const int32 sourceId = record.sourceId >= 0
			? record.sourceId
			: static_cast<int32>(recordIndex);
		const auto entityIt = sourceIdToEntityIndex.find(sourceId);
		if (entityIt == sourceIdToEntityIndex.end())
		{
			continue;
		}

		unique_pointer<MeshComponent> meshComponent(new MeshComponent());
		meshComponent->meshRelativePath = record.meshPath;
		meshComponent->lodLevel = record.lodLevel;
		meshComponent->visible = record.visible;
		loadedWorld->attachComponent(entityIt->second, moveValue(meshComponent));
		MeshStreaming::get()->requestMesh(record.meshPath, record.lodLevel);
	}

	outWorld = moveValue(loadedWorld);
	return true;
}

bool frameworkSerializationSaveWorldToFile(
	const World& world,
	const string& worldFilePath,
	string& outErrorText)
{
	outErrorText.clear();

	if (!frameworkFileSystemEnsureParentDirectory(worldFilePath))
	{
		outErrorText = "parent_directory_create_failed";
		error << "[FrameworkSerialization][Error] path=" << worldFilePath
			  << " reason=" << outErrorText << lineBreak;
		return false;
	}

	output_file_stream fileStream(worldFilePath, output_file_stream::trunc);
	if (!fileStream.is_open())
	{
		outErrorText = "file_open_failed";
		error << "[FrameworkSerialization][Error] path=" << worldFilePath
			  << " reason=" << outErrorText << lineBreak;
		return false;
	}

	fileStream << "[World]" << '\n';
	fileStream << "version=1" << '\n';
	fileStream << "worldName=" << toNarrowText(world.getWorldName()) << '\n';
	fileStream << '\n';

	const uint32 entityCount = world.getEntityCount();
	for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
	{
		const Entity* entity = world.getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		fileStream << "[Entity]" << '\n';
		fileStream << "id=" << entityIndex << '\n';

		const PlaceableEntity* placeableEntity = dynamic_cast<const PlaceableEntity*>(entity);
		if (placeableEntity != nullptr)
		{
			fileStream << "type=placeable" << '\n';
		}
		else
		{
			fileStream << "type=entity" << '\n';
		}

		fileStream << "active=" << (entity->activeState ? 1 : 0) << '\n';
		if (entity->parentEntityIndex == invalidEntityIndex)
		{
			fileStream << "parent=-1" << '\n';
		}
		else
		{
			fileStream << "parent=" << entity->parentEntityIndex << '\n';
		}

		if (placeableEntity != nullptr)
		{
			const Transform& transform = placeableEntity->transform;
			fileStream << "position="
					   << transform.positionX << ","
					   << transform.positionY << ","
					   << transform.positionZ << '\n';
			fileStream << "rotation="
					   << transform.rotationPitch << ","
					   << transform.rotationYaw << ","
					   << transform.rotationRoll << '\n';
			fileStream << "scale="
					   << transform.scaleX << ","
					   << transform.scaleY << ","
					   << transform.scaleZ << '\n';
		}

		const MeshComponent* meshComponent = nullptr;
		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
			const Component* component = world.getComponentByIndex(componentIndex);
			meshComponent = dynamic_cast<const MeshComponent*>(component);
			if (meshComponent != nullptr)
			{
				break;
			}
		}

		if (meshComponent != nullptr)
		{
			fileStream << "mesh=" << meshComponent->meshRelativePath << '\n';
			fileStream << "lod=" << meshComponent->lodLevel << '\n';
			fileStream << "visible=" << (meshComponent->visible ? 1 : 0) << '\n';
		}

		fileStream << '\n';
	}

	return true;
}
