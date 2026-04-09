#pragma once
#include "Engine/Platform/PlatformDefine.h"

class Framework;

class Module
{
public:
	explicit Module(string&& name)
		: name(moveValue(name))
	{}

	explicit Module(const string& name)
		: name(name)
	{}

	virtual ~Module() = default;

	const string& getName() const
	{
		return name;
	}

	virtual bool init(Framework& framework) = 0;
	virtual void preUpdate() = 0;
	virtual void postUpdate() = 0;
	virtual void shutdown() = 0;

private:
	string name;
};


template<typename T>
class StaticModule : public Module
{
public:
	StaticModule(const StaticModule&) = delete;
	StaticModule& operator=(const StaticModule&) = delete;
	StaticModule(StaticModule&&) = delete;
	StaticModule& operator=(StaticModule&&) = delete;

	static shared_pointer<T>& get()
	{
		static shared_pointer<T> module(new T());
		const bool validModule = module != nullptr
			&& !module->getName().empty();
		if (!validModule)
		{
			error << "[StaticModule][Assert] reason=module_missing_or_name_missing";
			if (module != nullptr)
			{
				error << " module=" << module->getName();
			}

			error << lineBreak;
			platformAssertFailFast("module != nullptr && !module->getName().empty()", __FILE__, static_cast<int32>(__LINE__), __func__);
		}

		return module;
	}

	static T* getPtr()
	{
		static T* modulePointer = get().get();
		return modulePointer;
	}

protected:
	explicit StaticModule(string&& name)
		: Module(moveValue(name))
	{}

	explicit StaticModule(const string& name)
		: Module(name)
	{}
};
