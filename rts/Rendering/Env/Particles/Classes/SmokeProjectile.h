/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SMOKE_PROJECTILE_H
#define SMOKE_PROJECTILE_H

#include "Sim/Projectiles/Projectile.h"
#include "System/float3.h"

class CUnit;

class CSmokeProjectile : public CProjectile
{
	CR_DECLARE_DERIVED(CSmokeProjectile)

public:
	CSmokeProjectile();
	CSmokeProjectile(
		CUnit* owner,
		const float3& pos,
		const float3& speed,
		float ttl,
		float startSize,
		float sizeExpansion,
		float color
	);
	~CSmokeProjectile() override;

	void Update() override;
	void Draw() override;
	void Init(const CUnit* owner, const float3& offset) override;

	int GetProjectilesCount() const override;

	static bool GetMemberInfo(SExpGenSpawnableMemberInfo& memberInfo);

public:
	float size;
private:
	float color;
	float age;
	float ageSpeed;
	float startSize;
	float sizeExpansion;
	int textureNum;
	size_t pgOffset;
};

#endif /* SMOKE_PROJECTILE_H */
