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
	virtual void update() = 0;
	virtual void shutdown() = 0;

private:
	string name;
};


template<typename T>
class StaticModule : public Module
{
public:
	explicit StaticModule(string&& name)
		: Module(moveValue(name))
	{}

	explicit StaticModule(const string& name)
		: Module(name)
	{}

	static shared_pointer<T>& get()
	{
		static shared_pointer<T> module(new T());
		return module;
	}
};
