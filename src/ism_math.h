#pragma once

// CPU contract for the Creator Mode interstellar medium.
// The HLSL in IsmField.psh must stay in lockstep with these formulas.
// Used by smoke so a spatial regression can fail without looking at pixels.

#include "new_eden_anchors.h"
#include "orbit_camera.h"
#include "tune_params.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace ism
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kFloor = 0.20f;
constexpr float kFlatten = 4.2f;
constexpr float kClumpScale = 0.48f;
constexpr float kBridge = 0.22f;
constexpr float kIrrAmp = 0.10f;
constexpr float kEnvEarlyOut = 0.002f;
constexpr float kNearFloorLy = 4.0f;
constexpr float kFarClipSafety = 1.12f;
constexpr float kIrrExtra = 1.10f;
constexpr float kTauScale = 0.022f;
constexpr float kTauMax = 2.60f;
constexpr float kGlowGain = 0.045f;
constexpr float kFill = 0.32f;
constexpr float kEmitScale = 0.085f;
constexpr float kSlabThickMul = 5.0f;
constexpr float kSlabPad = 5.0f;

// EO-Map fallback lobe centres (scene LY). Same space as kCentre / stars.
constexpr float kLobePos[3][3] = {
	{ -4.013f, -3.629f, -5.550f },
	{ -15.511f, -7.462f, 5.948f },
	{ -23.176f, -3.629f, 2.115f },
};
constexpr float kLobeW[3] = { 1.00f, 0.71f, 0.67f };

// data/new_eden_systems.manifest.json scene_aabb after (x, -z, -y).
constexpr float kCloudMin[3] = { -53.7784309f, -15.3927479f, -49.9852104f };
constexpr float kCloudMax[3] = { 35.5732536f, 7.39825964f, 51.2106590f };
constexpr float kCloudCentroid[3] = { -9.776f, -4.427f, 1.090f };

inline float Clamp01(float v)
{
	if (v < 0.0f)
	{
		return 0.0f;
	}
	if (v > 1.0f)
	{
		return 1.0f;
	}
	return v;
}

inline float Saturate(float v)
{
	return Clamp01(v);
}

