/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
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

#include "game/Entity.h"

#include <sstream>
#include <iomanip>
#include <cstring>

#include "animation/Animation.h"
#include "ai/Paths.h"

#include "core/Core.h"

#include "game/Camera.h"
#include "game/EntityManager.h"
#include "game/Inventory.h"
#include "game/Item.h"
#include "game/Levels.h"
#include "game/Player.h"
#include "game/NPC.h"

#include "graphics/data/Mesh.h"

#include "gui/Dragging.h"
#include "gui/Interface.h"
#include "gui/Speech.h"
#include "gui/hud/SecondaryInventory.h"

#include "io/log/Logger.h"

#include "platform/Environment.h"

#include "scene/ChangeLevel.h"
#include "scene/GameSound.h"
#include "scene/Interactive.h"
#include "scene/Light.h"
#include "scene/LinkedObject.h"
#include "scene/LoadLevel.h"

extern Entity * pIOChangeWeapon;

Entity::Entity(const res::path & classPath, EntityInstance instance)
	: ioflags(0)
	, lastpos(0.f)
	, pos(0.f)
	, move(0.f)
	, lastmove(0.f)
	, forcedmove(0.f)
	, requestRoomUpdate(true)
	, original_height(0.f)
	, original_radius(0.f)
	, m_icon(nullptr)
	, obj(nullptr)
	, tweaky(nullptr)
	, type_flags(0)
	, scriptload(0)
	, target(0.f)
	, targetinfo(TARGET_NONE)
	, inventory(nullptr)
	, show(SHOW_FLAG_IN_SCENE)
	, collision(0)
	, mainevent(SM_MAIN)
	, infracolor(Color3f::blue)
	, weight(1.f)
	, gameFlags(GFLAG_NEEDINIT | GFLAG_INTERACTIVITY)
	, fall(0.f)
	, initpos(0.f)
	, scale(1.f)
	, usepath(nullptr)
	, symboldraw(nullptr)
	, lastspeechflag(2)
	, inzone(nullptr)
	, m_disabledEvents(0)
	, stat_count(0)
	, stat_sent(0)
	, tweakerinfo(nullptr)
	, material(MATERIAL_NONE)
	, m_inventorySize(1)
	, soundtime(0)
	, soundcount(0)
	, sfx_time(0)
	, collide_door_time(0)
	, ouch_time(0)
	, dmg_sum(0.f)
	, flarecount(0)
	, invisibility(0.f)
	, basespeed(1.f)
	, speed_modif(0.f)
	, rubber(BASE_RUBBER)
	, max_durability(100.f)
	, durability(max_durability)
	, poisonous(0)
	, poisonous_count(0)
	, ignition(0.f)
	, head_rot(0.f)
	, damager_damages(0)
	, damager_type(0)
	, sfx_flag(0)
	, secretvalue(-1)
	, shop_multiply(1.f)
	, isHit(false)
	, inzone_show(SHOW_FLAG_MEGAHIDE)
	, spark_n_blood(0)
	, special_color(Color3f::white)
	, highlightColor(Color3f::black)
	, m_owner(nullptr)
	, m_index(size_t(-1))
	, m_id(classPath, instance)
	, m_idString(m_id.string())
	, m_classPath(classPath)
{
	
	m_index = entities.add(this);
	
	std::fill(anims.begin(), anims.end(), nullptr);
	
	lodYawBeforeLookAtCam = 999999999.f; // something impossible like as in it could be considered as not initialized
	arx_assert(lodYawBeforeLookAtCam == 999999999.f);
	playerDistLastCalcLOD = 0.f;
	LODpreventDegradeDelayUntil = 0;
	lodLastCalcTime = time(0);
	lodCooldownUntil = time(0);
	lodImproveWaitUntil = time(0);
	previousPosForLOD = Vec3f(0.f);
	
	resetLOD(false);
	
	for(size_t l = 0; l < MAX_ANIM_LAYERS; l++) {
		animlayer[l] = AnimLayer();
	}
	
	animBlend.m_active = false;
	animBlend.lastanimtime = 0;
	
	bbox3D = EERIE_3D_BBOX(Vec3f(0.f), Vec3f(0.f));
	
	bbox2D.min = Vec2f(-1.f, -1.f);
	bbox2D.max = Vec2f(-1.f, -1.f);
	
	_itemdata = nullptr;
	_fixdata = nullptr;
	_npcdata = nullptr;
	_camdata = nullptr;
	
	halo_native.color = Color3f(0.2f, 0.5f, 1.f);
	halo_native.radius = 45.f;
	halo_native.flags = 0;
	ARX_HALO_SetToNative(this);
	
}

