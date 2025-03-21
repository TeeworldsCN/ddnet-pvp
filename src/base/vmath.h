/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_VMATH_H
#define BASE_VMATH_H

#include <math.h>

#include "math.h"

// ------------------------------------

template<typename T>
class vector2_base
{
public:
	union
	{
		T x, u;
	};
	union
	{
		T y, v;
	};

	vector2_base() = default;
	vector2_base(T nx, T ny)
	{
		x = nx;
		y = ny;
	}

	vector2_base operator-() const { return vector2_base(-x, -y); }
	vector2_base operator-(const vector2_base &v) const { return vector2_base(x - v.x, y - v.y); }
	vector2_base operator+(const vector2_base &v) const { return vector2_base(x + v.x, y + v.y); }
	vector2_base operator*(const T v) const { return vector2_base(x * v, y * v); }
	vector2_base operator*(const vector2_base &v) const { return vector2_base(x * v.x, y * v.y); }
	vector2_base operator/(const T v) const { return vector2_base(x / v, y / v); }
	vector2_base operator/(const vector2_base &v) const { return vector2_base(x / v.x, y / v.y); }

	const vector2_base &operator+=(const vector2_base &v)
	{
		x += v.x;
		y += v.y;
		return *this;
	}
	const vector2_base &operator-=(const vector2_base &v)
	{
		x -= v.x;
		y -= v.y;
		return *this;
	}
	const vector2_base &operator*=(const T v)
	{
		x *= v;
		y *= v;
		return *this;
	}
	const vector2_base &operator*=(const vector2_base &v)
	{
		x *= v.x;
		y *= v.y;
		return *this;
	}
	const vector2_base &operator/=(const T v)
	{
		x /= v;
		y /= v;
		return *this;
	}
	const vector2_base &operator/=(const vector2_base &v)
	{
		x /= v.x;
		y /= v.y;
		return *this;
	}

	bool operator==(const vector2_base &v) const { return x == v.x && y == v.y; } // TODO: do this with an eps instead
	bool operator!=(const vector2_base &v) const { return x != v.x || y != v.y; }

	T &operator[](const int index) { return index ? y : x; }
};

template<typename T>
inline vector2_base<T> rotate(const vector2_base<T> &a, float angle)
{
	angle = angle * pi / 180.0f;
	float s = sinf(angle);
	float c = cosf(angle);
	return vector2_base<T>((T)(c * a.x - s * a.y), (T)(s * a.x + c * a.y));
}

template<typename T>
inline T distance(const vector2_base<T> a, const vector2_base<T> &b)
{
	return length(a - b);
}

template<typename T>
inline T dot(const vector2_base<T> a, const vector2_base<T> &b)
{
	return a.x * b.x + a.y * b.y;
}

inline float length(const vector2_base<float> &a)
{
	return sqrtf(a.x * a.x + a.y * a.y);
}

inline float angle(const vector2_base<float> &a)
{
	if(a.x == 0 && a.y == 0)
		return 0.0f;
	else if(a.x == 0)
		return a.y < 0 ? -pi / 2 : pi / 2;
	float result = atanf(a.y / a.x);
	if(a.x < 0)
		result = result + pi;
	return result;
}

template<typename T>
inline vector2_base<T> normalize_pre_length(const vector2_base<T> &v, T len)
{
	return vector2_base<T>(v.x / len, v.y / len);
	float l = (float)(1.0f / sqrtf(v.x * v.x + v.y * v.y));
	return vector2_base<float>(v.x * l, v.y * l);
}

inline vector2_base<float> normalize(const vector2_base<float> &v)
{
	float l = (float)(1.0f / sqrtf(v.x * v.x + v.y * v.y));
	return vector2_base<float>(v.x * l, v.y * l);
}

inline vector2_base<float> direction(float angle)
{
	return vector2_base<float>(cosf(angle), sinf(angle));
}

typedef vector2_base<float> vec2;
typedef vector2_base<bool> bvec2;
typedef vector2_base<int> ivec2;

template<typename T>
inline bool closest_point_on_line(vector2_base<T> line_point0, vector2_base<T> line_point1, vector2_base<T> target_point, vector2_base<T> &out_pos)
{
	vector2_base<T> c = target_point - line_point0;
	vector2_base<T> v = (line_point1 - line_point0);
	T d = length(line_point0 - line_point1);
	if(d > 0)
	{
		v = normalize_pre_length<T>(v, d);
		T t = dot(v, c) / d;
		out_pos = mix(line_point0, line_point1, clamp(t, (T)0, (T)1));
		return true;
	}
	else
		return false;
}