inline float Smoother01(float t)
{
	const float x = Clamp01(t);
	return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

inline int QualitySteps(float steps)
{
	if (steps >= 40.0f)
	{
		return 48;
	}
	if (steps >= 28.0f)
	{
		return 32;
	}
	if (steps >= 20.0f)
	{
		return 24;
	}
	if (steps >= 12.0f)
	{
		return 16;
	}
	return 8;
}

inline float TaperStartMul(float edgeSoft)
{
	return 1.35f + 0.65f * TuneClamp(edgeSoft, 0.40f, 2.50f);
}

inline float TaperEndMul(float edgeSoft)
{
	return 1.90f + 1.20f * TuneClamp(edgeSoft, 0.40f, 2.50f);
}

inline float VisibleRadius(float radius, float edgeSoft)
{
	return (std::max)(radius, 1.0f) * TaperEndMul(edgeSoft) * (1.0f + kIrrAmp);
}

inline float FarClipRadius(float radius, float edgeSoft)
{
	return VisibleRadius(radius, edgeSoft) * kIrrExtra * kFarClipSafety;
}

struct Volume
{
	float cx = neweden::kCentreX;
	float cy = neweden::kCentreY;
	float cz = neweden::kCentreZ;
	float radius = 32.0f;
	float thickness = 8.0f;
	float edgeSoft = 1.40f;
	float lobe = 0.70f;
	float floorAmt = kFloor;
};

inline Volume VolumeFromTune(const TuneParams& t)
{
	Volume v;
	v.radius = (std::max)(t.ismRadius, 1.0f);
	v.thickness = (std::max)(t.ismThickness, 0.4f);
	v.edgeSoft = t.ismEdgeSoft;
	v.lobe = (std::max)(t.ismLobe, 0.0f);
	return v;
}

inline float OuterIrregularity(float dx, float dz)
{
	const float ang = atan2f(dz, dx);
	return 0.50f * sinf(2.0f * ang + 0.41f) + 0.32f * sinf(3.0f * ang - 1.17f) + 0.18f * sinf(5.0f * ang + 2.03f);
}

inline float OuterTaper(float dx, float dz, const Volume& v)
{
	const float r = sqrtf(dx * dx + dz * dz);
	const float sigma = (std::max)(v.radius, 1.0e-6f);
	const float startMul = TaperStartMul(v.edgeSoft);
	const float endMul = TaperEndMul(v.edgeSoft);
	const float rMax = sigma * endMul * (1.0f + kIrrAmp);
	if (r >= rMax)
	{
		return 0.0f;
	}
	const float scale = 1.0f + kIrrAmp * OuterIrregularity(dx, dz);
	const float rEff = r / (std::max)(scale, 1.0e-3f);
	const float a = sigma * startMul;
	const float b = sigma * endMul;
	return 1.0f - Smoother01((rEff - a) / (std::max)(b - a, 1.0e-3f));
}

inline float Bridge(float px, float py, float pz, float ax, float ay, float az, float bx, float by, float bz, float w)
{
	const float pax = px - ax;
	const float pay = py - ay;
	const float paz = pz - az;
	const float bax = bx - ax;
	const float bay = by - ay;
	const float baz = bz - az;
	const float ba2 = bax * bax + bay * bay + baz * baz;
	float s = (ba2 > 1.0e-4f) ? ((pax * bax + pay * bay + paz * baz) / ba2) : 0.0f;
	s = Clamp01(s);
	const float dx = pax - bax * s;
	const float dy = pay - bay * s;
	const float dz = paz - baz * s;
	const float d2 = dx * dx + dy * dy + dz * dz;
	const float taper = 1.0f + (0.55f - 1.0f) * sinf(kPi * s);
	const float denom = (std::max)(w * w * taper * taper, 1.0e-4f);
	return expf(-d2 / denom);
}

inline float EnvelopeAt(float x, float y, float z, const Volume& v)
{
	const float dx = x - v.cx;
	const float dy = y - v.cy;
	const float dz = z - v.cz;
	const float r = sqrtf(dx * dx + dz * dz);
	const float sigma = (std::max)(v.radius, 1.0f);
	const float rMax = sigma * TaperEndMul(v.edgeSoft) * (1.0f + kIrrAmp);
	if (r >= rMax)
	{
		return 0.0f;
	}

	const float hz = v.thickness * (1.0f + r / sigma);
	const float disc = expf(-(r * r) / (2.0f * sigma * sigma)) * expf(-(dy * dy) / (2.0f * hz * hz));

	const float clumpR = (std::max)(sigma * kClumpScale, 1.0f);
	float lobes = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		float qx = x - kLobePos[i][0];
		float qy = (y - kLobePos[i][1]) * kFlatten;
		float qz = z - kLobePos[i][2];
		lobes += kLobeW[i] * expf(-(qx * qx + qy * qy + qz * qz) / (clumpR * clumpR));
	}

	float bridges = 0.0f;
	if (kBridge > 1.0e-3f)
	{
		const float bw = clumpR * 0.45f;
		const float px = x;
		const float py = y * kFlatten;
		const float pz = z;
		const float a0x = kLobePos[0][0];
		const float a0y = kLobePos[0][1] * kFlatten;
		const float a0z = kLobePos[0][2];
		const float a1x = kLobePos[1][0];
		const float a1y = kLobePos[1][1] * kFlatten;
		const float a1z = kLobePos[1][2];
		const float a2x = kLobePos[2][0];
		const float a2y = kLobePos[2][1] * kFlatten;
		const float a2z = kLobePos[2][2];
		bridges += Bridge(px, py, pz, a0x, a0y, a0z, a1x, a1y, a1z, bw);
		bridges += Bridge(px, py, pz, a1x, a1y, a1z, a2x, a2y, a2z, bw);
		bridges += Bridge(px, py, pz, a0x, a0y, a0z, a2x, a2y, a2z, bw);
	}

	float env = disc * (v.floorAmt + v.lobe * lobes + kBridge * bridges);
	const float innerFull = sigma * TaperStartMul(v.edgeSoft) * (1.0f - kIrrAmp);
	if (r > innerFull)
	{
		env *= OuterTaper(dx, dz, v);
	}
	return Saturate(env);
}

