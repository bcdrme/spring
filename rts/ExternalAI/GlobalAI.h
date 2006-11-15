#ifndef GLOBALAI_H
#define GLOBALAI_H

#include "Object.h"
#include "Platform/SharedLib.h"

class IGlobalAI;
class CGlobalAICallback;
class CGroupHandler;

class CGlobalAI :
	public CObject
{
public:
	CGlobalAI(int team, const char* dll);
	~CGlobalAI(void);

	void Update(void);
	void PreDestroy (); // called just before all the units are destroyed

	int team;
	bool cheatevents;

	bool IsCInterface;

	IGlobalAI* ai;
	CGlobalAICallback* callback;
	CGroupHandler* gh;

	SharedLib *lib;

	typedef bool (* ISCINTERFACE)();
	typedef int (* GETGLOBALAIVERSION)();
	typedef IGlobalAI* (* GETNEWAI)();
	typedef void (* RELEASEAI)(IGlobalAI* i);
	
	ISCINTERFACE _IsCInterface;
	GETGLOBALAIVERSION GetGlobalAiVersion;
	GETNEWAI GetNewAI;
	RELEASEAI ReleaseAI;
};

#endif
