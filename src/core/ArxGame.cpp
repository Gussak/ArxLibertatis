/*
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code').

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include "core/ArxGame.h"

#include <cstdio>

#include "ai/PathFinderManager.h"
#include "ai/Paths.h"

#include "animation/Animation.h"
#include "animation/Cinematic.h"

#include "core/Core.h"
#include "core/Config.h"
#include "core/Dialog.h"
#include "core/GameTime.h"
#include "core/Localisation.h"
#include "core/Resource.h"
#include "core/Win32Window.h"

#include "game/Inventory.h"
#include "game/Levels.h"
#include "game/Missile.h"
#include "game/NPC.h"
#include "game/Player.h"

#include "graphics/Draw.h"
#include "graphics/Frame.h"
#include "graphics/GraphicsEnum.h"
#include "graphics/GraphicsModes.h"
#include "graphics/GraphicsUtility.h"
#include "graphics/Math.h"
#include "graphics/VertexBuffer.h"
#include "graphics/data/Mesh.h"
#include "graphics/data/TextureContainer.h"
#include "graphics/direct3d/Direct3DRenderer.h"
#include "graphics/effects/Fog.h"
#include "graphics/font/Font.h"
#include "graphics/particle/ParticleEffects.h"
#include "graphics/particle/ParticleManager.h"
#include "graphics/texture/TextureStage.h"

#include "gui/Interface.h"
#include "gui/Menu.h"
#include "gui/MenuWidgets.h"
#include "gui/MiniMap.h"
#include "gui/Speech.h"
#include "gui/Text.h"
#include "gui/TextManager.h"

#include "input/Input.h"

#include "io/CinematicLoad.h"
#include "io/Logger.h"
#include "io/PakReader.h"
#include "io/Screenshot.h"

#include "scene/ChangeLevel.h"
#include "scene/Interactive.h"
#include "scene/GameSound.h"
#include "scene/Light.h"
#include "scene/LoadLevel.h"
#include "scene/Object.h"
#include "scene/Scene.h"

using std::string;

extern long ALLOW_CHEATS;
extern long FINAL_COMMERCIAL_GAME;
extern long FINAL_COMMERCIAL_DEMO;
extern long GAME_EDITOR;
extern long PLAY_LOADED_CINEMATIC;
extern char LAST_LAUNCHED_CINE[256];
extern long CHANGE_LEVEL_PROC_RESULT;
extern long FINAL_RELEASE;
extern long NOBUILDMAP;
extern long NOCHECKSUM;
extern long START_NEW_QUEST;
extern long CHANGE_LEVEL_ICON;
extern long NEED_INTRO_LAUNCH;
extern long SPLASH_THINGS_STAGE;
extern long REFUSE_GAME_RETURN;
extern long PLAYER_MOUSELOOK_ON;
extern long TRUE_PLAYER_MOUSELOOK_ON;
extern long FRAME_COUNT;
extern long PLAYER_PARALYSED;
extern long STOP_KEYBOARD_INPUT;
extern long USEINTERNORM;
extern long cur_mr;
extern long cur_rf;
extern long STRIKE_TIME;
extern long DeadTime;
extern long INTERTRANSPOLYSPOS;
extern long TRANSPOLYSPOS;
extern long FORCE_FRONT_DRAW;
extern long NO_TEXT_AT_ALL;
extern long FORCE_SHOW_FPS;
extern long FOR_EXTERNAL_PEOPLE;
extern long LAST_ROOM;
extern long LAST_PORTALS_COUNT;
extern int iTimeToDrawD7;
extern long LaunchDemo;

extern short uw_mode;

extern long CURRENT_BASE_FOCAL;
extern float BOW_FOCAL;

extern float GLOBAL_SLOWDOWN;
extern float sp_max_start;
extern float fZFogEnd;
extern float LAST_FADEVALUE;

extern float PROGRESS_BAR_TOTAL;
extern float PROGRESS_BAR_COUNT;
extern float OLD_PROGRESS_BAR_COUNT;

extern bool bOLD_CLIPP;

extern void DANAE_KillCinematic();
extern void LaunchWaitingCine();

extern Cinematic* ControlCinematique;
extern EERIE_3DOBJ* eyeballobj;
extern EERIE_3DOBJ * arrowobj;
extern TextureContainer * Movable;
extern TextureContainer * tflare;
extern INTERACTIVE_OBJ * FlyingOverIO;
extern E_ARX_STATE_MOUSE eMouseState;
extern Vec3f LastValidPlayerPos;
extern Color ulBKGColor;
extern EERIE_CAMERA conversationcamera;
extern ParticleManager * pParticleManager;
extern CircularVertexBuffer<TexturedVertex> * pDynamicVertexBuffer_TLVERTEX; // VB using TLVERTEX format.
extern CircularVertexBuffer<SMY_VERTEX3> * pDynamicVertexBuffer;
extern CMenuState * pMenu;

TextureContainer * ChangeLevel = NULL;
TextureContainer * Movable = NULL;   // TextureContainer for Movable Items (Red Cross)

long WILL_QUICKLOAD=0;
long WILL_QUICKSAVE=0;
long NEED_SPECIAL_RENDEREND=0;
long WILL_RELOAD_ALL_TEXTURES=0; // Set To 1 if Textures are to be reloaded from disk and restored.
long BOOKBUTTON=0;
long LASTBOOKBUTTON=0;
long EXTERNALVIEW=0;
long LASTEXTERNALVIEW=1;
long ARX_CONVERSATION=0;
long ARX_CONVERSATION_MODE=-1;
long ARX_CONVERSATION_LASTIS=-1;
long LAST_CONVERSATION=0;
long SHOW_INGAME_MINIMAP= 1;
long NEED_TEST_TEXT=0;
unsigned long FRAMETICKS=0;

float PLAYER_ARMS_FOCAL = 350.f;
float currentbeta=0.f;

unsigned char ARX_FLARES_Block=1;

Vec3f LASTCAMPOS;
Anglef LASTCAMANGLE;
INTERACTIVE_OBJ * CAMERACONTROLLER=NULL;
INTERACTIVE_OBJ *lastCAMERACONTROLLER=NULL;


//*************************************************************************************
// DANAE()
// ArxGame constructor. Sets attributes for the app.
//*************************************************************************************
ArxGame::ArxGame()
{
	m_bAppUseZBuffer  = true;
}

bool ArxGame::Initialize()
{
	bool init = Application::Initialize();
	if(!init) {
		return false;
	}

	init = InitGameData();
	if(!init) {
		return false;
	}

	init = InitLocalisation();
	if(!init) {
		return false;
	}

	Create();

	return true;
}

bool ArxGame::InitWindow() {
	
	m_MainWindow = new Win32Window();
	
	// Register ourself as a listener for this window messages
	m_MainWindow->AddListener(this);
	
	return m_MainWindow->Init(arxVersion, config.video.width, config.video.height, true, config.video.fullscreen);
	
}

bool ArxGame::InitGraphics() {
	
	GRenderer = new Direct3DRenderer();
	
	GRenderer->setGamma(config.video.luminosity, config.video.contrast, config.video.gamma);
	
	return true;
}

bool ArxGame::InitInput()
{
	LogDebug << "Input init";
	bool init = ARX_INPUT_Init();
	if(init) {
		LogInfo << "Input init success";
	} else {
		LogError << "Input init failed";
	}

	return init;
}

bool ArxGame::InitSound()
{
	LogDebug << "Sound init";
	bool init = ARX_SOUND_Init();
	if(init) {
		LogInfo << "Sound init success";
	} else {
		LogWarning << "Sound init failed";
	}

	return true;
}

bool ArxGame::InitGameData()
{
	bool init;
	
	init = AddPaks();
	if(!init) {
		LogError << "Error loading pak files";
		return false;
	}

	ARX_SOUND_LoadData();
	
	return init;
}

bool ArxGame::AddPaks() {
	
	arx_assert(!resources);
	
	resources = new PakReader;
	
	fs::path pak_data = "data.pak";
	if(!resources->addArchive(pak_data)) {
		LogFatal << "Unable to find main data file " << pak_data;
	}
	
	fs::path pak_loc = "loc.pak";
	if(!resources->addArchive(pak_loc)) {
		fs::path pak_loc_default = "loc_default.pak";
		if(!resources->addArchive(pak_loc_default)) {
			LogFatal << "Unable to find localisation file " << pak_loc << " or " << pak_loc_default;
		}
	}
	
	fs::path pak_data2 = "data2.pak";
	if(!resources->addArchive(pak_data2)) {
		LogFatal << "Unable to find aux data file " << pak_data2;
	}
	
	fs::path pak_sfx = "sfx.pak";
	if(!resources->addArchive(pak_sfx)) {
		LogFatal << "Unable to find sfx data file " << pak_sfx;
	}
	
	fs::path pak_speech = "speech.pak";
	if(!resources->addArchive(pak_speech)) {
		fs::path pak_speech_default = "speech_default.pak";
		if(!resources->addArchive(pak_speech_default)) {
			LogFatal << "Unable to find speech data file " << pak_speech << " or " << pak_speech_default;
		}
	}
	
	resources->addFiles("editor", "editor");
	resources->addFiles("game", "game");
	resources->addFiles("graph", "graph");
	resources->addFiles("localisation", "localisation");
	resources->addFiles("misc", "misc");
	resources->addFiles("sfx", "sfx");
	resources->addFiles("speech", "speech");
	
	return true;
}

//*************************************************************************************
// Create()
//*************************************************************************************
bool ArxGame::Create() {
	
	HRESULT hr;

	// Enumerate available D3D devices. The callback is used so the app can
	// confirm/reject each enumerated device depending on its capabilities.
	if (FAILED(hr = D3DEnum_EnumerateDevices(NULL)))
	{
		DisplayFrameworkError(hr, MSGERR_APPMUSTEXIT);
		return false;
	}

	// Select a device. Ask for a hardware device that renders in a window.
	if (FAILED(hr = D3DEnum_SelectDefaultDevice(&m_pDeviceInfo)))
	{
		DisplayFrameworkError(hr, MSGERR_APPMUSTEXIT);
		return false;
	}

	// Create a new CD3DFramework class. This class does all of our D3D
	// initialization and manages the common D3D objects.
	if (NULL == (m_pFramework = new CD3DFramework7()))
	{
		DisplayFrameworkError(E_OUTOFMEMORY, MSGERR_APPMUSTEXIT);
		return false;
	}
	
	// Initialize the 3D environment for the app
	if (FAILED(hr = Initialize3DEnvironment()))
	{
		DisplayFrameworkError(hr, MSGERR_APPMUSTEXIT);
		Cleanup3DEnvironment();
		return false;
	}

	// The app is ready to go
	m_bReady = true;

	this->m_pFramework->ShowFrame();
	return true;
}

void ArxGame::OnWindowGotFocus(const Window &) {
	
	if(GInput) {
		GInput->reset();
		GInput->unacquireDevices();
		GInput->acquireDevices();
	}
}

void ArxGame::OnWindowLostFocus(const Window &) {
	
	if(GInput) {
		GInput->unacquireDevices();
	}
}

void ArxGame::OnResizeWindow(const Window& window)
{
	// A new window size will require a new backbuffer
	// size, so the 3D structures must be changed accordingly.
	if (window.HasFocus() && m_bReady && !window.IsFullScreen())
		m_pFramework->m_bHasMoved = true;
}

void ArxGame::OnPaintWindow(const Window& window)
{
	ARX_UNUSED(window);

	// Handle paint messages when the app is not ready
	if (m_pFramework && !m_bReady)
	{
		if (m_pDeviceInfo->bWindowed)
		{
			m_pFramework->ShowFrame();
		}
		else
		{
			m_pFramework->FlipToGDISurface(true);
		}
	}
}

void ArxGame::OnDestroyWindow(const Window& window)
{
	ARX_UNUSED(window);

	LogInfo << "Application window is being destroyed";
	m_RunLoop = false;
}

//*************************************************************************************
// Run()
// Message-processing loop. Idle time is used to render the scene.
//*************************************************************************************
void ArxGame::Run()
{
	BeforeRun();
	
	m_RunLoop = true;

	while(m_RunLoop) {
		
		m_MainWindow->Tick();
		
		if(m_MainWindow->HasFocus() && m_bReady) {
			m_RunLoop = Render3DEnvironment();
		}
	}
}

//*************************************************************************************
// FrameMove()
// Called once per frame.
//*************************************************************************************
bool ArxGame::FrameMove() {
	
	if(!WILL_LAUNCH_CINE.empty()) {
		// A cinematic is waiting to be played...
		LaunchWaitingCine();
	}
	
	return true;
}

//*************************************************************************************
// DisplayFrameworkError()
// Displays error messages in a message box
//*************************************************************************************
void ArxGame::DisplayFrameworkError(HRESULT hr, APPMSGTYPE dwType) {
	switch (hr) {
		case D3DENUMERR_NODIRECTDRAW:
			LogError << "Unable to create DirectDraw";
			break;
		case D3DENUMERR_NOCOMPATIBLEDEVICES:
			LogError << "Unable to find any compatible Direct3D devices.";
			break;
		case D3DENUMERR_SUGGESTREFRAST:
			LogError << "Unable to find a compatible devices. Try to enable the reference rasterizer using EnableRefRast.reg.";
			break;
		case D3DENUMERR_ENUMERATIONFAILED:
			LogError << "Enumeration failure. Are you missing (32bit) graphics drivers?";
			break;
		case D3DFWERR_INITIALIZATIONFAILED:
			LogError << "Generic initialization error. Enable debug output for detailed information.";
			break;
		case D3DFWERR_NODIRECTDRAW:
			LogError << "No DirectDraw";
			break;
		case D3DFWERR_NODIRECT3D:
			LogError << "No Direct3D";
			break;
		case D3DFWERR_INVALIDMODE:
			LogError << "This Programe requires 16-bits (or higher) display mode to run in a window.";
			break;
		case D3DFWERR_COULDNTSETCOOPLEVEL:
			LogError << "Unable to set Cooperative Level";
			break;
		case D3DFWERR_NO3DDEVICE:
			LogError << "Unable to create Direct3DDevice object.";
			if (MSGWARN_SWITCHEDTOSOFTWARE == dwType)
				LogError << "Your 3D hardware chipset may not support rendering in the current display mode.";
			break;
		case D3DFWERR_NOZBUFFER:
			LogError << "No ZBuffer";
			break;
		case D3DFWERR_INVALIDZBUFFERDEPTH:
			LogError << "Invalid Z-buffer depth. Try switching modes from 16- to 32-bit (or vice versa)";
			break;
		case D3DFWERR_NOVIEWPORT:
			LogError << "No Viewport";
			break;
		case D3DFWERR_NOPRIMARY:
			LogError << "No primary";
			break;
		case D3DFWERR_NOCLIPPER:
			LogError << "No Clipper";
			break;
		case D3DFWERR_BADDISPLAYMODE:
			LogError << "Bad display mode";
			break;
		case D3DFWERR_NOBACKBUFFER:
			LogError << "No backbuffer";
			break;
		case D3DFWERR_NONZEROREFCOUNT:
			LogError << "A DDraw object has a non-zero reference count (meaning it was not properly cleaned up).";
			break;
		case D3DFWERR_NORENDERTARGET:
			LogError << "No render target";
			break;
		case E_OUTOFMEMORY:
			LogError << "Not enough memory!";
			break;
		case DDERR_OUTOFVIDEOMEMORY:
			LogError << "There was insufficient video memory to use the hardware device.";
			break;
		default:
			LogError << "Generic application error. Enable debug output for detailed information.";
	}

	if (MSGERR_APPMUSTEXIT == dwType) {
		LogError <<"This Program will now exit.";
	} else if (MSGWARN_SWITCHEDTOSOFTWARE == dwType){
		LogError <<"Switching to software rasterizer.";
	}
}

long MouseDragX, MouseDragY;
 
//*************************************************************************************
// Render3DEnvironment()
// Draws the scene.
//*************************************************************************************
bool ArxGame::Render3DEnvironment() {
	
	HRESULT hr;

	// Check the cooperative level before rendering
	if(FAILED(hr = m_pFramework->GetDirectDraw()->TestCooperativeLevel())) {
		
		printf("TestCooperativeLevel failed\n");
		switch(hr) {
			
			case DDERR_EXCLUSIVEMODEALREADYSET:
			case DDERR_NOEXCLUSIVEMODE:
				// Do nothing because some other app has exclusive mode
				return true;
			
			case DDERR_WRONGMODE:
				
				// The display mode changed on us. Resize accordingly
				if(m_pDeviceInfo->bWindowed) {
					return Change3DEnvironment();
				}
				break;
			
		}
		
		return SUCCEEDED(hr);
	}
	
	// Get the relative time, in seconds
	if(!FrameMove()) {
		return false;
	}
	
	// Render the scene as normal
	if(!Render()) {
		return false;
	}
	
	// Show the frame on the primary surface.
	if(FAILED(hr = m_pFramework->ShowFrame())) {
		
		printf("ShowFrame FAILED: %d %d <- look for this in ddraw.h\n", hr, hr&0xFFFF);
		
		if(DDERR_SURFACELOST != hr) {
			return false;
		}
		
		printf("RestoreSurfaces\n");
		m_pFramework->RestoreSurfaces();
		RestoreSurfaces();
	}
	
	return true;
}

//*************************************************************************************
// Cleanup3DEnvironment()
// Cleanup scene objects
//*************************************************************************************
void ArxGame::Cleanup3DEnvironment() {
	
	m_bReady  = false;
	
	if(m_pFramework) {
		DeleteDeviceObjects();
		delete m_pFramework, m_pFramework = NULL;
		FinalCleanup();
	}
	
	D3DEnum_FreeResources();
}

//*************************************************************************************
// Change3DEnvironment()
// Handles driver, device, and/or mode changes for the app.
//*************************************************************************************
bool ArxGame::Change3DEnvironment() {
	
	HRESULT hr;
	
	// Release all scene objects that will be re-created for the new device
	DeleteDeviceObjects();

	// Release framework objects, so a new device can be created
	if(FAILED(hr = m_pFramework->DestroyObjects())) {
		DisplayFrameworkError(hr, MSGERR_APPMUSTEXIT);
		return false;
	}

	// Check if going from fullscreen to windowed mode, or vice versa.
	m_MainWindow->SetFullscreen(!m_pDeviceInfo->bWindowed);
	
	// Inform the framework class of the driver change. It will internally
	// re-create valid surfaces, a d3ddevice, etc.
	if(FAILED(hr = Initialize3DEnvironment())) {
		DisplayFrameworkError(hr, MSGERR_APPMUSTEXIT);
		return false;
	}
	
	return true;
}

//*************************************************************************************
// Initialize3DEnvironment()
// Initializes the sample framework, then calls the app-specific function
// to initialize device specific objects. This code is structured to
// handled any errors that may occur during initialization
//*************************************************************************************
HRESULT ArxGame::Initialize3DEnvironment() {
	
	HRESULT hr;
	DWORD dwFrameworkFlags = 0L;
	dwFrameworkFlags |= (!m_pDeviceInfo->bWindowed ? D3DFW_FULLSCREEN : 0L);
	dwFrameworkFlags |= (m_pDeviceInfo->bStereo   ? D3DFW_STEREO  : 0L);
	dwFrameworkFlags |= (m_bAppUseZBuffer    ? D3DFW_ZBUFFER  : 0L);
	
	// Initialize the D3D framework
	if (SUCCEEDED(hr = m_pFramework->Initialize((HWND)m_MainWindow->GetHandle(),
					   m_pDeviceInfo->pDriverGUID, m_pDeviceInfo->pDeviceGUID,
					   &m_pDeviceInfo->ddsdFullscreenMode, dwFrameworkFlags)))
	{
		
		if (SUCCEEDED(hr = InitDeviceObjects()))
		{
			return S_OK;
		}
		else
		{
			DeleteDeviceObjects();
			m_pFramework->DestroyObjects();
		}
	}

	// If we get here, the first initialization passed failed. If that was with a
	// hardware device, try again using a software rasterizer instead.
	if (m_pDeviceInfo->bHardware)
	{
		// Try again with a software rasterizer
		DisplayFrameworkError(hr, MSGWARN_SWITCHEDTOSOFTWARE);
		D3DEnum_SelectDefaultDevice(&m_pDeviceInfo, D3DENUM_SOFTWAREONLY);
		return Initialize3DEnvironment();
	}
	return hr;
}

//*************************************************************************************
//*************************************************************************************
bool ArxGame::SwitchFullScreen()
{
	m_bReady = false;
	m_pDeviceInfo->bWindowed = !m_pDeviceInfo->bWindowed;

	if (FAILED(Change3DEnvironment()))
	{
		LogError << ("ChangeEnvironement Failed");
		return false;
	}

	m_bReady = true;
	m_pFramework->m_bHasMoved = true;

	return true;
}

//*************************************************************************************
// OutputText()
// Draws text on the window.
//*************************************************************************************
void ArxGame::OutputText(int x, int y, const string & str) {
	if(m_pFramework->GetRenderSurface()) {
		hFontInGame->Draw(x, y, str, Color(255, 255, 0));
	}
}

bool ArxGame::BeforeRun() {
	
	LogDebug << "Before Run...";
	
	ControlCinematique = new Cinematic(mainApp->m_pFramework->m_dwRenderWidth, mainApp->m_pFramework->m_dwRenderHeight);
	
	memset(&necklace,0,sizeof(ARX_NECKLACE));
	
	long old = GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = -1;
	
	necklace.lacet = loadObject("graph/interface/book/runes/lacet.teo");
	
	necklace.runes[RUNE_AAM] =         loadObject("graph/interface/book/runes/runes_aam.teo");
	necklace.runes[RUNE_CETRIUS] =     loadObject("graph/interface/book/runes/runes_citrius.teo");
	necklace.runes[RUNE_COMUNICATUM] = loadObject("graph/interface/book/runes/runes_comunicatum.teo");
	necklace.runes[RUNE_COSUM] =       loadObject("graph/interface/book/runes/runes_cosum.teo");
	necklace.runes[RUNE_FOLGORA] =     loadObject("graph/interface/book/runes/runes_folgora.teo");
	necklace.runes[RUNE_FRIDD] =       loadObject("graph/interface/book/runes/runes_fridd.teo");
	necklace.runes[RUNE_KAOM] =        loadObject("graph/interface/book/runes/runes_kaom.teo");
	necklace.runes[RUNE_MEGA] =        loadObject("graph/interface/book/runes/runes_mega.teo");
	necklace.runes[RUNE_MORTE] =       loadObject("graph/interface/book/runes/runes_morte.teo");
	necklace.runes[RUNE_MOVIS] =       loadObject("graph/interface/book/runes/runes_movis.teo");
	necklace.runes[RUNE_NHI] =         loadObject("graph/interface/book/runes/runes_nhi.teo");
	necklace.runes[RUNE_RHAA] =        loadObject("graph/interface/book/runes/runes_rhaa.teo");
	necklace.runes[RUNE_SPACIUM] =     loadObject("graph/interface/book/runes/runes_spacium.teo");
	necklace.runes[RUNE_STREGUM] =     loadObject("graph/interface/book/runes/runes_stregum.teo");
	necklace.runes[RUNE_TAAR] =        loadObject("graph/interface/book/runes/runes_taar.teo");
	necklace.runes[RUNE_TEMPUS] =      loadObject("graph/interface/book/runes/runes_tempus.teo");
	necklace.runes[RUNE_TERA] =        loadObject("graph/interface/book/runes/runes_tera.teo");
	necklace.runes[RUNE_VISTA] =       loadObject("graph/interface/book/runes/runes_vista.teo");
	necklace.runes[RUNE_VITAE] =       loadObject("graph/interface/book/runes/runes_vitae.teo");
	necklace.runes[RUNE_YOK] =         loadObject("graph/interface/book/runes/runes_yok.teo");
	
	necklace.pTexTab[RUNE_AAM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_aam[icon]");
	necklace.pTexTab[RUNE_CETRIUS] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_cetrius[icon]");
	necklace.pTexTab[RUNE_COMUNICATUM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_comunicatum[icon]");
	necklace.pTexTab[RUNE_COSUM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_cosum[icon]");
	necklace.pTexTab[RUNE_FOLGORA] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_folgora[icon]");
	necklace.pTexTab[RUNE_FRIDD] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_fridd[icon]");
	necklace.pTexTab[RUNE_KAOM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_kaom[icon]");
	necklace.pTexTab[RUNE_MEGA] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_mega[icon]");
	necklace.pTexTab[RUNE_MORTE] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_morte[icon]");
	necklace.pTexTab[RUNE_MOVIS] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_movis[icon]");
	necklace.pTexTab[RUNE_NHI] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_nhi[icon]");
	necklace.pTexTab[RUNE_RHAA] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_rhaa[icon]");
	necklace.pTexTab[RUNE_SPACIUM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_spacium[icon]");
	necklace.pTexTab[RUNE_STREGUM] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_stregum[icon]");
	necklace.pTexTab[RUNE_TAAR] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_taar[icon]");
	necklace.pTexTab[RUNE_TEMPUS] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_tempus[icon]");
	necklace.pTexTab[RUNE_TERA] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_tera[icon]");
	necklace.pTexTab[RUNE_VISTA] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_vista[icon]");
	necklace.pTexTab[RUNE_VITAE] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_vitae[icon]");
	necklace.pTexTab[RUNE_YOK] = TextureContainer::LoadUI("graph/obj3d/interactive/items/magic/rune_aam/rune_yok[icon]");
	
	for(size_t i = 0; i < RUNE_COUNT-1; i++) { // TODO why -1?
		if(necklace.pTexTab[i]) {
			necklace.pTexTab[i]->CreateHalo();
		}
	}
	
	// TODO this is the only place where _LoadTheObj is used
	EERIE_3DOBJ * _fogobj = _LoadTheObj("editor/obj3d/fog_generator.teo", "node_teo maps");
	ARX_FOGS_Set_Object(_fogobj);
	
	eyeballobj = _LoadTheObj("editor/obj3d/eyeball.teo", "eyeball_teo maps");
	cabal = _LoadTheObj("editor/obj3d/cabal.teo", "cabal_teo maps");
	nodeobj = _LoadTheObj("editor/obj3d/node.teo", "node_teo maps");
	
	cameraobj = loadObject("graph/obj3d/interactive/system/camera/camera.teo");
	markerobj = loadObject("graph/obj3d/interactive/system/marker/marker.teo");
	arrowobj = loadObject("graph/obj3d/interactive/items/weapons/arrow/arrow.teo");
	
	for(size_t i = 0; i < MAX_GOLD_COINS_VISUALS; i++) {
		
		std::ostringstream oss;
		
		if(i == 0) {
			oss << "graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin.teo";
		} else {
			oss << "graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin" << (i + 1) << ".teo";
		}
		
		GoldCoinsObj[i] = loadObject(oss.str());
		
		oss.str(string());
		
		if(i == 0) {
			oss << "graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin[icon]";
		} else {
			oss << "graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin" << (i + 1) << "[icon]";
		}
		
		GoldCoinsTC[i] = TextureContainer::LoadUI(oss.str());
	}
	
	Movable = TextureContainer::LoadUI("graph/interface/cursors/wrong");
	ChangeLevel = TextureContainer::LoadUI("graph/interface/icons/change_lvl");
	
	ARX_PLAYER_LoadHeroAnimsAndMesh();
	
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = old;
	
	return true;
}

bool ArxGame::Render() {
	
	FrameTime = ARX_TIME_Get();

	if (GLOBAL_SLOWDOWN!=1.f)
	{
		float ft;
		ft=FrameTime-LastFrameTime;
		Original_framedelay=ft;

		ft*=1.f-GLOBAL_SLOWDOWN;
		float minus;

		minus = ft;
		ARXTotalPausedTime+=minus;
		FrameTime = ARX_TIME_Get();

		if (LastFrameTime>FrameTime)
		{
			LastFrameTime=FrameTime;
		}

		ft=FrameTime-LastFrameTime;

		FrameDiff = ft;
		// Under 10 FPS the whole game slows down to avoid unexpected results...
		_framedelay=(float)FrameDiff;
	}
	else
	{
		// Nuky - added this security because sometimes when hitting ESC, FrameDiff would get negative
		if (LastFrameTime>FrameTime)
		{
			LastFrameTime=FrameTime;
		}
		FrameDiff = FrameTime-LastFrameTime;

		float FD;
		FD=FrameDiff;
		// Under 10 FPS the whole game slows down to avoid unexpected results...
		_framedelay=((float)(FrameDiff));
		FrameDiff = _framedelay;

		Original_framedelay=_framedelay;

// Original_framedelay = 1000/25;
		ARXTotalPausedTime+=FD-FrameDiff;
	}

static float _AvgFrameDiff = 150.f;
	if( FrameDiff > _AvgFrameDiff * 10.f )
	{
		FrameDiff = _AvgFrameDiff * 10.f;
	}
	else if ( FrameDiff > 15.f )
	{
		_AvgFrameDiff+= (FrameDiff - _AvgFrameDiff )*0.01f;
	}

	if( GInput->isKeyPressedNowPressed(Keyboard::Key_F12) )
	{
		EERIE_PORTAL_ReleaseOnlyVertexBuffer();
		ComputePortalVertexBuffer();
	}

	ACTIVECAM = &subj;

	if (this->m_pFramework->m_bHasMoved)
	{
		LogDebug << "has moved";
		
		DanaeRestoreFullScreen();

		this->m_pFramework->m_bHasMoved=false;

		AdjustUI();
	}

	// Update input
	GInput->update();
	ReMappDanaeButton();
	AdjustMousePosition();

	// Manages Splash Screens if needed
	if(DANAE_ManageSplashThings()) {
		goto norenderend;
	}

	// Clicked on New Quest ? (TODO:need certainly to be moved somewhere else...)
	if (START_NEW_QUEST)
	{
		LogDebug << "start quest";
		DANAE_StartNewQuest();
	}

	// Update Various Player Infos for this frame.
	if (FirstFrame==0)
		ARX_PLAYER_Frame_Update();
	
	// Project need to reload all textures ???
	if (WILL_RELOAD_ALL_TEXTURES)
	{
		LogDebug << "reload all textures";
		//ReloadAllTextures(); TODO is this needed for changing resolutions in-game?
		WILL_RELOAD_ALL_TEXTURES=0;
	}

	// Are we being teleported ?
	if ((TELEPORT_TO_LEVEL[0]) && (CHANGE_LEVEL_ICON==200))
	{
		LogDebug << "teleport to " << TELEPORT_TO_LEVEL << " " << TELEPORT_TO_POSITION << " "
		         << TELEPORT_TO_ANGLE;
		CHANGE_LEVEL_ICON=-1;
		ARX_CHANGELEVEL_Change(TELEPORT_TO_LEVEL, TELEPORT_TO_POSITION, TELEPORT_TO_ANGLE);
		memset(TELEPORT_TO_LEVEL,0,64);
		memset(TELEPORT_TO_POSITION,0,64);
	}

	if (NEED_INTRO_LAUNCH)
	{
		LogDebug << "need intro launch";
		SetEditMode(0);
		BLOCK_PLAYER_CONTROLS=1;
		ARX_INTERFACE_PlayerInterfaceModify(0,0);
		ARX_Menu_Resources_Release();
		ARXmenu.currentmode=AMCM_OFF;
		ARX_TIME_UnPause();
		SPLASH_THINGS_STAGE=14;
		NEED_INTRO_LAUNCH=0;
		REFUSE_GAME_RETURN=1;
		const char RESOURCE_LEVEL_10[] = "graph/levels/level10/level10.dlf";
		OLD_PROGRESS_BAR_COUNT=PROGRESS_BAR_COUNT=0;
		PROGRESS_BAR_TOTAL = 108;
		LoadLevelScreen(10);
		DanaeLoadLevel(RESOURCE_LEVEL_10);
		FORBID_SAVE=0;
		FirstFrame=1;
		SPLASH_THINGS_STAGE=0;
		GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapRepeat);
		return false;
	}
		
	//Setting long from long
	subj.centerx = DANAECENTERX;
	subj.centery = DANAECENTERY;

	//Casting long to float
	subj.posleft = subj.transform.xmod = ARX_CLEAN_WARN_CAST_FLOAT( DANAECENTERX );
	subj.postop  = subj.transform.ymod = ARX_CLEAN_WARN_CAST_FLOAT( DANAECENTERY );

	// Finally computes current focal
	BASE_FOCAL=(float)CURRENT_BASE_FOCAL+(BOW_FOCAL*( 1.0f / 4 ));

	// SPECIFIC code for Snapshot MODE... to insure constant capture framerate

	PULSATE=EEsin(FrameTime / 800);
	EERIEDrawnPolys=0;

	// EditMode Specific code
	if (EDITMODE)
	{
		TOTIOPDL=0;
		BLOCK_PLAYER_CONTROLS=0;
	}

	if (FirstFrame==0) // Checks for Keyboard & Moulinex
	{
		ARX_MOUSE_OVER=0;

		if (!EDITMODE && (ARXmenu.currentmode == AMCM_OFF)) // Playing Game
		{
			// Checks Clicks in Book Interface
			if (ARX_INTERFACE_MouseInBook())
			{
				ARX_MOUSE_OVER|=ARX_MOUSE_OVER_BOOK;
				LASTBOOKBUTTON=BOOKBUTTON;
				BOOKBUTTON=EERIEMouseButton;

				if ( ((EERIEMouseButton & 1) && !(LastMouseClick & 1) )
					|| ((EERIEMouseButton & 2) && !(LastMouseClick & 2) ) )
				{
					bookclick.x=DANAEMouse.x;
					bookclick.y=DANAEMouse.y;
				}
			}
			else if (InSecondaryInventoryPos(&DANAEMouse))
				ARX_MOUSE_OVER|=ARX_MOUSE_OVER_INVENTORY_2;
			else if (InPlayerInventoryPos(&DANAEMouse))
				ARX_MOUSE_OVER|=ARX_MOUSE_OVER_INVENTORY;
		}

		if ( (player.Interface & INTER_COMBATMODE)
			|| (PLAYER_MOUSELOOK_ON) )
		{
			FlyingOverIO = NULL; // Avoid to check with those modes
		}
		else
		{
			if ((DRAGINTER == NULL) && (FRAME_COUNT<=0))
			{
				if (!BLOCK_PLAYER_CONTROLS && !TRUE_PLAYER_MOUSELOOK_ON && !(ARX_MOUSE_OVER & ARX_MOUSE_OVER_BOOK)
					&& (eMouseState != MOUSE_IN_NOTE)
				   )
					FlyingOverIO = FlyingOverObject(&DANAEMouse);
				else
					FlyingOverIO = NULL;
			}
		}

		if ( (!PLAYER_PARALYSED)
			|| (ARXmenu.currentmode != AMCM_OFF) )

		{
			if (!STOP_KEYBOARD_INPUT)
				ManageKeyMouse();
			else
			{
				STOP_KEYBOARD_INPUT++;

				if (STOP_KEYBOARD_INPUT>2) STOP_KEYBOARD_INPUT=0;
			}
		}
	}
	else // Manages our first frameS
	{
		LogDebug << "first frame";
		ARX_TIME_Get();

		FirstFrameHandling();
		goto norenderend;
	}

	if (CheckInPolyPrecis(player.pos.x,player.pos.y,player.pos.z))
	{
		LastValidPlayerPos.x=player.pos.x;
		LastValidPlayerPos.y=player.pos.y;
		LastValidPlayerPos.z=player.pos.z;
	}

	// Updates Externalview
	EXTERNALVIEW=0;

	GRenderer->SetRenderState(Renderer::Fog, false);

	if(ARX_Menu_Render()) {
		goto norenderend;
	}

	if (WILL_QUICKSAVE)
	{
		SnapShot * pSnapShot = new SnapShot("sct", true);
		pSnapShot->GetSnapShotDim(160,100);
		delete pSnapShot;

		if (WILL_QUICKSAVE>=2)
		{
			ARX_QuickSave();
			WILL_QUICKSAVE=0;
		}
		else WILL_QUICKSAVE++;
	}

	if (WILL_QUICKLOAD)
	{
		WILL_QUICKLOAD=0;

		if (ARX_QuickLoad())
			NEED_SPECIAL_RENDEREND=1;
	}

	if (NEED_SPECIAL_RENDEREND)
	{
		NEED_SPECIAL_RENDEREND=0;
		goto norenderend;
	}

	GRenderer->SetRenderState(Renderer::Fog, true);
	GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapRepeat);

	// Are we displaying a 2D cinematic ? Yes = manage it
	if ( PLAY_LOADED_CINEMATIC
		&& ControlCinematique
			&& ControlCinematique->projectload)
	{
		if (DANAE_Manage_Cinematic()==1)
			goto norenderend;

		goto renderend;
	}

	if (ARXmenu.currentmode == AMCM_OFF)
	{
		if (!PLAYER_PARALYSED)
		{
			if (ManageEditorControls()) goto finish;
		}

		if ((!BLOCK_PLAYER_CONTROLS) && (!PLAYER_PARALYSED))
		{
			ManagePlayerControls();
		}
	}

	ARX_PLAYER_Manage_Movement();

	ARX_PLAYER_Manage_Visual();

	if (FRAME_COUNT<=0)
		ARX_MINIMAP_ValidatePlayerPos();

	// SUBJECTIVE VIEW UPDATE START  *********************************************************
	{
		// Clear screen & Z buffers
		if(desired.flags & GMOD_DCOLOR) {
			GRenderer->Clear(Renderer::ColorBuffer | Renderer::DepthBuffer, current.depthcolor.to<u8>());
		}
		else
		{
			subj.bkgcolor=ulBKGColor;
			GRenderer->Clear(Renderer::ColorBuffer | Renderer::DepthBuffer, subj.bkgcolor);
		}

		//-------------------------------------------------------------------------------
		//               DRAW CINEMASCOPE 16/9
		if(CINEMA_DECAL != 0.f) {
			Rect rectz[2];
			rectz[0].left = rectz[1].left = 0;
			rectz[0].right = rectz[1].right = DANAESIZX;
			rectz[0].top = 0;
			ARX_CHECK_LONG(CINEMA_DECAL * Yratio);
			long lMulResult = static_cast<long>(CINEMA_DECAL * Yratio);
			rectz[0].bottom = lMulResult;
			rectz[1].top = DANAESIZY - lMulResult;
			rectz[1].bottom = DANAESIZY;
			GRenderer->Clear(Renderer::ColorBuffer | Renderer::DepthBuffer, Color::none, 0.0f, 2, rectz);
		}
		//-------------------------------------------------------------------------------

	if(!GRenderer->BeginScene())
	{
		return false;
	}
	
	GRenderer->SetRenderState(Renderer::DepthWrite, true);
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);

	if ( (inter.iobj[0]) && (inter.iobj[0]->animlayer[0].cur_anim) )
	{
		ManageNONCombatModeAnimations();
		long old=USEINTERNORM;
		USEINTERNORM=0;
		float speedfactor;
		speedfactor=inter.iobj[0]->basespeed+inter.iobj[0]->speed_modif;

		if (cur_mr==3) speedfactor+=0.5f;

		if (cur_rf==3) speedfactor+=1.5f;

		if (speedfactor < 0) speedfactor = 0;

		long tFrameDiff = Original_framedelay;
	
		if ((player.Interface & INTER_COMBATMODE) && (STRIKE_TIME))// need some precision for weapon...
		{
			float restore=ACTIVECAM->use_focal;

			if ((!EXTERNALVIEW) && (!BOW_FOCAL))
			{
				ACTIVECAM->use_focal=PLAYER_ARMS_FOCAL*Xratio;
			}

			float cur=0;

			while ((cur<tFrameDiff) && (!(inter.iobj[0]->ioflags & IO_FREEZESCRIPT)))
			{
				long step=min(50L,tFrameDiff);

				if (inter.iobj[0]->ioflags & IO_FREEZESCRIPT) step=0;


				float iCalc = step*speedfactor ;
				ARX_CHECK_ULONG(iCalc);

				arx_assert(inter.iobj[0]->obj != NULL);
				EERIEDrawAnimQuat(inter.iobj[0]->obj, &inter.iobj[0]->animlayer[0], &inter.iobj[0]->angle,
				                  &inter.iobj[0]->pos, ARX_CLEAN_WARN_CAST_ULONG(iCalc), inter.iobj[0], false);

					if ((player.Interface & INTER_COMBATMODE) && (inter.iobj[0]->animlayer[1].cur_anim != NULL))
				ManageCombatModeAnimations();

				if (inter.iobj[0]->animlayer[1].cur_anim!=NULL)
					ManageCombatModeAnimationsEND();

				cur+=step*speedfactor;
			}

			ACTIVECAM->use_focal=restore;
		}
		else
		{
			float restore=ACTIVECAM->use_focal;

			if ((!EXTERNALVIEW) && (!BOW_FOCAL))
			{
				ACTIVECAM->use_focal=PLAYER_ARMS_FOCAL*Xratio;
			}


			float val=(float)tFrameDiff*speedfactor;
			ARX_CHECK_LONG(val);

			if (inter.iobj[0]->ioflags & IO_FREEZESCRIPT) val=0;

			arx_assert(inter.iobj[0]->obj != NULL);
			EERIEDrawAnimQuat(inter.iobj[0]->obj, &inter.iobj[0]->animlayer[0], &inter.iobj[0]->angle,
			                  &inter.iobj[0]->pos, ARX_CLEAN_WARN_CAST_ULONG(val), inter.iobj[0], false);


				if ((player.Interface & INTER_COMBATMODE) && (inter.iobj[0]->animlayer[1].cur_anim != NULL))
				ManageCombatModeAnimations();

			if (inter.iobj[0]->animlayer[1].cur_anim!=NULL)
					ManageCombatModeAnimationsEND();

			ACTIVECAM->use_focal=restore;
		}

		USEINTERNORM=old;
	}

	INTERACTIVE_OBJ * io;
	io=inter.iobj[0];
	ANIM_USE * useanim;
	useanim=&io->animlayer[1];
	ANIM_HANDLE ** alist;
	alist=io->anims;

	if ( BOW_FOCAL
			&& (useanim->cur_anim!=alist[ANIM_MISSILE_STRIKE_PART_1])
			&& (useanim->cur_anim!=alist[ANIM_MISSILE_STRIKE_PART_2])
			&& (useanim->cur_anim!=alist[ANIM_MISSILE_STRIKE_CYCLE]))
		{
			BOW_FOCAL-=Original_framedelay;

			if (BOW_FOCAL<0) BOW_FOCAL=0;
		}

		if (eyeball.exist == 2)
		{
		subj.d_pos.x=eyeball.pos.x;
		subj.d_pos.y=eyeball.pos.y;
		subj.d_pos.z=eyeball.pos.z;
		subj.d_angle.a=eyeball.angle.a;
		subj.d_angle.b=eyeball.angle.b;
		subj.d_angle.g=eyeball.angle.g;
		EXTERNALVIEW=1;
	}
	else if (EXTERNALVIEW)
	{
		float t=radians(player.angle.b);
		Vec3f tt;

		for (long l=0;l<250;l+=10)
		{
			tt.x=player.pos.x+(float)EEsin(t)*(float)l;
			tt.y=player.pos.y-50.f;
			tt.z=player.pos.z-(float)EEcos(t)*(float)l;
			EERIEPOLY * ep =EECheckInPoly(&tt);

			if (ep)
			{
				subj.d_pos.x=tt.x;
				subj.d_pos.y=tt.y;
				subj.d_pos.z=tt.z;
			}
			else break;
		}

		subj.d_angle.a=player.angle.a+30.f;
		subj.d_angle.b=player.angle.b;
		subj.d_angle.g=player.angle.g;
		EXTERNALVIEW=1;
	}
	else
	{
		subj.angle.a=player.angle.a;
		subj.angle.b=player.angle.b;
		subj.angle.g=player.angle.g;
		EXTERNALVIEW=0;

		if (inter.iobj[0])
		{
			long id = inter.iobj[0]->obj->fastaccess.view_attach;

			if (id!=-1)
			{
				subj.pos.x=inter.iobj[0]->obj->vertexlist3[id].v.x;
				subj.pos.y=inter.iobj[0]->obj->vertexlist3[id].v.y;
				subj.pos.z=inter.iobj[0]->obj->vertexlist3[id].v.z;

				Vec3f vect;
				vect.x=subj.pos.x-player.pos.x;
				vect.y=0;
				vect.z=subj.pos.z-player.pos.z;
				float len=Vector_Magnitude(&vect);

				if (len>46.f)
				{
					float div=46.f/len;
					vect.x*=div;
					vect.z*=div;
					subj.pos.x=player.pos.x+vect.x;
					subj.pos.z=player.pos.z+vect.z;
				}
			}
			else
			{
				subj.pos.x=player.pos.x;
				subj.pos.y=player.pos.y;
				subj.pos.z=player.pos.z;
				subj.pos.y+=PLAYER_BASE_HEIGHT;
			}
	}
		}

	if (EXTERNALVIEW)
	{
		subj.pos.x=(subj.pos.x+subj.d_pos.x)*( 1.0f / 2 );
		subj.pos.y=(subj.pos.y+subj.d_pos.y)*( 1.0f / 2 );
		subj.pos.z=(subj.pos.z+subj.d_pos.z)*( 1.0f / 2 );

		subj.angle.a=InterpolateAngle(subj.angle.a,subj.d_angle.a,0.1f);
		subj.angle.b=InterpolateAngle(subj.angle.b,subj.d_angle.b,0.1f);
		subj.angle.g=InterpolateAngle(subj.angle.g,subj.d_angle.g,0.1f);
	}

	if ((ARX_CONVERSATION) && (main_conversation.actors_nb))
	{
		// Decides who speaks !!
		if (main_conversation.current<0)
		for (long j=0;j<main_conversation.actors_nb;j++)
		{
			if (main_conversation.actors[j]>=0)
			{
				for(size_t k = 0 ; k < MAX_ASPEECH; k++) {
					if (aspeech[k].exist)
						if (aspeech[k].io==inter.iobj[main_conversation.actors[j]])
						{
							main_conversation.current=k;
							j=main_conversation.actors_nb+1;
							k=MAX_ASPEECH+1;
						}
				}
			}
		}

		long is=main_conversation.current;

		if (ARX_CONVERSATION_LASTIS!=is) ARX_CONVERSATION_MODE=-1;

		ARX_CONVERSATION_LASTIS=is;

		if (ARX_CONVERSATION_MODE==-1)
		{
			ARX_CONVERSATION_MODE=(long)(float)(rnd()*3.f+1.f);
			conversationcamera.size.a=rnd()*50.f;
			conversationcamera.size.b=0.f;
			conversationcamera.size.g=rnd()*50.f;
			conversationcamera.d_angle.a=0.f;
			conversationcamera.d_angle.b=0.f;
			conversationcamera.d_angle.g=0.f;

			if (rnd()>0.4f) conversationcamera.d_angle.a=(1.f-rnd()*2.f)*( 1.0f / 30 );

			if (rnd()>0.4f) conversationcamera.d_angle.b=(1.f-rnd()*1.2f)*( 1.0f / 5 );

			if (rnd()>0.4f) conversationcamera.d_angle.g=(1.f-rnd()*2.f)*( 1.0f / 40 );

			if (rnd()>0.5f)
			{
				conversationcamera.size.a=MAKEANGLE(180.f+rnd()*20.f-10.f);
				conversationcamera.size.b=0.f;
				conversationcamera.size.g=0.f;
				conversationcamera.d_angle.g=0.08f;
				conversationcamera.d_angle.b=0.f;
				conversationcamera.d_angle.a = 0.f;
			}
		}
		else
		{
			conversationcamera.size += conversationcamera.d_angle * FrameDiff;
		}

		Vec3f sourcepos,targetpos;

		if (ApplySpeechPos(&conversationcamera,is))
		{
			targetpos.x=conversationcamera.d_pos.x;
			targetpos.y=conversationcamera.d_pos.y;
			targetpos.z=conversationcamera.d_pos.z;
			sourcepos.x=conversationcamera.pos.x;
			sourcepos.y=conversationcamera.pos.y;
			sourcepos.z=conversationcamera.pos.z;
		}
		else
		{
			targetpos.x=player.pos.x;
			targetpos.y=player.pos.y;
			targetpos.z=player.pos.z;
			float t=radians(player.angle.b);
			sourcepos.x=targetpos.x+(float)EEsin(t)*100.f;
			sourcepos.y=targetpos.y;
			sourcepos.z=targetpos.z-(float)EEcos(t)*100.f;
			}

		Vec3f vect,vec2;
		vect.x=targetpos.x-sourcepos.x;
		vect.y=targetpos.y-sourcepos.y;
		vect.z=targetpos.z-sourcepos.z;
		float mag=1.f/Vector_Magnitude(&vect);
		vect.x*=mag;
		vect.y*=mag;
		vect.z*=mag;
		float dist=250.f-conversationcamera.size.g;

		if (dist<0.f) dist=(90.f-(dist*( 1.0f / 20 )));
		else if (dist<90.f) dist=90.f;

		_YRotatePoint(&vect,&vec2,EEcos(radians(conversationcamera.size.a)),EEsin(radians(conversationcamera.size.a)));
		
		sourcepos.x=targetpos.x-vec2.x*dist;
		sourcepos.y=targetpos.y-vec2.y*dist;
		sourcepos.z=targetpos.z-vec2.z*dist;

		if (conversationcamera.size.b!=0.f)
			sourcepos.y+=120.f-conversationcamera.size.b*( 1.0f / 10 );

		conversationcamera.pos.x=sourcepos.x;
		conversationcamera.pos.y=sourcepos.y;
		conversationcamera.pos.z=sourcepos.z;
		SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
		subj.pos.x=conversationcamera.pos.x;
		subj.pos.y=conversationcamera.pos.y;
		subj.pos.z=conversationcamera.pos.z;
		subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
		subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
		subj.angle.g=0.f;
		EXTERNALVIEW=1;
	}
	else
	{
		ARX_CONVERSATION_MODE=-1;
		ARX_CONVERSATION_LASTIS=-1;

		if (LAST_CONVERSATION)
		{
			AcquireLastAnim(inter.iobj[0]);
			ANIM_Set(&inter.iobj[0]->animlayer[1],inter.iobj[0]->anims[ANIM_WAIT]);
			inter.iobj[0]->animlayer[1].flags|=EA_LOOP;
		}
	}

		////////////////////////
	// Checks SCRIPT TIMERS.
	if (FirstFrame==0)
		ARX_SCRIPT_Timer_Check();

	/////////////////////////////////////////////
	// Now checks for speech controlled cinematic
	{
		long valid=-1;

		for(size_t i = 0; i < MAX_ASPEECH; i++) {
			if ((aspeech[i].exist) && (aspeech[i].cine.type>0))
			{
				valid=i;
				break;
			}
		}

		if (valid>=0)
		{
			CinematicSpeech * acs=&aspeech[valid].cine;
			INTERACTIVE_OBJ * io=aspeech[valid].io;
			float rtime=(float)(ARX_TIME_Get()-aspeech[valid].time_creation)/(float)aspeech[valid].duration;

			if (rtime<0) rtime=0;

			if (rtime>1) rtime=1;

			float itime=1.f-rtime;

			if ((rtime>=0.f) && (rtime<=1.f) && io)
			{
				float alpha,beta,distance,_dist;

				switch (acs->type)
				{
					case ARX_CINE_SPEECH_KEEP: {
						subj.pos.x=acs->pos1.x;
						subj.pos.y=acs->pos1.y;
						subj.pos.z=acs->pos1.z;
						subj.angle.a=acs->pos2.x;
						subj.angle.b=acs->pos2.y;
						subj.angle.g=acs->pos2.z;
						EXTERNALVIEW=1;
						break;
					}
					case ARX_CINE_SPEECH_ZOOM: {
						//need to compute current values
						alpha=acs->startangle.a*itime+acs->endangle.a*rtime;
						beta=acs->startangle.b*itime+acs->endangle.b*rtime;
						distance=acs->startpos*itime+acs->endpos*rtime;
						Vec3f targetpos = acs->pos1;
						conversationcamera.pos.x=-EEsin(radians(MAKEANGLE(io->angle.b+beta)))*distance+targetpos.x;
						conversationcamera.pos.y= EEsin(radians(MAKEANGLE(io->angle.a+alpha)))*distance+targetpos.y;
						conversationcamera.pos.z= EEcos(radians(MAKEANGLE(io->angle.b+beta)))*distance+targetpos.z;
						SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
						subj.pos.x=conversationcamera.pos.x;
						subj.pos.y=conversationcamera.pos.y;
						subj.pos.z=conversationcamera.pos.z;
						subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
						subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
						subj.angle.g=0.f;
						EXTERNALVIEW=1;
						break;
					}
					case ARX_CINE_SPEECH_SIDE_LEFT:
					case ARX_CINE_SPEECH_SIDE: {

						if (ValidIONum(acs->ionum))
						{

							const Vec3f & from = acs->pos1;
							const Vec3f & to = acs->pos2;

							Vec3f vect = (to - from).getNormalized();

							Vec3f vect2;
							if (acs->type==ARX_CINE_SPEECH_SIDE_LEFT)
							{
								Vector_RotateY(&vect2,&vect,-90);
							}
							else
							{
								Vector_RotateY(&vect2,&vect,90);
							}

							distance=acs->f0*itime+acs->f1*rtime;
							vect2 *= distance;
							_dist = dist(from, to);
							Vec3f tfrom = from + vect * acs->startpos * (1.0f / 100) * _dist;
							Vec3f tto = from + vect * acs->endpos * (1.0f / 100) * _dist;
							Vec3f targetpos;
							targetpos.x=tfrom.x*itime+tto.x*rtime;
							targetpos.y=tfrom.y*itime+tto.y*rtime+acs->f2;
							targetpos.z=tfrom.z*itime+tto.z*rtime;
							conversationcamera.pos.x=targetpos.x+vect2.x;
							conversationcamera.pos.y=targetpos.y+vect2.y+acs->f2;
							conversationcamera.pos.z=targetpos.z+vect2.z;
							SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
							subj.pos = conversationcamera.pos;
							subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
							subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
							subj.angle.g=0.f;
							EXTERNALVIEW=1;
						}

						break;
					}
					case ARX_CINE_SPEECH_CCCLISTENER_R:
					case ARX_CINE_SPEECH_CCCLISTENER_L:
					case ARX_CINE_SPEECH_CCCTALKER_R:
					case ARX_CINE_SPEECH_CCCTALKER_L: {

						//need to compute current values
						if (ValidIONum(acs->ionum))
						{
							Vec3f targetpos;
							if ((acs->type==ARX_CINE_SPEECH_CCCLISTENER_L)
								|| (acs->type==ARX_CINE_SPEECH_CCCLISTENER_R))
							{
								conversationcamera.pos.x=acs->pos2.x;
								conversationcamera.pos.y=acs->pos2.y;
								conversationcamera.pos.z=acs->pos2.z;
								targetpos.x=acs->pos1.x;
								targetpos.y=acs->pos1.y;
								targetpos.z=acs->pos1.z;
							}
							else
							{
								conversationcamera.pos.x=acs->pos1.x;
								conversationcamera.pos.y=acs->pos1.y;
								conversationcamera.pos.z=acs->pos1.z;
								targetpos.x=acs->pos2.x;
								targetpos.y=acs->pos2.y;
								targetpos.z=acs->pos2.z;
							}
							
							distance=(acs->startpos*itime+acs->endpos*rtime)*( 1.0f / 100 );
							
							Vec3f vect;
							vect.x=conversationcamera.pos.x-targetpos.x;
							vect.y=conversationcamera.pos.y-targetpos.y;
							vect.z=conversationcamera.pos.z-targetpos.z;
							Vec3f vect2;
							Vector_RotateY(&vect2,&vect,90);
							vect2.normalize();
							Vec3f vect3 = vect.getNormalized();

							vect = vect * distance + vect3 * 80.f;
							vect2 *= 45.f;

							if ((acs->type==ARX_CINE_SPEECH_CCCLISTENER_R)
								|| (acs->type==ARX_CINE_SPEECH_CCCTALKER_R))
							{
								vect2 = -vect2;
							}

							conversationcamera.pos = vect + targetpos + vect2;
							SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
							subj.pos = conversationcamera.pos;
							subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
							subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
							subj.angle.g=0.f;
							EXTERNALVIEW=1;
						}

						break;
					}
					case ARX_CINE_SPEECH_NONE: break;
				}

				LASTCAMPOS.x=subj.pos.x;
				LASTCAMPOS.y=subj.pos.y;
				LASTCAMPOS.z=subj.pos.z;
				LASTCAMANGLE.a=subj.angle.a;
				LASTCAMANGLE.b=subj.angle.b;
				LASTCAMANGLE.g=subj.angle.g;
			}
		}
	}

	if (player.life<=0)
	{
			DeadTime += ARX_CLEAN_WARN_CAST_LONG(FrameDiff);
		float mdist = EEfabs(player.physics.cyl.height)-60;
		DeadCameraDistance+=(float)FrameDiff*( 1.0f / 80 )*((mdist-DeadCameraDistance)/mdist)*2.f;

		if (DeadCameraDistance>mdist) DeadCameraDistance=mdist;

		Vec3f targetpos;

		targetpos.x = player.pos.x;
			targetpos.y = player.pos.y;
			targetpos.z = player.pos.z;

			long id  = inter.iobj[0]->obj->fastaccess.view_attach;
		long id2 = GetActionPointIdx( inter.iobj[0]->obj, "chest2leggings" );

		if (id!=-1)
		{
			targetpos.x = inter.iobj[0]->obj->vertexlist3[id].v.x;
			targetpos.y = inter.iobj[0]->obj->vertexlist3[id].v.y;
			targetpos.z = inter.iobj[0]->obj->vertexlist3[id].v.z;
		}

		conversationcamera.pos.x = targetpos.x;
		conversationcamera.pos.y = targetpos.y - DeadCameraDistance;
		conversationcamera.pos.z = targetpos.z;

		if (id2!=-1)
		{
				conversationcamera.pos.x=inter.iobj[0]->obj->vertexlist3[id2].v.x;
				conversationcamera.pos.y=inter.iobj[0]->obj->vertexlist3[id2].v.y-DeadCameraDistance;
				conversationcamera.pos.z=inter.iobj[0]->obj->vertexlist3[id2].v.z;
		}

		SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
		subj.pos.x=conversationcamera.pos.x;
		subj.pos.y=conversationcamera.pos.y;
		subj.pos.z=conversationcamera.pos.z;
		subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
		subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
			subj.angle.g = 0;
		EXTERNALVIEW=1;

#ifdef BUILD_EDITOR
		if(!GAME_EDITOR)
			BLOCK_PLAYER_CONTROLS=1;
#endif
	}
	else
	{
		DeadCameraDistance=0;

	}

	/////////////////////////////////////
	LAST_CONVERSATION=ARX_CONVERSATION;

	if (GInput->isKeyPressedNowPressed(Keyboard::Key_Spacebar) && (CAMERACONTROLLER!=NULL))
		CAMERACONTROLLER=NULL;

	if (CAMERACONTROLLER!=NULL)
	{
		if (lastCAMERACONTROLLER!=CAMERACONTROLLER)
		{
			currentbeta=CAMERACONTROLLER->angle.b;
		}

			Vec3f targetpos;

		targetpos.x=CAMERACONTROLLER->pos.x;
		targetpos.y=CAMERACONTROLLER->pos.y+PLAYER_BASE_HEIGHT;
		targetpos.z=CAMERACONTROLLER->pos.z;

			float delta_angle = AngleDifference(currentbeta, CAMERACONTROLLER->angle.b);
			float delta_angle_t = delta_angle * FrameDiff * ( 1.0f / 1000 );

			if (EEfabs(delta_angle_t) > EEfabs(delta_angle)) delta_angle_t = delta_angle;

			currentbeta += delta_angle_t;
		float t=radians(MAKEANGLE(currentbeta));
		conversationcamera.pos.x=targetpos.x+(float)EEsin(t)*160.f;
		conversationcamera.pos.y=targetpos.y+40.f;
		conversationcamera.pos.z=targetpos.z-(float)EEcos(t)*160.f;

		SetTargetCamera(&conversationcamera,targetpos.x,targetpos.y,targetpos.z);
		subj.pos.x=conversationcamera.pos.x;
		subj.pos.y=conversationcamera.pos.y;
		subj.pos.z=conversationcamera.pos.z;
		subj.angle.a=MAKEANGLE(-conversationcamera.angle.a);
		subj.angle.b=MAKEANGLE(conversationcamera.angle.b-180.f);
		subj.angle.g=0.f;
		EXTERNALVIEW=1;
	}

	lastCAMERACONTROLLER=CAMERACONTROLLER;

	if ((USE_CINEMATICS_CAMERA) && (USE_CINEMATICS_PATH.path!=NULL))
	{
		Vec3f pos,pos2;
			USE_CINEMATICS_PATH._curtime = ARX_TIME_Get();

		USE_CINEMATICS_PATH._curtime+=50;
		long pouet2=ARX_PATHS_Interpolate(&USE_CINEMATICS_PATH,&pos);
		USE_CINEMATICS_PATH._curtime-=50;
		long pouet=ARX_PATHS_Interpolate(&USE_CINEMATICS_PATH,&pos2);

		if ((pouet!=-1) && (pouet2!=-1))
		{
			if(USE_CINEMATICS_CAMERA == 2) {
				subj.pos = pos;
				subj.d_angle = subj.angle;
				pos2 = (pos2 + pos) * (1.0f/2);
				SetTargetCamera(&subj, pos2.x, pos2.y, pos2.z);
			} else {
				DebugSphere(pos.x, pos.y, pos.z, 2, 50, Color::red);
			}

			if (USE_CINEMATICS_PATH.aupflags & ARX_USEPATH_FLAG_FINISHED) // was .path->flags
			{
				USE_CINEMATICS_CAMERA=0;
				USE_CINEMATICS_PATH.path=NULL;
			}
		}
		else
		{
			USE_CINEMATICS_CAMERA=0;
			USE_CINEMATICS_PATH.path=NULL;
		}
	}

	UpdateCameras();

		///////////////////////////////////////////
	ARX_PLAYER_FrameCheck(Original_framedelay);

	if (MasterCamera.exist)
	{
		if (MasterCamera.exist & 2)
		{
			MasterCamera.exist&=~2;
			MasterCamera.exist|=1;
			MasterCamera.io=MasterCamera.want_io;
			MasterCamera.aup=MasterCamera.want_aup;
			MasterCamera.cam=MasterCamera.want_cam;
		}

		if (MasterCamera.cam->focal<100.f) MasterCamera.cam->focal=350.f;

		SetActiveCamera(MasterCamera.cam);
		EXTERNALVIEW=1;
	}
	else
	{
		// Set active camera for this viewport
		SetActiveCamera(&subj);
	}

	ARX_GLOBALMODS_Apply();

	if (EDITMODE) GRenderer->SetRenderState(Renderer::Fog, false);

		ManageQuakeFX();

	// Prepare ActiveCamera
	PrepareCamera(ACTIVECAM);
	// Recenter Viewport depending on Resolution

	// setting long from long
	ACTIVECAM->centerx = DANAECENTERX;
	ACTIVECAM->centery = DANAECENTERY;
	// casting long to float
	ACTIVECAM->posleft = ACTIVECAM->transform.xmod = ARX_CLEAN_WARN_CAST_FLOAT( DANAECENTERX );
	ACTIVECAM->postop = ACTIVECAM->transform.ymod = ARX_CLEAN_WARN_CAST_FLOAT( DANAECENTERY );


	// Set Listener Position
	{
		float t = radians(MAKEANGLE(ACTIVECAM->angle.b));
		Vec3f front(-EEsin(t), 0.f, EEcos(t));
		front.normalize();
		Vec3f up(0.f, 1.f, 0.f);
		ARX_SOUND_SetListener(&ACTIVECAM->pos, &front, &up);
	}

	// Reset Transparent Polys Idx
	INTERTRANSPOLYSPOS=TRANSPOLYSPOS=0;

	// Check For Hiding/unHiding Player Gore
	if ((EXTERNALVIEW) || (player.life<=0))
	{
		ARX_INTERACTIVE_Show_Hide_1st(inter.iobj[0],0);
	}

	if (!EXTERNALVIEW)
	{
		ARX_INTERACTIVE_Show_Hide_1st(inter.iobj[0],1);
	}

	LASTEXTERNALVIEW=EXTERNALVIEW;

	// NOW DRAW the player (Really...)
	if ( (inter.iobj[0])
		&& (inter.iobj[0]->animlayer[0].cur_anim) )
	{
		float restore=ACTIVECAM->use_focal;

		if ((!EXTERNALVIEW) && (!BOW_FOCAL))
		{
			ACTIVECAM->use_focal=PLAYER_ARMS_FOCAL*Xratio;
		}

		if (!EXTERNALVIEW)
			FORCE_FRONT_DRAW=1;

		if (inter.iobj[0]->invisibility>0.9f) inter.iobj[0]->invisibility=0.9f;

		arx_assert(inter.iobj[0]->obj != NULL);
		EERIEDrawAnimQuat(inter.iobj[0]->obj, &inter.iobj[0]->animlayer[0], &inter.iobj[0]->angle,
		                  &inter.iobj[0]->pos, 0, inter.iobj[0]);
		
		ACTIVECAM->use_focal=restore;
		FORCE_FRONT_DRAW=0;
	}

	// SUBJECTIVE VIEW UPDATE START  *********************************************************
	GRenderer->SetRenderState(Renderer::DepthWrite, true);
	GRenderer->SetRenderState(Renderer::DepthTest, true);

	if (FirstFrame==0)
	{
		PrepareIOTreatZone();
			ARX_PHYSICS_Apply();

		if (FRAME_COUNT<=0)
				PrecalcIOLighting(&ACTIVECAM->pos, ACTIVECAM->cdepth * 0.6f);

		ACTIVECAM->fadecolor.r=current.depthcolor.r;
		ACTIVECAM->fadecolor.g=current.depthcolor.g;
		ACTIVECAM->fadecolor.b=current.depthcolor.b;

		if (uw_mode)
		{
			float val=10.f;
			GRenderer->GetTextureStage(0)->SetMipMapLODBias(val);
			ARX_SCENE_Render(1);
			val=-0.3f;
			GRenderer->GetTextureStage(0)->SetMipMapLODBias(val);
		} else {
			ARX_SCENE_Render(1);
		}
	}

	// Begin Particles ***************************************************************************
	if (!(Project.hide & HIDE_PARTICLES))
	{
		if (pParticleManager)
		{
			pParticleManager->Update(ARX_CLEAN_WARN_CAST_LONG(FrameDiff));
			pParticleManager->Render();
		}

		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		GRenderer->SetRenderState(Renderer::DepthWrite, false);
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		ARX_FOGS_Render();

		ARX_PARTICLES_Render(&subj);
		UpdateObjFx();
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);

	}

	// End Particles ***************************************************************************

	if (!EDITMODE) // Playing Game
	{
		// Checks Magic Flares Drawing
		if (!PLAYER_PARALYSED)
		{
			if (EERIEMouseButton & 1)
			{
				if ((ARX_FLARES_Block==0) && (CurrSlot<(long)MAX_SLOT))
					ARX_SPELLS_AddPoint(DANAEMouse);
				else
				{
					CurrPoint=0;
					ARX_FLARES_Block=0;
					CurrSlot=1;
				}
			}
			else if (ARX_FLARES_Block==0)
				ARX_FLARES_Block=1;
		}

		ARX_SPELLS_Precast_Check();
		ARX_SPELLS_ManageMagic();
		ARX_SPELLS_UpdateSymbolDraw();

		ManageTorch();

		// Renders Magical Flares
		if ( !((player.Interface & INTER_MAP )
			&& (!(player.Interface & INTER_COMBATMODE)))
			&& flarenum
			)
		{
			ARX_MAGICAL_FLARES_Draw(FRAMETICKS);
				FRAMETICKS = ARXTimeUL();
		}
	}
#ifdef BUILD_EDITOR
	else  // EDITMODE == true
	{
		if (!(Project.hide & HIDE_NODES))
			RenderAllNodes();

		std::stringstream ss("EDIT MODE - Selected ");
		ss <<  NbIOSelected;
		ARX_TEXT_Draw(hFontInBook, 100, 2, ss.str(), Color::yellow);
	
		if (EDITION==EDITION_FOGS)
			ARX_FOGS_RenderAll();
	}
#endif
	
	GRenderer->SetRenderState(Renderer::AlphaBlending, true);
	GRenderer->SetRenderState(Renderer::DepthWrite, false);

	// Checks some specific spell FX
	CheckMr();

	if (Project.improve)
		DrawImproveVisionInterface();
	else
	{
		if ((subj.focal<BASE_FOCAL))
		{
			static const float INC_FOCAL = 75.0f;
			subj.focal+=INC_FOCAL;

			if (subj.focal>BASE_FOCAL) subj.focal=BASE_FOCAL;
		}
		else if (subj.focal>BASE_FOCAL) subj.focal=BASE_FOCAL;
	}

	if (eyeball.exist!=0)
	{
		DrawMagicSightInterface();
	}

		if (PLAYER_PARALYSED)
	{
		GRenderer->SetRenderState(Renderer::DepthWrite, false);
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);

		EERIEDrawBitmap(0.f, 0.f, (float)DANAESIZX, (float)DANAESIZY, 0.0001f, NULL, Color(71, 71, 255));
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		GRenderer->SetRenderState(Renderer::DepthWrite, true);
	}

	if (FADEDIR)
	{
		ManageFade();
	}

	GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	GRenderer->SetRenderState(Renderer::DepthWrite, true);
	
	// Red screen fade for damages.
	ARX_DAMAGE_Show_Hit_Blood();

	// Manage Notes/Books opened on screen
	GRenderer->SetRenderState(Renderer::Fog, false);
	ARX_INTERFACE_NoteManage();

	finish:; //----------------------------------------------------------------
	// Update spells
	ARX_SPELLS_Update();
	GRenderer->SetCulling(Renderer::CullNone);
	GRenderer->SetRenderState(Renderer::Fog, true);

	// Manage Death visual & Launch menu...
	if (DeadTime>2000)
		ARX_PLAYER_Manage_Death();

	//-------------------------------------------------------------------------

	// INTERFACE
		// Remove the Alphablend State if needed : NO Z Clear
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	GRenderer->SetRenderState(Renderer::Fog, false);

	// Draw game interface if needed
	if (ARXmenu.currentmode == AMCM_OFF)
	if (!(Project.hide & HIDE_INTERFACE) && !CINEMASCOPE)
	{
		GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapClamp);
		DrawAllInterface();
		DrawAllInterfaceFinish();

		if ( (player.Interface & INTER_MAP )
			&& (!(player.Interface & INTER_COMBATMODE))
			&& flarenum
			)
		{
			GRenderer->SetRenderState(Renderer::DepthTest, false);
			ARX_MAGICAL_FLARES_Draw(FRAMETICKS);
			GRenderer->SetRenderState(Renderer::DepthTest, true);
			FRAMETICKS = ARXTimeUL();
		}
	}

	GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapRepeat);

	GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	PopAllTriangleList();
	GRenderer->SetRenderState(Renderer::AlphaBlending, true);
	PopAllTriangleListTransparency();
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);

	GRenderer->SetRenderState(Renderer::Fog, true);
		this->GoFor2DFX();
	GRenderer->SetRenderState(Renderer::Fog, false);
	GRenderer->Clear(Renderer::DepthBuffer);

	// Speech Management
	if (!EDITMODE)
	{
		ARX_SPEECH_Check();
		ARX_SPEECH_Update();
	}

	GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapRepeat);

	if(pTextManage && !pTextManage->Empty())
	{
		pTextManage->Update(FrameDiff);
		pTextManage->Render();
	}

	if (SHOW_INGAME_MINIMAP && ((PLAY_LOADED_CINEMATIC == 0) && (!CINEMASCOPE) && (!BLOCK_PLAYER_CONTROLS) && (ARXmenu.currentmode == AMCM_OFF))
		&& (!(player.Interface & INTER_MAP ) ))
	{
			long SHOWLEVEL = ARX_LEVELS_GetRealNum(CURRENTLEVEL);

		if ((SHOWLEVEL>=0) && (SHOWLEVEL<32))
			ARX_MINIMAP_Show(SHOWLEVEL,1,1);
	}

		//-------------------------------------------------------------------------

	// CURSOR Rendering
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);

	if (DRAGINTER)
	{
		ARX_INTERFACE_RenderCursor();

		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		PopAllTriangleList();
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		PopAllTriangleListTransparency();
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);

		ARX_INTERFACE_HALO_Flush();
	}
	else
	{
		ARX_INTERFACE_HALO_Flush();
		ARX_INTERFACE_RenderCursor();
	}

	GRenderer->SetRenderState(Renderer::Fog, true);

	//----------------RENDEREND------------------------------------------------
	renderend:
		;

	if (sp_max_start)
		Manage_sp_max();

	// Some Visual Debug/Info Text
	CalcFPS();

	if (!FINAL_COMMERCIAL_DEMO)
	{
		if ((NEED_TEST_TEXT) && (!FINAL_COMMERCIAL_DEMO))
		{
			ShowTestText();
		}

		if (!NO_TEXT_AT_ALL)
		{
			if (ViewMode & VIEWMODE_INFOTEXT)
			{
				ShowInfoText();
			}
			else if (FORCE_SHOW_FPS)
			{
				ShowFPS();
			}
		}

	if ((USE_PORTALS) && (NEED_TEST_TEXT) && (!FOR_EXTERNAL_PEOPLE))
		{
			char tex[250];

			switch(USE_PORTALS)
			{
			case 1:
				sprintf(tex,"2DPortals_ROOM: %ld",LAST_ROOM);
				break;
			case 2:
				sprintf(tex,"3DPortals_ROOM: %ld - Vis %ld",LAST_ROOM,LAST_PORTALS_COUNT);
				break;
			case 3:
				sprintf(tex,"3DPortals_ROOM(Transform): %ld - Vis %ld",LAST_ROOM,LAST_PORTALS_COUNT);
				break;
			case 4:
				sprintf(tex,"3DPortals_ROOM(TransformSC): %ld - Vis %ld",LAST_ROOM,LAST_PORTALS_COUNT);
				break;
			}

			mainApp->OutputText( 320, 240, tex );
		}

		if((NEED_TEST_TEXT) && (!FOR_EXTERNAL_PEOPLE))
		{
			if(bOLD_CLIPP)
			{
				mainApp->OutputText(0, 240, "New Clipp" );
			}
			else
			{
				mainApp->OutputText(0,274,"New Clipp");
			}
		}
	}

	//----------------------------------------------------------------------------
	// Begin 2D Pass for Lense Flares

	if ((PLAY_LOADED_CINEMATIC == 0) && (!CINEMASCOPE) && (!BLOCK_PLAYER_CONTROLS) && (ARXmenu.currentmode == AMCM_OFF))
	{
		if (GInput->actionNowPressed(CONTROLS_CUST_QUICKLOAD) && !WILL_QUICKLOAD)
		{
			WILL_QUICKLOAD=1;
		}

		if (GInput->actionNowPressed(CONTROLS_CUST_QUICKSAVE) && !WILL_QUICKSAVE)
		{
			iTimeToDrawD7=2000;
			WILL_QUICKSAVE=1;
		}

		ARX_DrawAfterQuickLoad();
	}

	GRenderer->EndScene();

	//--------------NORENDEREND---------------------------------------------------
	norenderend:
		;

	if(GInput->isKeyPressedNowPressed(Keyboard::Key_F10))
	{
		GetSnapShot();
	}

	if ((LaunchDemo) && (FirstFrame == 0))
	{
		NOCHECKSUM=1;
		LaunchDemo=0;
		LaunchDummyParticle();
	}
	}
	
	if (ARXmenu.currentmode == AMCM_OFF)
	{
		ARX_SCRIPT_AllowInterScriptExec();
		ARX_SCRIPT_EventStackExecute();
		// Updates Damages Spheres
		ARX_DAMAGES_UpdateAll();
		ARX_MISSILES_Update();

		if (FirstFrame==0)
			ARX_PATH_UpdateAllZoneInOutInside();
	}

	LastFrameTime=FrameTime;
	LastMouseClick=EERIEMouseButton;

	return true;
}

void ArxGame::GoFor2DFX()
{
	TexturedVertex lv,ltvv;

	long needed = 0;

	for (long i=0;i<TOTPDL;i++)
	{
		EERIE_LIGHT * el=PDL[i];

		if (el->extras & EXTRAS_FLARE)
		{
			if(distSqr(ACTIVECAM->pos, el->pos) < square(2200)) {
				needed=1;
				break;
			}
		}
	}

	if (!needed) return;

					{
		INTERACTIVE_OBJ* pTableIO[256];
		int nNbInTableIO = 0;

		float temp_increase=_framedelay*( 1.0f / 1000 )*4.f;
		{
			bool bComputeIO = false;

			for (int i=0;i<TOTPDL;i++)
			{
				EERIE_LIGHT * el=PDL[i];

				long lPosx=(long)(float)(el->pos.x*ACTIVEBKG->Xmul);
				long lPosz=(long)(float)(el->pos.z*ACTIVEBKG->Zmul);

				if( (lPosx<0)||
					(lPosx>=ACTIVEBKG->Xsize)||
					(lPosz<0)||
					(lPosz>=ACTIVEBKG->Zsize)||
					(!ACTIVEBKG->fastdata[lPosx][lPosz].treat) )
				{
					el->treat=0;
					continue;
				}

				if (el->extras & EXTRAS_FLARE)
				{
					lv.sx=el->pos.x;
					lv.sy=el->pos.y;
					lv.sz=el->pos.z;
					specialEE_RTP(&lv,&ltvv);
					el->temp-=temp_increase;

					if (!(player.Interface & INTER_COMBATMODE)
						&& (player.Interface & INTER_MAP))
						continue;

					if ((ltvv.rhw > 0.f) &&
						(ltvv.sx>0.f) &&
						(ltvv.sy>(CINEMA_DECAL*Yratio)) &&
						(ltvv.sx<DANAESIZX) &&
						(ltvv.sy<(DANAESIZY-(CINEMA_DECAL*Yratio)))
						)
					{
						Vec3f vector;
						vector.x=lv.sx-ACTIVECAM->pos.x;
						vector.y=lv.sy-ACTIVECAM->pos.y;
						vector.z=lv.sz-ACTIVECAM->pos.z;
						float fNorm = 50.f / vector.length();
						vector *= fNorm;
						TexturedVertex ltvv2;
						lv.sx-=vector.x;
						lv.sy-=vector.y;
						lv.sz-=vector.z;
						specialEE_RTP(&lv,&ltvv2);

						float fZFar=ProjectionMatrix._33*(1.f/(ACTIVECAM->cdepth*fZFogEnd))+ProjectionMatrix._43;

						Vec3f hit;
						EERIEPOLY *tp=NULL;
						Vec2s ees2dlv;
						Vec3f ee3dlv;
						ee3dlv.x = lv.sx;
						ee3dlv.y = lv.sy;
						ee3dlv.z = lv.sz;


						ARX_CHECK_SHORT(ltvv.sx) ;
						ARX_CHECK_SHORT(ltvv.sy) ;

						ees2dlv.x = ARX_CLEAN_WARN_CAST_SHORT(ltvv.sx) ;
						ees2dlv.y = ARX_CLEAN_WARN_CAST_SHORT(ltvv.sy) ;


						if( !bComputeIO )
						{
							GetFirstInterAtPos(&ees2dlv, 2, &ee3dlv, pTableIO, &nNbInTableIO );
							bComputeIO = true;
						}

						if(
							(ltvv.sz>fZFar)||
							EERIELaunchRay3(&ACTIVECAM->pos,&ee3dlv,&hit,tp,1)||
							GetFirstInterAtPos(&ees2dlv, 3, &ee3dlv, pTableIO, &nNbInTableIO )
							)
						{
							el->temp-=temp_increase*2.f;
						}
						else
						{
							el->temp+=temp_increase*2.f;
						}

					}

					if (el->temp<0.f) el->temp=0.f;
					else if (el->temp>.8f) el->temp=.8f;
				}
			}
		}

		// End 2D Pass ***************************************************************************

		{
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			GRenderer->SetRenderState(Renderer::DepthWrite, false);
			GRenderer->SetCulling(Renderer::CullNone);
			GRenderer->SetRenderState(Renderer::DepthTest, false);
			GRenderer->SetFogColor(Color::none);

			for (int i=0;i<TOTPDL;i++)
			{
				EERIE_LIGHT * el=PDL[i];

				if ((!el->exist) || (!el->treat)) continue;

				if (el->extras & EXTRAS_FLARE)
				{
					if (el->temp>0.f)
					{
						lv.sx=el->pos.x;
						lv.sy=el->pos.y;
						lv.sz=el->pos.z;
						lv.rhw=1.f;
						specialEE_RT((TexturedVertex *)&lv,(Vec3f *)&ltvv);
						float v=el->temp;

						if (FADEDIR)
						{
							v*=1.f-LAST_FADEVALUE;
						}

						float siz;

						if (el->extras & EXTRAS_FIXFLARESIZE)
							siz=el->ex_flaresize;
						else
							siz=-el->ex_flaresize;

						EERIEDrawSprite(&lv, siz, tflare, Color3f(v*el->rgb.r,v*el->rgb.g,v*el->rgb.b).to<u8>(), ltvv.sz);

					}
				}
			}

			GRenderer->SetRenderState(Renderer::DepthTest, true);
		}
	}

	GRenderer->SetRenderState(Renderer::DepthWrite, true);
}


bool ArxGame::InitDeviceObjects()
{
	// Enable Z-buffering RenderState
	GRenderer->SetRenderState(Renderer::DepthTest, true);
	
	// Restore All Textures RenderState
	GRenderer->RestoreAllTextures();

	ARX_PLAYER_Restore_Skin();
	
	// Disable Lighting RenderState
	GRenderer->SetRenderState(Renderer::Lighting, false);

	// Setup Texture Border RenderState
	GRenderer->GetTextureStage(0)->SetWrapMode(TextureStage::WrapRepeat);

	GRenderer->GetTextureStage(1)->DisableColor();
	
	// Fog
	float fogEnd = 0.48f;
	float fogStart = fogEnd * 0.65f;
	GRenderer->SetFogParams(Renderer::FogLinear, fogStart, fogEnd);
	GRenderer->SetFogColor(current.depthcolor.to<u8>());
	GRenderer->SetRenderState(Renderer::Fog, true);
	
	SetZBias(0);

	ComputePortalVertexBuffer();
	VertexBuffer<SMY_VERTEX3> * vb3 = GRenderer->createVertexBuffer3(4000, Renderer::Stream);
	pDynamicVertexBuffer = new CircularVertexBuffer<SMY_VERTEX3>(vb3);
	
	VertexBuffer<TexturedVertex> * vb = GRenderer->createVertexBufferTL(4000, Renderer::Stream);
	pDynamicVertexBuffer_TLVERTEX = new CircularVertexBuffer<TexturedVertex>(vb);

	if(pMenu)
	{
		pMenu->bReInitAll=true;
	}

	ARX_SetAntiAliasing();

	EvictManagedTextures();

	return true;
}

//*************************************************************************************
// FinalCleanup()
// Called before the app exits
//*************************************************************************************
bool ArxGame::FinalCleanup() {
	
	EERIE_PATHFINDER_Release();
	ARX_INPUT_Release();
	ARX_SOUND_Release();
	
	return true;
}

//*************************************************************************************
// DeleteDeviceObjects()
// Called when the app is exitting, or the device is being changed,
// this function deletes any device dependant objects.
//*************************************************************************************

bool ArxGame::DeleteDeviceObjects() {
	
	GRenderer->ReleaseAllTextures();
	
	if(pDynamicVertexBuffer_TLVERTEX) {
		delete pDynamicVertexBuffer_TLVERTEX;
		pDynamicVertexBuffer_TLVERTEX = NULL;
	}
	
	if(pDynamicVertexBuffer) {
		delete pDynamicVertexBuffer;
		pDynamicVertexBuffer = NULL;
	}
	
	EERIE_PORTAL_ReleaseOnlyVertexBuffer();
	
	return true;
}