inline bool IntersectSphere(float rox, float roy, float roz, float rdx, float rdy, float rdz, float cx, float cy, float cz, float radius, float& tEnter, float& tExit)
{
	const float ocx = rox - cx;
	const float ocy = roy - cy;
	const float ocz = roz - cz;
	const float b = ocx * rdx + ocy * rdy + ocz * rdz;
	const float cc = ocx * ocx + ocy * ocy + ocz * ocz - radius * radius;
	const float h = b * b - cc;
	if (h < 0.0f)
	{
		tEnter = 0.0f;
		tExit = 0.0f;
		return false;
	}
	const float s = sqrtf(h);
	tEnter = -b - s;
	tExit = -b + s;
	return tExit > 0.0f;
}

inline bool IntersectSlabY(float roy, float rdy, float y0, float y1, float& tEnter, float& tExit)
{
	if (y1 < y0)
	{
		const float tmp = y0;
		y0 = y1;
		y1 = tmp;
	}
	if (fabsf(rdy) < 1.0e-5f)
	{
		if (roy < y0 || roy > y1)
		{
			tEnter = 0.0f;
			tExit = 0.0f;
			return false;
		}
		tEnter = 0.0f;
		tExit = 1.0e6f;
		return true;
	}
	const float t0 = (y0 - roy) / rdy;
	const float t1 = (y1 - roy) / rdy;
	tEnter = (std::min)(t0, t1);
	tExit = (std::max)(t0, t1);
	return tExit > (std::max)(tEnter, 0.0f);
}

struct MarchInterval
{
	bool hit = false;
	float tStart = 0.0f;
	float tEnd = 0.0f;
};

inline MarchInterval DiscMarchWindow(
	float rox,
	float roy,
	float roz,
	float rdx,
	float rdy,
	float rdz,
	const Volume& v,
	float camDist,
	float nearCut)
{
	MarchInterval out;
	const float farR = FarClipRadius(v.radius, v.edgeSoft);
	float sph0 = 0.0f;
	float sph1 = 0.0f;
	if (!IntersectSphere(rox, roy, roz, rdx, rdy, rdz, v.cx, v.cy, v.cz, farR, sph0, sph1))
	{
		return out;
	}

	const float yHalf = v.thickness * kSlabThickMul + kSlabPad;
	float slab0 = 0.0f;
	float slab1 = 0.0f;
	if (!IntersectSlabY(roy, rdy, v.cy - yHalf, v.cy + yHalf, slab0, slab1))
	{
		return out;
	}

	const float t0 = (std::max)(kNearFloorLy, Saturate(nearCut) * (std::max)(camDist, 1.0f));
	const float tStart = (std::max)((std::max)((std::max)(sph0, 0.0f), slab0), t0 * 0.30f);
	const float tEnd = (std::min)(sph1, slab1);
	if (tEnd <= tStart)
	{
		return out;
	}
	out.hit = true;
	out.tStart = tStart;
	out.tEnd = tEnd;
	return out;
}

inline float MarchMaxEnvelope(
	float rox,
	float roy,
	float roz,
	float rdx,
	float rdy,
	float rdz,
	const Volume& v,
	float camDist,
	float nearCut,
	int steps)
{
	const MarchInterval win = DiscMarchWindow(rox, roy, roz, rdx, rdy, rdz, v, camDist, nearCut);
	if (!win.hit)
	{
		return 0.0f;
	}
	const int n = (std::max)(steps, 1);
	const float dt = (win.tEnd - win.tStart) / float(n);
	float best = 0.0f;
	for (int i = 0; i < n; ++i)
	{
		const float t = win.tStart + (float(i) + 0.5f) * dt;
		const float env = EnvelopeAt(rox + rdx * t, roy + rdy * t, roz + rdz * t, v);
		if (env > best)
		{
			best = env;
		}
	}
	return best;
}

