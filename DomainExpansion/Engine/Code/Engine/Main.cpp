#include "Engine/Framework/ApplicationRunOptions.h"
#include "Engine/Framework/ApplicationSession.h"
#include "Engine/Platform/PlatformDefine.h"

int WINAPI wWinMain(
	HandleInstance windowInstanceHandle,
	HandleInstance previousWindowInstanceHandle,
	WideStringPointer commandLine,
	int commandShow)
{
	unused(windowInstanceHandle);
	unused(previousWindowInstanceHandle);
	unused(commandShow);

	const ApplicationRunOptions applicationRunOptions = parseApplicationRunOptions(commandLine);
	ApplicationSession applicationSession = {};
	return applicationSession.run(applicationRunOptions);
}
