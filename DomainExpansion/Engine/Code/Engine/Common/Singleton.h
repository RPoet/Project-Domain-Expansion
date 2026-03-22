#pragma once

template<typename type_name>
class Singleton
{
public:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static type_name& get()
	{
		static type_name instance;
		return instance;
	}

protected:
	Singleton() = default;
	~Singleton() = default;
};
