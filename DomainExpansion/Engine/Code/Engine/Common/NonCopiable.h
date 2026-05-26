#pragma once

class NonCopiable
{
protected:
	NonCopiable() = default;
	~NonCopiable() = default;

	NonCopiable(const NonCopiable&) = delete;
	NonCopiable& operator=(const NonCopiable&) = delete;
	NonCopiable(NonCopiable&&) = delete;
	NonCopiable& operator=(NonCopiable&&) = delete;
};
