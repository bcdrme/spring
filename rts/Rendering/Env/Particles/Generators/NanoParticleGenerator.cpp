#include "NanoParticleGenerator.h"

CR_BIND(NanoParticleData, )
CR_REG_METADATA(NanoParticleData,
(
	CR_MEMBER(partPos),
	CR_MEMBER(createFrame),
	CR_MEMBER(partSpeed),
	CR_MEMBER(color),
	CR_MEMBER(animParams),
	CR_MEMBER(partSize),
	CR_MEMBER(rotParams),
	CR_MEMBER(drawOrder),
	CR_MEMBER(texCoord)
))