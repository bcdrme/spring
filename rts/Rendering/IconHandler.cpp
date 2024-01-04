/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "IconHandler.h"

#include <algorithm>
#include <cassert>
#include <locale>
#include <cctype>
#include <cmath>

#include "Rendering/GL/myGL.h"
#include "Rendering/GL/RenderBuffers.h"
#include "System/Log/ILog.h"
#include "Lua/LuaParser.h"
#include "Textures/Bitmap.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileSystem.h"

namespace icon {

CIconHandler iconHandler;

static CIconData dummyIconData[CIconHandler::ICON_DATA_OFFSET];


/******************************************************************************/
//
//  CIconHandler
//

void CIconHandler::Kill()
{
	glDeleteTextures(1, &defTexID);

	defTexID = 0;
	numIcons = 0;

	dummyIconData[ SAFETY_DATA_IDX] = {};
	dummyIconData[DEFAULT_DATA_IDX] = {};

	iconMap.clear();

	for (CIconData& id: iconData) {
		id = {};
	}
}


bool CIconHandler::LoadIcons(const std::string& filename)
{
	LuaParser luaParser(filename, SPRING_VFS_MOD_BASE, SPRING_VFS_MOD_BASE);

	if (!luaParser.Execute())
		LOG_L(L_WARNING, "%s: %s", filename.c_str(), luaParser.GetErrorLog().c_str());

	const LuaTable iconTypes = luaParser.GetRoot();

	std::vector<std::string> iconNames;
	iconTypes.GetKeys(iconNames);

	dummyIconData[ SAFETY_DATA_IDX] = {};
	//dummyIconData[DEFAULT_DATA_IDX] = {"default", GetDefaultTexture(), 1.0f, 1.0f, false, false, DEFAULT_TEX_SIZE_X, DEFAULT_TEX_SIZE_Y, 0.0f, 0.0f, 1.0f, 1.0f};
	dummyIconData[DEFAULT_DATA_IDX] = { "default", GetDefaultTexture(), 1.0f, 1.0f, false, false, 0.0f, 0.0f, 1.0f, 1.0f };


	for (const std::string& iconName : iconNames) {
		const LuaTable iconTable = iconTypes.SubTable(iconName);

		AddIcon(
			iconName,
			iconTable.GetString("bitmap",       ""),
			iconTable.GetFloat ("size",         1.0f),
			iconTable.GetFloat ("distance",     1.0f),
			iconTable.GetBool  ("radiusAdjust", false),
			iconTable.GetFloat ("u0",			0.0f),
			iconTable.GetFloat ("v0",			0.0f),
			iconTable.GetFloat ("u1",			1.0f),
			iconTable.GetFloat ("v1",			1.0f)
		);
	}

	const auto it = iconMap.find("default");

	if (it != iconMap.end()) {
		dummyIconData[DEFAULT_DATA_IDX].CopyData(GetIconData(it->second.dataIdx));
		dummyIconData[DEFAULT_DATA_IDX].SwapOwner(GetIconDataMut(it->second.dataIdx));
	} else {
		iconMap["default"] = CIcon(DEFAULT_DATA_IDX);
	}

	return true;
}


bool CIconHandler::AddIcon(
	const std::string& iconName,
	const std::string& texName,
	float size,
	float distance,
	bool radAdj,
	float u0, float v0, float u1, float v1
) {
	if (numIcons == iconData.size()) {
		LOG_L(L_DEBUG, "[IconHandler::%s] too many icons added (maximum=%u)", __func__, numIcons);
		return false;
	}

	unsigned int texID = 0;
	//unsigned int xsize = 0;
	//unsigned int ysize = 0;

	bool ownTexture = true;

	try {

		ownTexture = false;
		if (!texName.empty()) {
			const auto it = iconTexNameToTexID.find(texName);

			if (it != iconTexNameToTexID.end()){ // Assign it if we found it
				texID = it->second;
			}
			else {
				CBitmap bitmap;
				if (bitmap.Load(texName)) {
					if (FileSystem::GetExtension(texName) == "dds")
						texID = bitmap.CreateTexture();
					else
						texID = bitmap.CreateMipMapTexture();

					iconTexNameToTexID[texName] = texID;

					glBindTexture(GL_TEXTURE_2D, texID);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				else {
					// failed to load it, 
					texID = GetDefaultTexture();
				}
			}
		}
		else {
			texID = GetDefaultTexture();
		}
	} catch (const content_error& ex) {
		// bail on non-existant file
		LOG_L(L_DEBUG, "[IconHandler::%s] exception \"%s\" adding icon \"%s\" with texture \"%s\"", __func__, ex.what(), iconName.c_str(), texName.c_str());
		return false;
	}

	const auto it = iconMap.find(iconName);

	if (it != iconMap.end())
		FreeIcon(iconName);

	// data must be constructed first since CIcon's ctor will Ref() it
	//iconData[numIcons] = { iconName, texID,  size, distance, radAdj, ownTexture, xsize, ysize, u0, v0, u1, v1 };
	iconData[numIcons] = {iconName, texID,  size, distance, radAdj, ownTexture, u0, v0, u1, v1};
	// indices 0 and 1 are reserved
	iconMap[iconName] = CIcon(ICON_DATA_OFFSET + numIcons++);

	if (iconName == "default") {
		dummyIconData[DEFAULT_DATA_IDX].CopyData(&iconData[numIcons - 1]);
		dummyIconData[DEFAULT_DATA_IDX].SwapOwner(&iconData[numIcons - 1]);
	}

	return true;
}


bool CIconHandler::FreeIcon(const std::string& iconName)
{
	const auto it = iconMap.find(iconName);

	if (it == iconMap.end())
		return false;
	if (iconName == "default")
		return false;

	// fill with default data (TODO: reuse freed slots)
	GetIconDataMut(it->second.dataIdx)->CopyData(&dummyIconData[DEFAULT_DATA_IDX]);

	iconMap.erase(iconName);
	return true;
}


CIcon CIconHandler::GetIcon(const std::string& iconName) const
{
	const auto it = iconMap.find(iconName);

	if (it == iconMap.end())
		return GetDefaultIcon();

	return it->second;
}

const CIconData* CIconHandler::GetSafetyIconData() { return &dummyIconData[SAFETY_DATA_IDX]; }
const CIconData* CIconHandler::GetDefaultIconData() { return &dummyIconData[DEFAULT_DATA_IDX]; }


unsigned int CIconHandler::GetDefaultTexture()
{
	// FIXME: just use a PNG ?

	if (defTexID != 0)
		return defTexID;

	CBitmap defaultEngineBitmap;
	if (defaultEngineBitmap.Load("bitmaps/defaultradardot.png")) {
		glBindTexture(GL_TEXTURE_2D, defTexID = defaultEngineBitmap.CreateTexture());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		return defTexID;
	}

	unsigned char si[DEFAULT_TEX_SIZE_X * DEFAULT_TEX_SIZE_Y * 4];
	for (int y = 0; y < DEFAULT_TEX_SIZE_Y; ++y) {
		for (int x = 0; x < DEFAULT_TEX_SIZE_X; ++x) {
			const int index = ((y * DEFAULT_TEX_SIZE_X) + x) * 4;
			const int dx = (x - 64);
			const int dy = (y - 64);
			const float r = std::sqrt((dx * dx) + (dy * dy)) / 64.0f;
			if (r > 1.0f) {
				si[index + 0] = 0;
				si[index + 1] = 0;
				si[index + 2] = 0;
				si[index + 3] = 0;
			} else {
				const unsigned char val = (unsigned char)(255 - (r * r * r * 255));
				si[index + 0] = val;
				si[index + 1] = val;
				si[index + 2] = val;
				si[index + 3] = 255;
			}
		}
	}

	CBitmap bitmap(si, DEFAULT_TEX_SIZE_X, DEFAULT_TEX_SIZE_Y);

	glBindTexture(GL_TEXTURE_2D, defTexID = bitmap.CreateTexture());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return defTexID;
}



/******************************************************************************/
//
//  CIcon
//

CIcon::CIcon()
{
	CIconData* data = nullptr;

	// use default data if handler is initialized
	if (iconHandler.defTexID != 0) {
		data = &dummyIconData[dataIdx = CIconHandler::DEFAULT_DATA_IDX];
	} else {
		data = &dummyIconData[dataIdx = CIconHandler::SAFETY_DATA_IDX];
	}

	data->Ref();
}


CIcon::CIcon(unsigned int idx)
{
	iconHandler.GetIconDataMut(dataIdx = idx)->Ref();
}

CIcon::CIcon(const CIcon& icon)
{
	iconHandler.GetIconDataMut(dataIdx = icon.dataIdx)->Ref();
}

CIcon::~CIcon()
{
	// NB: icons can outlive the handler
	UnRefData(&iconHandler);
}


CIcon& CIcon::operator=(const CIcon& icon)
{
	if (dataIdx != icon.dataIdx) {
		CIconData* iconData = iconHandler.GetIconDataMut(dataIdx);

		iconData->UnRef();
		iconData = iconHandler.GetIconDataMut(dataIdx = icon.dataIdx);
		iconData->Ref();
	}

	return *this;
}


void CIcon::UnRefData(CIconHandler* ih) {
	if (ih != nullptr)
		ih->GetIconDataMut(dataIdx)->UnRef();

	dataIdx = CIconHandler::SAFETY_DATA_IDX;
}

const CIconData* CIcon::operator->()  const { return iconHandler.GetIconData(dataIdx); }
const CIconData* CIcon::GetIconData() const { return iconHandler.GetIconData(dataIdx); }






/******************************************************************************/
//
//  CIconData
//

CIconData::CIconData(
	const std::string& _name,
	unsigned int _texID,
	float _size,
	float _distance,
	bool radAdj,
	bool ownTex,
	//unsigned int _xsize,
	//unsigned int _ysize,
	float _u0, float _v0, float _u1, float _v1
)
	: name(_name)
	, refCount(0)

	, texID(_texID)
	//, xsize(_xsize)
	//, ysize(_ysize)

	, size(_size)
	, distance(_distance)
	, distSqr(distance * distance)

	, ownTexture(ownTex)
	, radiusAdjust(radAdj)

	, u0(_u0)
	, v0(_v0)
	, u1(_u1)
	, v1(_v1)
{
}

CIconData::~CIconData()
{
	if (ownTexture) {
		#ifndef HEADLESS
		assert(texID != 0);
		#endif
		glDeleteTextures(1, &texID);
	}

	texID = 0;
}


void CIconData::CopyData(const CIconData* iconData)
{
	name         = iconData->name;
	texID        = iconData->texID;
	size         = iconData->size;
	distance     = iconData->distance;
	distSqr      = iconData->distSqr;
	radiusAdjust = iconData->radiusAdjust;
	//xsize        = iconData->xsize;
	//ysize        = iconData->ysize;
	u0			 = iconData->u0;
	v0			 = iconData->v0;
	u1			 = iconData->u1;
	v1			 = iconData->v1;
	ownTexture   = false;
}

void CIconData::BindTexture() const
{
	glBindTexture(GL_TEXTURE_2D, texID);
}


void CIconData::Draw(float x0, float y0, float x1, float y1) const
{
	glBindTexture(GL_TEXTURE_2D, texID);
	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0); glVertex2f(x0, y0);
	glTexCoord2f(u1, v0); glVertex2f(x1, y0);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u0, v1); glVertex2f(x0, y1);
	glEnd();
}

}

