#include "Engine/Module/ShaderCompiler/ShaderCompiler.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Render/Backends/Dx12/Dx12Shader.h"

#include <dxcapi.h>

static bool validateShaderCompileRequest(const ShaderCompileRequest& compileRequest)
{
	const bool validStage = getShaderStageIndex(compileRequest.stage) != uint32MaxValue;
	const bool validSourcePath = !compileRequest.sourceRelativePath.empty();
	const bool validEntryPoint = !compileRequest.entryPoint.empty();
	const bool validTargetPlatform = getShaderTargetPlatformIndex(compileRequest.targetPlatform) != uint32MaxValue;
	const bool validProfile = !compileRequest.profile.empty();
	return validStage && validSourcePath && validEntryPoint && validTargetPlatform && validProfile;
}

static bool loadDxCompilerCreateInstanceFunction(decltype(&DxcCreateInstance)& outDxcCreateInstance)
{
	outDxcCreateInstance = nullptr;

	static HMODULE dxCompilerModule = nullptr;
	static decltype(&DxcCreateInstance) dxCompilerCreateInstance = nullptr;
	if (dxCompilerCreateInstance != nullptr)
	{
		outDxcCreateInstance = dxCompilerCreateInstance;
		return true;
	}

	auto tryLoadDxCompilerModule = [&](const string& dxCompilerDllPathText)
	{
		HMODULE loadedDxCompilerModule = LoadLibraryA(dxCompilerDllPathText.c_str());
		if (loadedDxCompilerModule != nullptr)
		{
			decltype(&DxcCreateInstance) loadedDxCompilerCreateInstance =
				reinterpret_cast<decltype(&DxcCreateInstance)>(GetProcAddress(loadedDxCompilerModule, "DxcCreateInstance"));
			if (loadedDxCompilerCreateInstance != nullptr)
			{
				dxCompilerModule = loadedDxCompilerModule;
				dxCompilerCreateInstance = loadedDxCompilerCreateInstance;
				outDxcCreateInstance = dxCompilerCreateInstance;
				return true;
			}

			FreeLibrary(loadedDxCompilerModule);
		}

		return false;
	};

	if (tryLoadDxCompilerModule("dxcompiler.dll"))
	{
		return true;
	}

	vector<filesystem_path> dxCompilerDllCandidatePaths = {};
	const filesystem_path windowsKitsBinRoot = "C:\\Program Files (x86)\\Windows Kits\\10\\bin";
	if (exists(windowsKitsBinRoot) && is_directory(windowsKitsBinRoot))
	{
		for (const filesystem_directory_entry& windowsKitsBinEntry : filesystem_directory_iterator(windowsKitsBinRoot))
		{
			if (!windowsKitsBinEntry.is_directory())
			{
				continue;
			}

			const filesystem_path dxCompilerDllCandidatePath = windowsKitsBinEntry.path() / "x64" / "dxcompiler.dll";
			if (exists(dxCompilerDllCandidatePath))
			{
				dxCompilerDllCandidatePaths.push_back(dxCompilerDllCandidatePath);
			}
		}
	}

	sort(
		dxCompilerDllCandidatePaths.begin(),
		dxCompilerDllCandidatePaths.end(),
		[](const filesystem_path& leftPath, const filesystem_path& rightPath)
		{
			return leftPath.string() > rightPath.string();
		});
	for (const filesystem_path& dxCompilerDllCandidatePath : dxCompilerDllCandidatePaths)
	{
		if (tryLoadDxCompilerModule(dxCompilerDllCandidatePath.string()))
		{
			return true;
		}
	}

	return false;
}

static bool createCompiledShaderObject(
	const ShaderCompileRequest& compileRequest,
	vector<char>&& shaderByteCode,
	shared_pointer<ShaderAsset>& outShaderAsset,
	shared_pointer<ShaderObject>& outShaderObject)
{
	outShaderAsset = nullptr;
	outShaderObject = nullptr;

	ShaderLoadRequest shaderLoadRequest{
		.stage = compileRequest.stage,
		.sourceRelativePath = compileRequest.sourceRelativePath,
		.entryPoint = compileRequest.entryPoint,
		.definesHash = compileRequest.definesHash,
	};

	shared_pointer<ShaderAsset> shaderAsset(new ShaderAsset());
	if (shaderAsset == nullptr || !shaderAsset->initialize(shaderLoadRequest))
	{
		return false;
	}

	ShaderBinaryLoadRequest shaderBinaryLoadRequest{
		.targetPlatform = compileRequest.targetPlatform,
		.binaryRelativePath = !compileRequest.outputBinaryRelativePath.empty()
			? compileRequest.outputBinaryRelativePath
			: (compileRequest.sourceRelativePath + "|" + compileRequest.entryPoint + "|memory"),
		.profile = compileRequest.profile,
	};
	if (compileRequest.targetPlatform == ShaderTargetPlatform::dx12)
	{
		shared_pointer<Dx12ShaderObject> dx12ShaderObject(new Dx12ShaderObject());
		if (dx12ShaderObject == nullptr
			|| !dx12ShaderObject->initialize(shaderAsset, shaderBinaryLoadRequest, moveValue(shaderByteCode)))
		{
			return false;
		}

		outShaderAsset = shaderAsset;
		outShaderObject = dx12ShaderObject;
		return true;
	}

	return false;
}