// ------------------------------------
template<typename T>
class vector3_base
{
public:
	union
	{
		T x, r, h;
	};
	union
	{
		T y, g, s;
	};
	union
	{
		T z, b, v, l;
	};

	vector3_base() = default;
	vector3_base(T nx, T ny, T nz)
	{
		x = nx;
		y = ny;
		z = nz;
	}

	vector3_base operator-(const vector3_base &v) const { return vector3_base(x - v.x, y - v.y, z - v.z); }
	vector3_base operator-() const { return vector3_base(-x, -y, -z); }
	vector3_base operator+(const vector3_base &v) const { return vector3_base(x + v.x, y + v.y, z + v.z); }
	vector3_base operator*(const T v) const { return vector3_base(x * v, y * v, z * v); }
	vector3_base operator*(const vector3_base &v) const { return vector3_base(x * v.x, y * v.y, z * v.z); }
	vector3_base operator/(const T v) const { return vector3_base(x / v, y / v, z / v); }
	vector3_base operator/(const vector3_base &v) const { return vector3_base(x / v.x, y / v.y, z / v.z); }

	const vector3_base &operator+=(const vector3_base &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	const vector3_base &operator-=(const vector3_base &v)
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	const vector3_base &operator*=(const T v)
	{
		x *= v;
		y *= v;
		z *= v;
		return *this;
	}
	const vector3_base &operator*=(const vector3_base &v)
	{
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}
	const vector3_base &operator/=(const T v)
	{
		x /= v;
		y /= v;
		z /= v;
		return *this;
	}
	const vector3_base &operator/=(const vector3_base &v)
	{
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}

	bool operator==(const vector3_base &v) const { return x == v.x && y == v.y && z == v.z; } // TODO: do this with an eps instead
	bool operator!=(const vector3_base &v) const { return x != v.x || y != v.y || z != v.z; }
};

template<typename T>
inline T distance(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return length(a - b);
}

template<typename T>
inline T dot(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

template<typename T>
inline vector3_base<T> cross(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return vector3_base<T>(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

//
inline float length(const vector3_base<float> &a)
{
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

inline vector3_base<float> normalize(const vector3_base<float> &v)
{
	float l = (float)(1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z));
	return vector3_base<float>(v.x * l, v.y * l, v.z * l);
}

typedef vector3_base<float> vec3;
typedef vector3_base<bool> bvec3;
typedef vector3_base<int> ivec3;

// ------------------------------------

template<typename T>
class vector4_base
{
public:
	union
	{
		T x, r, h;
	};
	union
	{
		T y, g, s;
	};
	union
	{
		T z, b, l;
	};
	union
	{
		T w, a;
	};

	vector4_base() = default;
	vector4_base(T nx, T ny, T nz, T nw)
	{
		x = nx;
		y = ny;
		z = nz;
		w = nw;
	}

	vector4_base operator+(const vector4_base &v) const { return vector4_base(x + v.x, y + v.y, z + v.z, w + v.w); }
	vector4_base operator-(const vector4_base &v) const { return vector4_base(x - v.x, y - v.y, z - v.z, w - v.w); }
	vector4_base operator-() const { return vector4_base(-x, -y, -z, -w); }
	vector4_base operator*(const vector4_base &v) const { return vector4_base(x * v.x, y * v.y, z * v.z, w * v.w); }
	vector4_base operator*(const T v) const { return vector4_base(x * v, y * v, z * v, w * v); }
	vector4_base operator/(const vector4_base &v) const { return vector4_base(x / v.x, y / v.y, z / v.z, w / v.w); }
	vector4_base operator/(const T v) const { return vector4_base(x / v, y / v, z / v, w / v); }

	const vector4_base &operator+=(const vector4_base &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}
	const vector4_base &operator-=(const vector4_base &v)
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}
	const vector4_base &operator*=(const T v)
	{
		x *= v;
		y *= v;
		z *= v;
		w *= v;
		return *this;
	}
	const vector4_base &operator*=(const vector4_base &v)
	{
		x *= v.x;
		y *= v.y;
		z *= v.z;
		w *= v.w;
		return *this;
	}
	const vector4_base &operator/=(const T v)
	{
		x /= v;
		y /= v;
		z /= v;
		w /= v;
		return *this;
	}
	const vector4_base &operator/=(const vector4_base &v)
	{
		x /= v.x;
		y /= v.y;
		z /= v.z;
		w /= v.w;
		return *this;
	}

	bool operator==(const vector4_base &v) const { return x == v.x && y == v.y && z == v.z && w == v.w; } // TODO: do this with an eps instead
};

typedef vector4_base<float> vec4;
typedef vector4_base<bool> bvec4;
typedef vector4_base<int> ivec4;

#endif
