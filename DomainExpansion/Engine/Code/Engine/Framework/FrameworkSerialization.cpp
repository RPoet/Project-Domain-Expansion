#include "Engine/Framework/FrameworkSerialization.h"

#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/EditorCameraMovementComponent.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Asset/DiskLoaderModule.h"
#include "Engine/Module/Asset/MeshStreaming.h"

#include <fstream>
#include <sstream>

enum class ParseSection : uint32
{
	none = 0,
	world = 1,
	entity = 2,
};

struct Temp_EntityRecord
{
	int32 sourceId = -1;
	string type = "entity";
	string name = {};
	bool active = true;
	int32 parentId = -1;
	Transform transform = {};
	string meshPath = {};
	uint32 lodLevel = 0;
	bool visible = true;
	bool hasCameraComponent = false;
	bool cameraEditor = false;
	bool cameraPrimary = false;
	float cameraFieldOfViewYDegrees = 60.0f;
	float cameraNearPlane = 0.1f;
	float cameraFarPlane = 100.0f;
	bool hasEditorCameraMovementComponent = false;
	float editorCameraMovementSpeed = 4.0f;
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

static MeshComponent* getFirstMeshComponent(Entity* entity, World* world)
{
	if (entity == nullptr || world == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != MeshComponent::staticComponentType)
		{
			continue;
		}

		return static_cast<MeshComponent*>(component);
	}

	return nullptr;
}

static CameraComponent* getFirstCameraComponent(Entity* entity, World* world)
{
	if (entity == nullptr || world == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
		{
			continue;
		}

		return static_cast<CameraComponent*>(component);
	}

	return nullptr;
}

