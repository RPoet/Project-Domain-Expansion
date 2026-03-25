#include "Engine/Module/ShaderCompiler/ShaderCompiler.h"

bool ShaderCompiler::compileFromFile(
	const ShaderCompileRequest& compileRequest,
	ShaderCompileResult& outCompileResult) const
{
	outCompileResult.clear();
	unused(compileRequest);
	assert(false && "[ShaderCompiler][Assert] reason=not_implemented_yet mode=file");
}

bool ShaderCompiler::compileFromMemory(
	const ShaderCompileRequest& compileRequest,
	const char* sourceText,
	ShaderCompileResult& outCompileResult) const
{
	unused(sourceText);
	outCompileResult.clear();
	unused(compileRequest);
	assert(false && "[ShaderCompiler][Assert] reason=not_implemented_yet mode=memory");
}