inline orbit::Camera DefaultNewEdenCamera()
{
	orbit::Camera cam;
	cam.target = { neweden::kCentreX, neweden::kCentreY, neweden::kCentreZ };
	cam.yaw = 0.35f;
	cam.pitch = 0.62f;
	cam.distance = 190.0f;
	cam.nearZ = 0.2f;
	cam.farZ = 2500.0f;
	return cam;
}

inline void DefaultViewBasis(orbit::Vec3& eye, orbit::Vec3& right, orbit::Vec3& up, orbit::Vec3& fwd)
{
	const orbit::Camera cam = DefaultNewEdenCamera();
	eye = cam.Eye();
	const orbit::Vec3 back = orbit::Normalize(orbit::Sub(eye, cam.target));
	const orbit::Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	right = orbit::Normalize(orbit::Cross(worldUp, back));
	up = orbit::Cross(back, right);
	fwd = { -back.x, -back.y, -back.z };
}

inline orbit::Vec3 RayDirection(const orbit::Vec3& right, const orbit::Vec3& up, const orbit::Vec3& fwd, float ndcX, float ndcY, float aspect, float tanHalf)
{
	return orbit::Normalize({
		fwd.x + right.x * (ndcX * tanHalf * aspect) + up.x * (ndcY * tanHalf),
		fwd.y + right.y * (ndcX * tanHalf * aspect) + up.y * (ndcY * tanHalf),
		fwd.z + right.z * (ndcX * tanHalf * aspect) + up.z * (ndcY * tanHalf),
	});
}

inline bool PointInAabb(float x, float y, float z)
{
	return x >= kCloudMin[0] && x <= kCloudMax[0] && y >= kCloudMin[1] && y <= kCloudMax[1] && z >= kCloudMin[2] &&
		z <= kCloudMax[2];
}