static EditorCameraMovementComponent* getFirstEditorCameraMovementComponent(Entity* entity, World* world)
{
	if (entity == nullptr || world == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != EditorCameraMovementComponent::staticComponentType)
		{
			continue;
		}

		return static_cast<EditorCameraMovementComponent*>(component);
	}

	return nullptr;
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

	vector<Temp_EntityRecord> entityRecords;
	Temp_EntityRecord currentRecord = {};
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

		if (key == "name")
		{
			currentRecord.name = value;
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

		if (key == "cameraPrimary")
		{
			currentRecord.hasCameraComponent = true;
			if (!parseBoolText(value, currentRecord.cameraPrimary))
			{
				return failParse(worldFilePath, lineNumber, "invalid_camera_primary", outErrorText);
			}

			continue;
		}

		if (key == "cameraEditor")
		{
			currentRecord.hasCameraComponent = true;
			if (!parseBoolText(value, currentRecord.cameraEditor))
			{
				return failParse(worldFilePath, lineNumber, "invalid_camera_editor", outErrorText);
			}

			continue;
		}

		if (key == "cameraFieldOfViewYDegrees")
		{
			currentRecord.hasCameraComponent = true;
			string_input_stream parser(value);
			parser >> currentRecord.cameraFieldOfViewYDegrees;
			if (!parser || !parser.eof())
			{
				return failParse(worldFilePath, lineNumber, "invalid_camera_field_of_view", outErrorText);
			}

			continue;
		}

		if (key == "cameraNearPlane")
		{
			currentRecord.hasCameraComponent = true;
			string_input_stream parser(value);
			parser >> currentRecord.cameraNearPlane;
			if (!parser || !parser.eof())
			{
				return failParse(worldFilePath, lineNumber, "invalid_camera_near_plane", outErrorText);
			}

			continue;
		}

		if (key == "cameraFarPlane")
		{
			currentRecord.hasCameraComponent = true;
			string_input_stream parser(value);
			parser >> currentRecord.cameraFarPlane;
			if (!parser || !parser.eof())
			{
				return failParse(worldFilePath, lineNumber, "invalid_camera_far_plane", outErrorText);
			}

			continue;
		}

		if (key == "editorCameraMovementSpeed")
		{
			currentRecord.hasEditorCameraMovementComponent = true;
			string_input_stream parser(value);
			parser >> currentRecord.editorCameraMovementSpeed;
			if (!parser || !parser.eof())
			{
				return failParse(worldFilePath, lineNumber, "invalid_editor_camera_movement_speed", outErrorText);
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
		const Temp_EntityRecord& record = entityRecords[recordIndex];
		const bool createPlaceableEntity = record.type == "placeable" || record.type == "Placeable";
		if (record.hasCameraComponent && !createPlaceableEntity)
		{
			assert(false && "[FrameworkSerialization][Assert] reason=camera_requires_placeable_entity");
			outErrorText = "camera_requires_placeable_entity";
			return false;
		}

		if (record.hasEditorCameraMovementComponent
			&& (!createPlaceableEntity || !record.hasCameraComponent || !record.cameraEditor))
		{
			assert(false && "[FrameworkSerialization][Assert] reason=editor_camera_movement_requires_editor_camera");
			outErrorText = "editor_camera_movement_requires_editor_camera";
			return false;
		}

		const uint32 entityIndex = createPlaceableEntity
			? loadedWorld->createPlaceableEntity()
			: loadedWorld->createEntity();

		Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			outErrorText = "entity_create_failed";
			return false;
		}

		entity->setName(record.name);
		entity->setActive(record.active);
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
		const Temp_EntityRecord& record = entityRecords[recordIndex];
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
		const Temp_EntityRecord& record = entityRecords[recordIndex];
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

	for (uint32 recordIndex = 0; recordIndex < static_cast<uint32>(entityRecords.size()); ++recordIndex)
	{
		const Temp_EntityRecord& record = entityRecords[recordIndex];
		if (!record.hasCameraComponent)
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

		unique_pointer<CameraComponent> cameraComponent(new CameraComponent());
		cameraComponent->editorCamera = record.cameraEditor;
		cameraComponent->primary = record.cameraPrimary;
		cameraComponent->fieldOfViewYDegrees = record.cameraFieldOfViewYDegrees;
		cameraComponent->nearPlane = record.cameraNearPlane;
		cameraComponent->farPlane = record.cameraFarPlane;
		loadedWorld->attachComponent(entityIt->second, moveValue(cameraComponent));
	}

	for (uint32 recordIndex = 0; recordIndex < static_cast<uint32>(entityRecords.size()); ++recordIndex)
	{
		const Temp_EntityRecord& record = entityRecords[recordIndex];
		if (!record.hasEditorCameraMovementComponent)
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

		unique_pointer<EditorCameraMovementComponent> editorCameraMovementComponent(new EditorCameraMovementComponent());
		editorCameraMovementComponent->setMovementSpeed(record.editorCameraMovementSpeed);
		loadedWorld->attachComponent(entityIt->second, moveValue(editorCameraMovementComponent));
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

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[FrameworkSerialization][Assert] reason=disk_loader_module_missing");
	if (!diskLoaderModule->ensureParentDirectory(worldFilePath))
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

		CameraComponent* cameraComponent = getFirstCameraComponent(const_cast<Entity*>(entity), const_cast<World*>(&world));
		EditorCameraMovementComponent* editorCameraMovementComponent =
			getFirstEditorCameraMovementComponent(const_cast<Entity*>(entity), const_cast<World*>(&world));

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

		if (!entity->getName().empty())
		{
			fileStream << "name=" << entity->getName() << '\n';
		}

		fileStream << "active=" << (entity->isActive() ? 1 : 0) << '\n';
		if (entity->getParentEntityIndex() == invalidEntityIndex)
		{
			fileStream << "parent=-1" << '\n';
		}
		else
		{
			fileStream << "parent=" << entity->getParentEntityIndex() << '\n';
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

		MeshComponent* meshComponent = getFirstMeshComponent(const_cast<Entity*>(entity), const_cast<World*>(&world));

		if (meshComponent != nullptr)
		{
			fileStream << "mesh=" << meshComponent->meshRelativePath << '\n';
			fileStream << "lod=" << meshComponent->lodLevel << '\n';
			fileStream << "visible=" << (meshComponent->visible ? 1 : 0) << '\n';
		}

		if (cameraComponent != nullptr)
		{
			fileStream << "cameraEditor=" << (cameraComponent->editorCamera ? 1 : 0) << '\n';
			fileStream << "cameraPrimary=" << (cameraComponent->primary ? 1 : 0) << '\n';
			fileStream << "cameraFieldOfViewYDegrees=" << cameraComponent->fieldOfViewYDegrees << '\n';
			fileStream << "cameraNearPlane=" << cameraComponent->nearPlane << '\n';
			fileStream << "cameraFarPlane=" << cameraComponent->farPlane << '\n';
		}

		if (editorCameraMovementComponent != nullptr)
		{
			fileStream << "editorCameraMovementSpeed=" << editorCameraMovementComponent->getMovementSpeed() << '\n';
		}

		fileStream << '\n';
	}

	return true;
}