void Entity::resetLOD(bool bDelete) {
	if(bDelete) {
		//for(auto it1 : objLOD) {
			//if(!it1.second) continue;
			//if(it1.second == objLOD[LOD_PERFECT]) continue; // LOD_PERFECT is the main model and shall not be touched here
			//EERIE_3DOBJ * objChk = it1.second;
			//for(auto it2 : objLOD) {
				//if(it2.second == objChk) it2.second = nullptr; // it1.second will end up as nullptr too and be skipped
			//}
			//delete objChk;
		//}
		for(LODFlag lodChk1 : LODList) {
			if(!objLOD[lodChk1]) continue;
			if(objLOD[lodChk1] == objLOD[LOD_PERFECT]) continue; // LOD_PERFECT is the main model and shall not be touched here
			
			// can delete
			EERIE_3DOBJ * objChk1 = objLOD[lodChk1];
			for(LODFlag lodChk2 : LODList) {
				if(objLOD[lodChk2] == objChk1) {
					LogDebug("nullptr to LOD " << LODtoStr(lodChk2) << ", file=" << objLOD[lodChk2]->fileUniqueRelativePathName);
					objLOD[lodChk2] = nullptr;
				}
			}
			LogDebug("deleting " << LODtoStr(lodChk1) << ", file=" << objChk1->fileUniqueRelativePathName);
			delete objChk1;
		}
	}
	currentLOD = LOD_NONE;
	previousLOD = currentLOD;
	// TODO something like std::fill(aObjLOD.begin(), aObjLOD.end(), nullptr); ?
	objLOD.clear();
	objLOD.emplace(LOD_PERFECT, nullptr);
	objLOD.emplace(LOD_HIGH, nullptr);
	objLOD.emplace(LOD_MEDIUM, nullptr);
	objLOD.emplace(LOD_LOW, nullptr);
	objLOD.emplace(LOD_BAD, nullptr);
	objLOD.emplace(LOD_FLAT, nullptr);
	objLOD.emplace(LOD_ICON, nullptr);
	availableLODFlags = 0;
	iconLODFlags = 0;
}

Entity::~Entity() {
	
	cleanReferences();
	
	if(g_cameraEntity == this) {
		g_cameraEntity = nullptr;
	}
	
	// Releases ToBeDrawn Transparent Polys linked to this object !
	tweaks.clear();
	
	if(obj && !(ioflags & IO_CAMERA) && !(ioflags & IO_MARKER) && !(ioflags & IO_GOLD)) {
		delete obj, obj = nullptr;
	}
	
	spells.removeTarget(this);
	
	delete tweakerinfo;
	delete tweaky, tweaky = nullptr;
	
	ReleaseScript(&script);
	ReleaseScript(&over_script);
	
	for(ANIM_HANDLE * & anim : anims) {
		if(anim) {
			EERIE_ANIMMANAGER_ReleaseHandle(anim);
			anim = nullptr;
		}
	}
	
	lightHandleDestroy(dynlight);
	
	delete usepath;
	
	delete symboldraw;
	symboldraw = nullptr;
	
	if(ioflags & IO_NPC) {
		delete _npcdata;
	} else if(ioflags & IO_ITEM) {
		delete _itemdata->equipitem;
		delete _itemdata;
	} else if(ioflags & IO_FIX) {
		delete _fixdata;
	} else if(ioflags & IO_CAMERA && _camdata) {
		if(g_camera == &_camdata->cam) {
			SetActiveCamera(&g_playerCamera);
		}
		delete _camdata;
	}
	
	g_secondaryInventoryHud.clear(this);
	
	if(inventory) {
		for(auto slot : inventory->slots()) {
			if(slot.entity) {
				slot.entity->pos = GetItemWorldPosition(slot.entity);
				removeFromInventories(slot.entity);
			}
		}
	}
	
	if(m_index != size_t(-1)) {
		entities.remove(m_index);
	}
	
}

