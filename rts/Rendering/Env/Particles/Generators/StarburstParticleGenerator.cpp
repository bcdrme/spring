#include "StarburstParticleGenerator.h"

CR_BIND(StarburstParticleData, )
CR_REG_METADATA(StarburstParticleData,
(
	CR_MEMBER(partPos),
	CR_MEMBER(missileAge),
	CR_MEMBER(partSpeed),
	CR_MEMBER(curTracerPart),
	CR_MEMBER(drawOrder),
	CR_IGNORED(unused),
	CR_MEMBER(tracerPosSpeed),
	CR_MEMBER(tracerDirNumMods),
	CR_MEMBER(allAgeMods),
	CR_MEMBER(texCoord1),
	CR_MEMBER(texCoord3)
))