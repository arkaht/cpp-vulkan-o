#pragma once

namespace math
{
	template <typename T>
	T clamp( T value, T min, T max )
	{
		return value > max ? max :
			   value < min ? min :
			   value;
	}
}