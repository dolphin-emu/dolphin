#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <string>

struct Config 
{
	Config();
	void Load();
	void Save();

	int iAdapter;
	int iFSResolution;
	int iMultisampleMode;

	int iPostprocessEffect;
	int iCompileDLsLevel;

	bool renderInMainframe;
	bool bFullscreen;
	bool bVsync;
	bool bWireFrame;
	bool bOverlayStats;
	bool bDumpTextures;
	bool bOldCard;
	bool bShowShaderErrors;
	//enhancements
	bool bForceFiltering;
	bool bForceMaxAniso;

	bool bPreUpscale;
	int iPreUpscaleFilter;

	bool bTruform;
	int iTruformLevel;

	int iWindowedRes;

	char psProfile[16];
	char vsProfile[16];

	std::string texDumpPath;
};


extern Config g_Config;


struct Statistics
{
	int numPrimitives;

	int numPixelShadersCreated;
	int numPixelShadersAlive;
	int numVertexShadersCreated;
	int numVertexShadersAlive;

	int numTexturesCreated;
	int numTexturesAlive;

	int numRenderTargetsCreated;
	int numRenderTargetsAlive;
	
	int numDListsCalled;
	int numDListsCreated;
	int numDListsAlive;

	int numJoins;

	struct ThisFrame
	{
		int numBPLoads;
		int numCPLoads;
		int numXFLoads;
		
		int numBPLoadsInDL;
		int numCPLoadsInDL;
		int numXFLoadsInDL;
		
		int numDLs;
		int numDLPrims;
		int numPrims;
		int numShaderChanges;
		int numBadCommands; //hope this always is zero ;)
	};
	ThisFrame thisFrame;
	void ResetFrame() {memset(&thisFrame,0,sizeof(ThisFrame));}
};

extern Statistics stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a) (a)++;
#define ADDSTAT(a,b) (a)+=(b);
#define SETSTAT(a,x) (a)=(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a,b) ;
#define SETSTAT(a,x) ;
#endif

#endif