static bool compileShaderSourceText(
	const ShaderCompileRequest& compileRequest,
	const char* sourceText,
	const string& sourceNameText,
	ShaderCompileResult& outCompileResult)
{
	outCompileResult.clear();
	if (!validateShaderCompileRequest(compileRequest) || sourceText == nullptr)
	{
		return false;
	}

	assert(!compileRequest.profile.empty() && "[ShaderCompiler][Assert] reason=profile_missing");

	const bool supportedTargetPlatform = compileRequest.targetPlatform == ShaderTargetPlatform::dx12;
	assert(supportedTargetPlatform && "[ShaderCompiler][Assert] reason=unsupported_target_platform");
	if (!supportedTargetPlatform)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=unsupported_target_platform";
		return false;
	}

	decltype(&DxcCreateInstance) dxcCreateInstance = nullptr;
	if (!loadDxCompilerCreateInstanceFunction(dxcCreateInstance))
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=dxcompiler_not_available";
		return false;
	}

	com_pointer<IDxcUtils> dxcUtils = nullptr;
	com_pointer<IDxcCompiler3> dxcCompiler = nullptr;
	if (FAILED(dxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf())))
		|| FAILED(dxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler.GetAddressOf())))
		|| dxcUtils == nullptr
		|| dxcCompiler == nullptr)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=dxcompiler_create_instance_failed";
		return false;
	}

	com_pointer<IDxcIncludeHandler> includeHandler = nullptr;
	if (FAILED(dxcUtils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf())) || includeHandler == nullptr)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=dxcompiler_include_handler_create_failed";
		return false;
	}

	const filesystem_path sourcePath = !sourceNameText.empty()
		? filesystem_path(sourceNameText)
		: filesystem_path(compileRequest.sourceRelativePath);
	const filesystem_path includeDirectoryPath = sourcePath.has_parent_path() ? sourcePath.parent_path() : filesystem_path();

	auto toWideText = [](const string& text)
	{
		return wstring(text.begin(), text.end());
	};

	vector<wstring> dxcArgumentStorage = {};
	vector<const wide_character*> dxcArguments = {};
	dxcArgumentStorage.reserve(10);
	dxcArguments.reserve(10);

	auto pushDxCompilerArgument = [&](const wstring& argumentText)
	{
		dxcArgumentStorage.push_back(argumentText);
		dxcArguments.push_back(dxcArgumentStorage.back().c_str());
	};

	pushDxCompilerArgument(!sourcePath.empty() ? sourcePath.wstring() : L"ShaderCompilerMemorySource.hlsl");
	pushDxCompilerArgument(L"-E");
	pushDxCompilerArgument(toWideText(compileRequest.entryPoint));
	pushDxCompilerArgument(L"-T");
	pushDxCompilerArgument(toWideText(compileRequest.profile));
	if (!includeDirectoryPath.empty())
	{
		pushDxCompilerArgument(L"-I");
		pushDxCompilerArgument(includeDirectoryPath.wstring());
	}
#if defined(_DEBUG)
	pushDxCompilerArgument(L"-Zi");
	pushDxCompilerArgument(L"-Od");
#else
	pushDxCompilerArgument(L"-O3");