void Entity::setObjMain(EERIE_3DOBJ * o) {
	objMain = o;
	obj = objMain;
}

res::path Entity::instancePath() const {
	return m_classPath.parent() / idString();
}

void Entity::setOwner(Entity * owner) {
	
	if(m_owner != owner) {
		
		if(m_owner) {
			removeFromInventories(this);
			unlinkEntity(*this);
			if(m_owner && (m_owner->ioflags & IO_NPC) && m_owner->_npcdata->weapon == this) {
				m_owner->_npcdata->weapon = nullptr;
				m_owner->_npcdata->weapontype = 0;
			}
			if(player.torch == this) {
				arx_assert(m_owner == entities.player());
				ARX_PLAYER_KillTorch();
			}
		}
		
		m_owner = owner;
		
	}
	
	updateOwner();
	
	if(m_owner && g_draggedEntity == this) {
		setDraggedEntity(nullptr);
	}
	
}

void Entity::updateOwner() {
	
	if(m_owner) {
		
		if(player.torch == this) {
			arx_assert(m_owner == entities.player());
			show = SHOW_FLAG_ON_PLAYER;
			return;
		}
		
		if((m_owner->ioflags & IO_NPC) && m_owner->_npcdata->weapon == this) {
			if(show != SHOW_FLAG_HIDDEN && show != SHOW_FLAG_MEGAHIDE) {
				show = (m_owner == entities.player()) ? SHOW_FLAG_ON_PLAYER : SHOW_FLAG_LINKED;
			}
			return;
		}
		
		if(isEntityLinked(*this)) {
			if(show != SHOW_FLAG_HIDDEN && show != SHOW_FLAG_MEGAHIDE) {
				show = (m_owner == entities.player()) ? SHOW_FLAG_ON_PLAYER : SHOW_FLAG_LINKED;
			}
			return;
		}
		
		if(locateInInventories(this)) {
			show = SHOW_FLAG_IN_INVENTORY;
			return;
		}
		
		if(show != SHOW_FLAG_HIDDEN && show != SHOW_FLAG_MEGAHIDE) {
			show = SHOW_FLAG_IN_SCENE;
		}
		m_owner = nullptr;
		
	}
	
}

void Entity::cleanReferences() {
	
	ARX_INTERACTIVE_DestroyIOdelayedRemove(this);
	
	if(g_draggedEntity == this) {
		setDraggedEntity(nullptr);
	}
	
	if(FlyingOverIO == this) {
		FlyingOverIO = nullptr;
	}
	
	if(COMBINE == this) {
		COMBINE = nullptr;
	}
	
	if(pIOChangeWeapon == this) {
		pIOChangeWeapon = nullptr; // TODO we really need a proper weak_ptr
	}
	
	if(ioSteal == this) {
		ioSteal = nullptr;
	}
	
	TREATZONE_RemoveIO(this);
	gameFlags &= ~GFLAG_ISINTREATZONE;
	
	ARX_SPEECH_ReleaseIOSpeech(*this);
	
	ARX_INTERACTIVE_DestroyDynamicInfo(this);
	
	setOwner(nullptr);
	
	ARX_SCRIPT_Timer_Clear_For_IO(this);
	
	spells.endByCaster(index());
	
	lightHandleDestroy(ignit_light);
	
	ARX_SOUND_Stop(ignit_sound);
	ignit_sound = audio::SourcedSample();
	
}

void Entity::destroy() {
	
	LogDebug("destroying entity " << idString());
	
	if(instance() > 0 && !(ioflags & IO_NOSAVE)) {
		if(scriptload) {
			// In case we previously saved this entity...
			currentSavedGameRemoveEntity(idString());
		} else {
			currentSavedGameStoreEntityDeletion(idString());
		}
	}
	
	if(obj) {
		while(!obj->linked.empty()) {
			if(obj->linked.back().io) {
				arx_assert(ValidIOAddress(obj->linked.back().io));
				obj->linked.back().io->destroy();
			} else {
				obj->linked.pop_back();
			}
		}
	}
	if((ioflags & IO_NPC) && _npcdata->weapon) {
		_npcdata->weapon->destroy();
	}
	
	// TODO should we also destroy inventory items
	// currently they remain orphaned with state SHOW_FLAG_IN_INVENTORY
	
	delete this;
	
}