inline bool ValidateIsmMath(std::string& error, char* logBuf, size_t logBufSize)
{
	const TuneParams defaults = TuneDefaults();
	const Volume vol = VolumeFromTune(defaults);

	if (!PointInAabb(vol.cx, vol.cy, vol.cz))
	{
		error = "ISM centre is outside the New Eden scene AABB";
		return false;
	}
	const float toCentroid = sqrtf(
		(vol.cx - kCloudCentroid[0]) * (vol.cx - kCloudCentroid[0]) +
		(vol.cy - kCloudCentroid[1]) * (vol.cy - kCloudCentroid[1]) +
		(vol.cz - kCloudCentroid[2]) * (vol.cz - kCloudCentroid[2]));
	if (toCentroid > 2.0f)
	{
		error = "ISM centre does not overlap the New Eden centroid";
		return false;
	}

	const float centreEnv = EnvelopeAt(vol.cx, vol.cy, vol.cz, vol);
	if (centreEnv < 0.35f)
	{
		error = "ISM envelope at the volume centre is too weak";
		return false;
	}

	const float farEnv = EnvelopeAt(vol.cx + 220.0f, vol.cy, vol.cz, vol);
	if (farEnv > 1.0e-4f)
	{
		error = "ISM envelope far outside the radial extent is not ~0";
		return false;
	}

	const float aboveEnv = EnvelopeAt(vol.cx, vol.cy + vol.thickness * 5.0f, vol.cz, vol);
	if (aboveEnv > 0.02f)
	{
		error = "ISM envelope well above the disc thickness is not ~0";
		return false;
	}

	Volume hard = vol;
	hard.edgeSoft = 0.40f;
	Volume mid = vol;
	mid.edgeSoft = 1.40f;
	Volume soft = vol;
	soft.edgeSoft = 2.40f;
	const float probeR = vol.radius * 2.50f;
	const float eHard = EnvelopeAt(vol.cx + probeR, vol.cy, vol.cz, hard);
	const float eMid = EnvelopeAt(vol.cx + probeR, vol.cy, vol.cz, mid);
	const float eSoft = EnvelopeAt(vol.cx + probeR, vol.cy, vol.cz, soft);
	if (!(eHard <= eMid + 1.0e-4f && eMid <= eSoft + 1.0e-4f))
	{
		error = "ISM edge softness is not monotonic";
		return false;
	}

	orbit::Vec3 eye, right, up, fwd;
	DefaultViewBasis(eye, right, up, fwd);
	const orbit::Camera cam = DefaultNewEdenCamera();
	const float aspect = 1280.0f / 720.0f;
	const float tanHalf = tanf(cam.fovY * 0.5f);
	const int steps = QualitySteps(defaults.ismSteps);
	const float centreHit = MarchMaxEnvelope(
		eye.x,
		eye.y,
		eye.z,
		fwd.x,
		fwd.y,
		fwd.z,
		vol,
		cam.distance,
		defaults.ismNearCut,
		steps);
	if (centreHit < 0.12f)
	{
		error = "default camera centre ray misses the ISM volume";
		return false;
	}

	const MarchInterval centreWin = DiscMarchWindow(
		eye.x,
		eye.y,
		eye.z,
		fwd.x,
		fwd.y,
		fwd.z,
		vol,
		cam.distance,
		defaults.ismNearCut);
	float centreTau = 0.0f;
	if (centreWin.hit)
	{
		const float dt = (centreWin.tEnd - centreWin.tStart) / float(steps);
		for (int i = 0; i < steps; ++i)
		{
			const float t = centreWin.tStart + (float(i) + 0.5f) * dt;
			const float env = EnvelopeAt(eye.x + fwd.x * t, eye.y + fwd.y * t, eye.z + fwd.z * t, vol);
			centreTau += defaults.ismDensity * env * dt * kTauScale;
		}
	}
	if (centreTau < 0.20f)
	{
		error = "default camera centre ray optical depth is too low";
		return false;
	}

	const int gridW = 24;
	const int gridH = 14;
	int hits = 0;
	int total = 0;
	for (int j = 0; j < gridH; ++j)
	{
		for (int i = 0; i < gridW; ++i)
		{
			const float u = (float(i) + 0.5f) / float(gridW);
			const float v = (float(j) + 0.5f) / float(gridH);
			const float ndcX = u * 2.0f - 1.0f;
			const float ndcY = 1.0f - v * 2.0f;
			const orbit::Vec3 rd = RayDirection(right, up, fwd, ndcX, ndcY, aspect, tanHalf);
			++total;
			if (MarchMaxEnvelope(
					eye.x,
					eye.y,
					eye.z,
					rd.x,
					rd.y,
					rd.z,
					vol,
					cam.distance,
					defaults.ismNearCut,
					QualitySteps(defaults.ismSteps)) >= kEnvEarlyOut)
			{
				++hits;
			}
		}
	}
	const float hitFrac = float(hits) / float(total);
	if (hitFrac < 0.28f)
	{
		error = "default camera does not send a substantial fraction of rays through the ISM";
		return false;
	}

	if (QualitySteps(8.0f) != 8 || QualitySteps(16.0f) != 16 || QualitySteps(24.0f) != 24 ||
		QualitySteps(32.0f) != 32 || QualitySteps(48.0f) != 48)
	{
		error = "ISM quality step buckets are wrong";
		return false;
	}

	if (logBuf && logBufSize > 0)
	{
		std::snprintf(
			logBuf,
			logBufSize,
			"ism math: centre_env=%.3f far=%.5f above=%.5f edge=%.3f/%.3f/%.3f default_ray=%.3f "
			"centre_tau=%.3f screen_hits=%d/%d (%.1f%%) radius=%.1f thick=%.1f far_clip=%.1f centre=(%.2f,%.2f,%.2f)\n",
			centreEnv,
			farEnv,
			aboveEnv,
			eHard,
			eMid,
			eSoft,
			centreHit,
			centreTau,
			hits,
			total,
			hitFrac * 100.0f,
			vol.radius,
			vol.thickness,
			FarClipRadius(vol.radius, vol.edgeSoft),
			vol.cx,
			vol.cy,
			vol.cz);
	}
	return true;
}
}
