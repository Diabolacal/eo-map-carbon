// Host-side orbit camera. TrinityAL has no camera or matrix type.
// Layout matches XMMatrixLookAtRH / XMMatrixPerspectiveFovRH (row-vector, row-major).

#pragma once

#include <cmath>

namespace orbit
{
constexpr float kPi = 3.14159265358979323846f;

struct Vec3
{
	float x;
	float y;
	float z;
};

inline float Dot(Vec3 a, Vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(Vec3 a, Vec3 b)
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vec3 Sub(Vec3 a, Vec3 b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vec3 Normalize(Vec3 v)
{
	const float len = sqrtf(Dot(v, v));
	if (len <= 1e-8f)
	{
		return { 0.0f, 0.0f, 0.0f };
	}
	return { v.x / len, v.y / len, v.z / len };
}

struct Mat4
{
	float m[4][4];
};

inline Mat4 Mul(const Mat4& a, const Mat4& b)
{
	Mat4 r = {};
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
		}
	}
	return r;
}

inline Mat4 LookAtRH(Vec3 eye, Vec3 at, Vec3 up)
{
	const Vec3 z = Normalize(Sub(eye, at));
	const Vec3 x = Normalize(Cross(up, z));
	const Vec3 y = Cross(z, x);
	Mat4 r = {};
	r.m[0][0] = x.x;
	r.m[0][1] = y.x;
	r.m[0][2] = z.x;
	r.m[1][0] = x.y;
	r.m[1][1] = y.y;
	r.m[1][2] = z.y;
	r.m[2][0] = x.z;
	r.m[2][1] = y.z;
	r.m[2][2] = z.z;
	r.m[3][0] = -Dot(x, eye);
	r.m[3][1] = -Dot(y, eye);
	r.m[3][2] = -Dot(z, eye);
	r.m[3][3] = 1.0f;
	return r;
}

inline Mat4 PerspectiveFovRH(float fovY, float aspect, float zn, float zf)
{
	const float h = 1.0f / tanf(fovY * 0.5f);
	const float w = h / aspect;
	Mat4 r = {};
	r.m[0][0] = w;
	r.m[1][1] = h;
	r.m[2][2] = zf / (zn - zf);
	r.m[2][3] = -1.0f;
	r.m[3][2] = (zn * zf) / (zn - zf);
	return r;
}

struct Camera
{
	float yaw = 0.55f;
	float pitch = 0.38f;
	float distance = 145.0f;
	float fovY = 60.0f * kPi / 180.0f;
	float nearZ = 0.4f;
	float farZ = 2500.0f;

	void Orbit(float dxPixels, float dyPixels)
	{
		const float sens = 0.005f;
		yaw += dxPixels * sens;
		pitch += dyPixels * sens;
		const float limit = 1.35f;
		if (pitch > limit)
		{
			pitch = limit;
		}
		if (pitch < -limit)
		{
			pitch = -limit;
		}
	}

	void Zoom(int wheelDelta)
	{
		const float clicks = float(wheelDelta) / 120.0f;
		distance *= powf(0.88f, clicks);
		if (distance < 12.0f)
		{
			distance = 12.0f;
		}
		if (distance > 520.0f)
		{
			distance = 520.0f;
		}
	}

	Vec3 Eye() const
	{
		const float cp = cosf(pitch);
		const float sp = sinf(pitch);
		const float cy = cosf(yaw);
		const float sy = sinf(yaw);
		return { distance * cp * sy, distance * sp, distance * cp * cy };
	}

	Mat4 ViewProjection(float aspect) const
	{
		const Vec3 eye = Eye();
		const Vec3 at = { 0.0f, 0.0f, 0.0f };
		const Vec3 up = { 0.0f, 1.0f, 0.0f };
		return Mul(LookAtRH(eye, at, up), PerspectiveFovRH(fovY, aspect, nearZ, farZ));
	}
};
}