#endif

	DxcBuffer sourceBuffer{
		.Ptr = sourceText,
		.Size = strlen(sourceText),
		.Encoding = DXC_CP_UTF8,
	};

	com_pointer<IDxcResult> compiledShaderResult = nullptr;
	const HRESULT compileDispatchResult = dxcCompiler->Compile(
		&sourceBuffer,
		dxcArguments.data(),
		static_cast<uint32>(dxcArguments.size()),
		includeHandler.Get(),
		IID_PPV_ARGS(compiledShaderResult.GetAddressOf()));
	if (FAILED(compileDispatchResult) || compiledShaderResult == nullptr)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=dxcompiler_dispatch_failed";
		return false;
	}

	com_pointer<IDxcBlobUtf8> diagnosticBlob = nullptr;
	compiledShaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(diagnosticBlob.GetAddressOf()), nullptr);
	bool hasDiagnosticText = diagnosticBlob != nullptr;
	if (hasDiagnosticText)
	{
		hasDiagnosticText = diagnosticBlob->GetStringPointer() != nullptr && diagnosticBlob->GetStringLength() != 0;
	}

	outCompileResult.diagnosticText = hasDiagnosticText
		? string(diagnosticBlob->GetStringPointer(), static_cast<size_t>(diagnosticBlob->GetStringLength()))
		: string();

	HRESULT compileResult = E_FAIL;
	if (FAILED(compiledShaderResult->GetStatus(&compileResult)) || FAILED(compileResult))
	{
		return false;
	}

	com_pointer<IDxcBlob> compiledShaderBlob = nullptr;
	if (FAILED(compiledShaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(compiledShaderBlob.GetAddressOf()), nullptr))
		|| compiledShaderBlob == nullptr
		|| compiledShaderBlob->GetBufferSize() == 0)
	{
		return false;
	}

	vector<char> shaderByteCode = {};
	shaderByteCode.resize(static_cast<size_t>(compiledShaderBlob->GetBufferSize()));
	memcpy(
		shaderByteCode.data(),
		compiledShaderBlob->GetBufferPointer(),
		static_cast<size_t>(compiledShaderBlob->GetBufferSize()));

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	if (!compileRequest.outputBinaryRelativePath.empty())
	{
		assert(diskLoaderModule != nullptr && "[ShaderCompiler][Assert] reason=disk_loader_module_missing");
		string outputAbsolutePath = {};
		const bool resolvedOutputAbsolutePath = diskLoaderModule->resolveAbsolutePathFromResources(compileRequest.outputBinaryRelativePath, outputAbsolutePath);
		assert(resolvedOutputAbsolutePath && "[ShaderCompiler][Assert] reason=output_path_resolve_failed");
		if (!resolvedOutputAbsolutePath)
		{
			outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=output_path_resolve_failed";
			return false;
		}

		if (!diskLoaderModule->saveBinaryFile(outputAbsolutePath, shaderByteCode))
		{
			outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=output_binary_write_failed path=" + outputAbsolutePath;
			return false;
		}

		outCompileResult.wroteOutputBinary = true;
	}

	if (!createCompiledShaderObject(compileRequest, moveValue(shaderByteCode), outCompileResult.shaderAsset, outCompileResult.shaderObject))
	{
		outCompileResult.success = false;
		outCompileResult.shaderAsset = nullptr;
		outCompileResult.shaderObject = nullptr;
		outCompileResult.wroteOutputBinary = !compileRequest.outputBinaryRelativePath.empty();
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=compiled_shader_object_create_failed";
		return false;
	}

	outCompileResult.success = true;
	return true;
}

bool ShaderCompiler::compileFromFile(
	const ShaderCompileRequest& compileRequest,
	ShaderCompileResult& outCompileResult) const
{
	outCompileResult.clear();
	if (!validateShaderCompileRequest(compileRequest))
	{
		return false;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[ShaderCompiler][Assert] reason=disk_loader_module_missing");
	string sourceAbsolutePath = {};
	const bool resolvedSourceAbsolutePath = diskLoaderModule->resolvePathFromResources(compileRequest.sourceRelativePath, sourceAbsolutePath);
	assert(resolvedSourceAbsolutePath && "[ShaderCompiler][Assert] reason=source_path_resolve_failed");
	if (!resolvedSourceAbsolutePath)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=source_path_resolve_failed";
		return false;
	}

	InputFileStream sourceFileStream = diskLoaderModule->openInputFileStream(sourceAbsolutePath, true);
	if (!sourceFileStream.is_open())
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=source_file_open_failed path=" + sourceAbsolutePath;
		return false;
	}

	sourceFileStream.seekg(0, InputFileStream::end);
	const stream_position sourceFileSize = sourceFileStream.tellg();
	if (sourceFileSize <= 0)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=source_file_empty path=" + sourceAbsolutePath;
		return false;
	}

	string sourceText = {};
	sourceText.resize(static_cast<size_t>(sourceFileSize));
	sourceFileStream.seekg(0, InputFileStream::beg);
	sourceFileStream.read(sourceText.data(), static_cast<stream_size>(sourceText.size()));
	if (!sourceFileStream)
	{
		outCompileResult.diagnosticText = "[ShaderCompiler][CompileFailed] reason=source_file_read_failed path=" + sourceAbsolutePath;
		return false;
	}

	return compileShaderSourceText(compileRequest, sourceText.c_str(), sourceAbsolutePath, outCompileResult);
}

bool ShaderCompiler::compileFromMemory(
	const ShaderCompileRequest& compileRequest,
	const char* sourceText,
	ShaderCompileResult& outCompileResult) const
{
	outCompileResult.clear();
	if (!validateShaderCompileRequest(compileRequest) || sourceText == nullptr || sourceText[0] == '\0')
	{
		return false;
	}

	string sourceAbsolutePath = {};
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	if (diskLoaderModule == nullptr || !diskLoaderModule->resolvePathFromResources(compileRequest.sourceRelativePath, sourceAbsolutePath))
	{
		sourceAbsolutePath = compileRequest.sourceRelativePath;
	}

	return compileShaderSourceText(compileRequest, sourceText, sourceAbsolutePath, outCompileResult);
}