void Entity::destroyOne() {
	
	if((ioflags & IO_ITEM) && _itemdata->count > 1) {
		_itemdata->count--;
	} else {
		destroy();
	}
	
}

bool Entity::isInvulnerable() {
	
	return durability >= 100.f;
	
}

bool Entity::setLOD(const LODFlag lodRequest) {
	if(currentLOD == lodRequest) return true;
	if(!(ioflags & IO_ITEM)) return false; // only items for now
	if(currentLOD == LOD_NONE) {
		if(!obj) return false; // wait it be initialized elsewhere
		if(lodRequest != LOD_PERFECT) return false; // wait first proper request happen
	}
	if(currentLOD != LOD_NONE) if(previousPosForLOD != pos) return false; // wait physics rest. wont work with: if(lastpos != pos) if(obj->pbox->active)
	
	LODFlag lodChk = lodRequest;
	
	static platform::EnvVarHandler<std::string,LODFlag> evLODMax = [](){evLODMax.setId("ARX_LODMax"); return platform::getEnvironmentVariableValueString(evLODMax.ev, evLODMax.id().c_str(), Logger::LogLevel::Info, "", "PERFECT").getString();}();
	if(evLODMax.chkMod()) evLODMax.evc = strToLOD(evLODMax.ev);
	
	static platform::EnvVarHandler<std::string,LODFlag> evLODMin = [](){evLODMin.setId("ARX_LODMin"); return platform::getEnvironmentVariableValueString(evLODMin.ev, evLODMin.id().c_str(), Logger::LogLevel::Info, "", "ICON").getString();}();
	if(evLODMin.chkMod()) evLODMin.evc = strToLOD(evLODMin.ev);
	
	if(evLODMin.evc < evLODMax.evc) {
		evLODMin.ev = LODtoStr(evLODMin.evc = evLODMax.evc);
		LogWarning << "fixed LOD min to '" << LODtoStr(evLODMin.evc) << "'";
	}
	if(evLODMax.evc > evLODMin.evc) {
		evLODMax.ev = LODtoStr(evLODMax.evc = evLODMin.evc);
		LogWarning << "fixed LOD max to '" << evLODMax.ev << "'";
	}
	
	if(!obj) {
		return false;
	}
	
	if(availableLODFlags == 0) {
		if(!load3DModelAndLOD(*this, obj->fileUniqueRelativePathName, obj->pbox != nullptr)) {
			return false;
		}
	}
	
	// because max quality is lowest flag value
	if(lodChk < evLODMax.evc) lodChk = evLODMax.evc;
	if(lodChk > evLODMin.evc) lodChk = evLODMin.evc;
	
	// seek available LOD if requested not found
	if(!(availableLODFlags & lodChk)) {
		if(lodChk < currentLOD) { // requested to improve LOD
			while(lodChk) {
				if(availableLODFlags & lodChk) break;
				// if requested LOD is not available
				lodChk = static_cast<LODFlag>(lodChk >> 1); // will improve LOD more than requested
			}
			arx_assert_msg(lodChk,"LOD_PERFECT shall always be available (original 3D model) but was not found! entity='%s'", idString().c_str());
		}
		
		if(lodChk > currentLOD) { // requested to lower LOD quality
			while(lodChk != LOD_ICON) { // limit to worst quality LOD
				if(availableLODFlags & lodChk) break;
				lodChk = static_cast<LODFlag>(lodChk << 1);
			}
		}
	}
	
	if(lodChk && (availableLODFlags & lodChk)) {
		currentLOD = lodChk;
		obj = objLOD[currentLOD];
		//TODO *(obj->pbox) = *(objLOD[currentLOD]->pbox); // TODO not working, should continue a physics impulse from the other lod. use a part of Eerie_Copy ?
		usemesh = obj->fileUniqueRelativePathName;
		return true;
	}
	
	return false;
}
