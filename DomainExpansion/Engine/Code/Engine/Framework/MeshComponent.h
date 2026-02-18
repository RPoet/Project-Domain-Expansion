#pragma once

#include "Engine/Framework/Component.h"

class MeshComponent final : public Component
{
public:
	string meshRelativePath = {};
	uint32 lodLevel = 0;
	bool visible = true;
};
