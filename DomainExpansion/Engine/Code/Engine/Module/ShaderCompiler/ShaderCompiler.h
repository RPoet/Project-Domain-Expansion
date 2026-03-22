#pragma once

#include "Render/Shader.h"

struct ShaderCompileRequest
{
	ShaderStage stage = ShaderStage::unknown;
	string sourceRelativePath = {};
	string entryPoint = {};
	ShaderTargetPlatform targetPlatform = ShaderTargetPlatform::unknown;
	string profile = {};
	string outputBinaryRelativePath = {};
};

struct ShaderCompileResult
{
	bool success = false;
	string diagnosticText = {};
	shared_pointer<ShaderAsset> shaderAsset = nullptr;
	shared_pointer<ShaderObject> shaderObject = nullptr;
	bool wroteOutputBinary = false;

	void clear()
	{
		success = false;
		diagnosticText.clear();
		shaderAsset = nullptr;
		shaderObject = nullptr;
		wroteOutputBinary = false;
	}
};

class ShaderCompiler
{
public:
	bool compileFromFile(const ShaderCompileRequest& compileRequest, ShaderCompileResult& outCompileResult) const;
	bool compileFromMemory(
		const ShaderCompileRequest& compileRequest,
		const char* sourceText,
		ShaderCompileResult& outCompileResult) const;
};
