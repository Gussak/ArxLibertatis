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

#include "script/ScriptedAnimation.h"

#include <cstring>
#include <string>
#include <string_view>

#include <boost/algorithm/string/predicate.hpp>

#include "ai/Paths.h"
#include "core/GameTime.h"
#include "game/EntityManager.h"
#include "game/Inventory.h"
#include "game/NPC.h"
#include "graphics/data/Mesh.h"
#include "io/resource/ResourcePath.h"
#include "scene/Interactive.h"
#include "script/ScriptUtils.h"
#include "util/Number.h"


namespace script {

namespace {

typedef std::map<std::string_view, AnimationNumber> Animations;
Animations animations;

AnimationNumber getAnimationNumber(std::string_view name) {
	
	auto it = animations.find(name);
	
	return (it == animations.end()) ? ANIM_NONE : it->second;
}

class RotateCommand : public Command {
	
public:
	
	RotateCommand() : Command("rotate", AnyEntity) { }
	
	/**
	 * Rotate [-ea] <e?strEntID> x y z
	 * -a is absolute rotation
	 */
	Result execute(Context & context) override {
		
		Entity * io = context.getEntity();
		std::string strEntID;
		bool bAbs=false;
		
		HandleFlags("ea") {
			if(flg & flag('a')) {
				bAbs=true;
			}
			if(flg & flag('e')) {
				strEntID = context.getWord();
			}
		}
		
		float pitch = context.getFloat();
		float yaw   = context.getFloat();
		float roll  = context.getFloat();
		
		if(strEntID != "") {
			io = entities.getById(context.getStringVar(strEntID));
			if(!io) { //after consume params
				ScriptWarning << "invalid entity ID: " << strEntID;
				return Failed;
			}
		}
		
		DebugScript(' ' << pitch << ' ' << yaw << ' ' << roll);
		io->angle.setPitch(bAbs ? pitch : io->angle.getPitch() + pitch);
		io->angle.setYaw  (bAbs ? yaw   : io->angle.getYaw  () + yaw  );
		io->angle.setRoll (bAbs ? roll  : io->angle.getRoll () + roll );
		io->angle.normalize();
		
		io->animBlend.lastanimtime = 0;
		
		return Success;
	}
	
};

class ForceAnimCommand : public Command {
	
	static void forceAnim(Entity & io, ANIM_HANDLE * ea) {
		
		AnimLayer & layer0 = io.animlayer[0];
		
		if(layer0.cur_anim
		   && layer0.cur_anim != io.anims[ANIM_DIE]
		   && layer0.cur_anim != io.anims[ANIM_HIT1]) {
			AcquireLastAnim(&io);
		}
		
		FinishAnim(&io, layer0.cur_anim);
		io.lastmove = Vec3f(0.f);
		ANIM_Set(layer0, ea);
		layer0.flags |= EA_FORCEPLAY;
		
		CheckSetAnimOutOfTreatZone(&io, layer0);
	}
	
public:
	
	ForceAnimCommand() : Command("forceanim", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string anim = context.getWord();
		
		DebugScript(' ' << anim);
		
		AnimationNumber num = getAnimationNumber(anim);
		if(num == ANIM_NONE) {
			ScriptWarning << "unknown animation: " << anim;
			return Failed;
		}
		
		Entity & io = *context.getEntity();
		if(!io.anims[num]) {
			ScriptWarning << "animation " << anim << " not loaded";
			return Failed;
		}
		
		forceAnim(io, io.anims[num]);
		
		return Success;
	}
	
};

class ForceAngleCommand : public Command {
	
public:
	
	ForceAngleCommand() : Command("forceangle", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		float angle = MAKEANGLE(context.getFloat());
		
		DebugScript(' ' << angle);
		
		context.getEntity()->angle.setYaw(angle);
		
		return Success;
	}
	
};

class PlayAnimCommand : public Command {
	
	static void setNextAnim(Entity * io, ANIM_HANDLE * ea, AnimLayer & layer, bool loop, bool nointerpol) {
		
		if(!io) {
			return;
		}
		
		if(IsDeadNPC(*io)) {
			return;
		}
		
		if(!nointerpol) {
			AcquireLastAnim(io);
		}
		
		FinishAnim(io, layer.cur_anim);
		ANIM_Set(layer, ea);
		
		if(loop) {
			layer.flags |= EA_LOOP;
		} else {
			layer.flags &= ~EA_LOOP;
		}
		layer.flags |= EA_FORCEPLAY;
	}
	
public:
	
	PlayAnimCommand() : Command("playanim") { }
	
	Result execute(Context & context) override {
		
		Entity * iot = context.getEntity();
		long layerIndex = 0;
		bool loop = false;
		bool nointerpol = false;
		bool execute = false;
		
		HandleFlags("123lnep") {
			if(flg & flag('1')) {
				layerIndex = 0;
			}
			if(flg & flag('2')) {
				layerIndex = 1;
			}
			if(flg & flag('3')) {
				layerIndex = 2;
			}
			loop = test_flag(flg, 'l');
			nointerpol = test_flag(flg, 'n');
			execute = test_flag(flg, 'e');
			if(flg & flag('p')) {
				iot = entities.player();
				iot->move = iot->lastmove = Vec3f(0.f);
			}
		}
		
		std::string anim = context.getWord();
		
		DebugScript(' ' << options << ' ' << anim);
		
		if(!iot) {
			ScriptWarning << "must either use -p or use with IO";
			return Failed;
		}
		
		AnimLayer & layer = iot->animlayer[layerIndex];
		
		if(anim == "none") {
			layer.cur_anim = nullptr;
			return Success;
		}
		
		AnimationNumber num = getAnimationNumber(anim);
		if(num == ANIM_NONE) {
			ScriptWarning << "unknown anim: " << anim;
			return Failed;
		}
		
		if(!iot->anims[num]) {
			ScriptWarning << "animation " << anim << " not loaded";
			return Failed;
		}
		
		iot->ioflags |= IO_NO_PHYSICS_INTERPOL;
		setNextAnim(iot, iot->anims[num], layer, loop, nointerpol);
		
		if(!loop) {
			CheckSetAnimOutOfTreatZone(iot, layer);
		}
		
		if(iot == entities.player()) {
			layer.flags &= ~EA_STATICANIM;
		}
		
		if(execute) {
			
			size_t pos = context.skipCommand();
			if(pos == size_t(-1)) {
				ScriptWarning << "used -e flag without command to execute";
				return Success;
			}
			
			std::string timername = getDefaultScriptTimerName(context.getEntity(), "anim_timer");
			
			SCR_TIMER & timer = createScriptTimer(context.getEntity(), std::move(timername));
			timer.es = context.getScript();
			timer.interval = 1s;
			// Don't assume that we successfully set the animation - use the current animation
			if(layer.cur_anim) {
				arx_assert(layer.altidx_cur < layer.cur_anim->anims.size());
				if(layer.currentAltAnim()->anim_time > toAnimationDuration(timer.interval)) {
					timer.interval = toGameDuration(layer.currentAltAnim()->anim_time);
				}
			}
			timer.pos = pos;
			timer.start = g_gameTime.now();
			timer.count = 1;
			
			DebugScript(": scheduled timer " << timer.name << " in " << toMsi(timer.interval) << "ms");
			
		}
		
		return Success;
	}
	
};

class LoadAnimCommand : public Command {
	
public:
	
	LoadAnimCommand() : Command("loadanim") { }
	
	Result execute(Context & context) override {
		
		Entity * iot = context.getEntity();
		
		HandleFlags("p") {
			if(flg & flag('p')) {
				iot = entities.player();
			}
		}
		
		std::string anim = context.getWord();
		
		res::path file = res::path::load(context.getWord());
		
		DebugScript(' ' << options << ' ' << anim << ' ' << file);
		
		
		if(!iot) {
			ScriptWarning << "must either use -p or use with IO";
			return Failed;
		}
		
		AnimationNumber num = getAnimationNumber(anim);
		if(num == ANIM_NONE) {
			ScriptWarning << "unknown anim: " << anim;
			return Failed;
		}
		
		if(iot->anims[num]) {
			ReleaseAnimFromIO(iot, num);
		}
		
		if(file == "none") {
			iot->anims[num] = nullptr;
			return Success;
		}
		
		res::path path;
		if(iot == entities.player() || (iot->ioflags & IO_NPC)) {
			path = ("graph/obj3d/anims/npc" / file).set_ext("tea");
		} else {
			path = ("graph/obj3d/anims/fix_inter" / file).set_ext("tea");
		}
		
		iot->anims[num] = EERIE_ANIMMANAGER_Load_NoWarning(path);
		
		if(!iot->anims[num]) {
			ScriptWarning << "animation not found: " << path;
			return Failed;
		}
		
		return Success;
	}
	
};

class MoveCommand : public Command {
	
public:
	
	MoveCommand() : Command("move", AnyEntity) { }
	
	Result execute(Context & context) override {
		std::string strEntId;
		
		HandleFlags("e") {
			if(flg & flag('e')) {
				strEntId = context.getStringVar(context.getWord());
			}
		}
		
		float dx = context.getFloat();
		float dy = context.getFloat();
		float dz = context.getFloat();
		
		DebugScript(' ' << strEntId << ' ' << dx << ' ' << dy << ' ' << dz);
		
		Entity * entity = context.getEntity();
		if(strEntId != "") {
			entity = entities.getById(strEntId);
			if(!entity) {
				ScriptWarning << "invalid entity id " << strEntId;
				return Failed;
			}
		}
		
		entity->pos += Vec3f(dx, dy, dz);
		
		return Success;
	}
	
};

class InterpolateCommand : public Command {
	
public:
	
	InterpolateCommand() : Command("interpolate", AnyEntity) { }
	
	void interpretLocation(Vec3f & pos, const std::string_view & strPos){ //TODO static global reuse with ^dist_{x,y,z}, place at ScriptUtils probably
		int iStrPosIni=0;
		int iStrPosNext=strPos.find(',',iStrPosIni);
		pos.x = util::parseFloat(strPos.substr(iStrPosIni,iStrPosNext-iStrPosIni));
		
		iStrPosIni=iStrPosNext+1; //skip ','
		iStrPosNext=strPos.find(',',iStrPosIni);
		pos.y = util::parseFloat(strPos.substr(iStrPosIni,iStrPosNext-iStrPosIni));
		
		iStrPosIni=iStrPosNext+1; //skip ','
		pos.z = util::parseFloat(strPos.substr(iStrPosIni));
	}
	
	std::string vec3fToStr(Vec3f & v3) {
		std::string s;
		s+=std::to_string(v3.x)+",";
		s+=std::to_string(v3.y)+",";
		s+=std::to_string(v3.z);
		return s;
	}
	
	/**
	 * INTERPOLATE [-flsp] <EntityToMove> <f?FromLocation> <TargetLocation|TargetEntity> <NearDist|PercentDist|StepDist>
	 *  <EntityToMove>: (entityID) is who will be moved
	 *  [-l]: no limits, allows negative distance and distance bigger than max distance
	 *  [-f]: FromLocation: x,y,z is the absolute position to move from ignoring EntityToMove position
	 *  [-s]: StepDist: will move this distance from where it is towards target. if negative, will move away instead.
	 *  [-p]: PercentDist: Near dist but as percent. From 1.0 to 0.0 will be a percent of the distance between them, where 0.0 is at target. A negative percent will move past the target. A positive will move away the target.
	 *  NearDist: if not -p and not -s. If bigger than the distance between the from and target location, allows moving farer instead of nearer, and if negative it can move past the target location.
	 *  <TargetLocation|TargetEntity>: TargetLocation: x,y,z is the absolute target position; TargetEntity: (entityID) is the target entity to get the position
	 * TODO: put this comment on wiki instead (keep here too would be better tho, but then it would be easier to format here how it would be required on wiki, so we could just copy/paste there)
	 */
	Result execute(Context & context) override {
		Entity * entToMove = nullptr;
		Entity * entTarget = nullptr;
		Vec3f posTarget = Vec3f(0.f);
		Vec3f posFrom = Vec3f(0.f);
		Vec3f posRequested = Vec3f(0.f);
		bool bLimitDist = true;
		bool bAbsPosFrom = false;
		bool bPosTarget = false;
		float fContextDist = 0.f;
		char cDistMode = 'n'; //NearDist
		
		HandleFlags("flsp") {
			if(flg & flag('f')) {
				bAbsPosFrom = true;
			}
			if(flg & flag('l')) {
				bLimitDist=false;
			}
			if(flg & flag('s')) {
				cDistMode = 's';
			}
			if(flg & flag('p')) {
				cDistMode = 'p';
			}
		}
		
		std::string strEntityToMove = context.getWord();
		if(strEntityToMove[0] == '$' || strEntityToMove[0] == '\xA3') strEntityToMove = context.getStringVar(strEntityToMove);
		entToMove = strEntityToMove=="self" ? context.getEntity() : entities.getById(strEntityToMove);
		
		//optional from absolute position
		if(bAbsPosFrom){
			//param:FromLocation
			interpretLocation(posFrom, context.getWord());
		} else {
			if(entToMove) {
				posFrom = entToMove == entities.player() ? entities.player()->pos : GetItemWorldPosition(entToMove);
			}
		}
		
		std::string strTarget = context.getWord();
		if(boost::contains(strTarget,",")){ //absolute location
			//param:TargetLocation
			interpretLocation(posTarget,strTarget);
			bPosTarget = true;
		}else{
			//param:TargetEntity
			if(strTarget[0] == '$' || strTarget[0] == '\xA3') strTarget = context.getStringVar(strTarget);
			entTarget = strTarget=="self" ? context.getEntity() : entities.getById(strTarget);
			if(entTarget) {
				posTarget = entTarget == entities.player() ? entities.player()->pos : GetItemWorldPosition(entTarget);
			}
		}
		
		fContextDist = context.getFloat();
		
		///////////////////////////////////////////////////////////////
		// !!! ATTENTION !!!
		// can only return after all params have been collected to not break the remainder of the script!
		// return failure first from null pointers that are a better warn info than others.
		///////////////////////////////////////////////////////////////
		
		if(!entToMove) {
			ScriptWarning << "null EntityToMove " << strEntityToMove;
			return Failed;
		}
		
		if(!bPosTarget){
			if(!entTarget) {
				ScriptWarning << "null TargetEntity " << strTarget;
				return Failed;
			}
		}
			
		if(!bPosTarget){
			if(entToMove == entTarget){
				ScriptWarning << "EntityToMove and TargetEntity are the same";
				return Failed;
			}
		}
		
		if(posFrom==posTarget){ //already there
			return Success;
		}
		
		float fDistMax = fdist(posFrom, posTarget);
		if(fDistMax < 0.f) { //TODO explain how fdist() negative can happen
			fDistMax = 0.f;
		}
		
		float fDistRequested = 0.f;
		switch(cDistMode) {
			case 's':
				if(fContextDist == 0.f){ //wont move at all
					ScriptWarning << "step distance is 0, wont move at all";
					return Failed; //because some movement should be happening, it is a step after all.
				}
				
				if(bLimitDist) {
					if(fContextDist < 0.f) {
						return Success; //limit requested so just accept it, wont move at all
					}
					if(fContextDist > fDistMax) {
						posRequested = posTarget;
					}
				}
				
				fDistRequested = fDistMax - fContextDist;
				
				break;
				
			case 'p':
				if(fContextDist == 1.0f){ //wont move at all, is 100% far away
					return Success;
				}
				if(bLimitDist) {
					if(fContextDist > 1.0f){
						return Success; //limit requested so just accept it, wont move at all, is limited to max perc dist
					} else if(fContextDist < 0.f){
						fContextDist = 0.f;
					}
				}
				
				fDistRequested = fDistMax * fContextDist;
				
				if(fDistRequested == 0) {
					posRequested = posTarget;
				}
				
				break;
				
			case 'n':
				if(fContextDist == fDistMax) { //wont move at all, is at max dist
					return Success;
				}
				
				if(bLimitDist) {
					if(fContextDist > fDistMax){
						return Success; //limit requested so just accept it, wont move at all, is limited to max dist
					}
					if(fContextDist < 0.f){
						fContextDist = 0.f; //fix the dist by limit request
					}
				}
				
				fDistRequested = fContextDist;
				
				break;
				
		}
		
		if((cDistMode=='p' || cDistMode=='n') && (fContextDist == 0.f)) { //NearTargetDistance: will just be placed at target location
			posRequested = posTarget;
		}
		
		if(posRequested != posTarget) {
			Vec3f delta = posFrom - posTarget;
			float fDeltaNorm = ffsqrt( //TODO compare results with LegacyMath.h:interpolatePos() ?
				arx::pow2(posFrom.x - posTarget.x) + //glm::pow(
				arx::pow2(posFrom.y - posTarget.y) + 
				arx::pow2(posFrom.z - posTarget.z)   );
			float fDistPerc=fDistRequested/fDeltaNorm;
			posRequested = posTarget + (fDistPerc*delta);
		}
		
		DebugScript("posRequested=" << vec3fToStr(posRequested) << ", fContextDist=" << fContextDist);
		
		ARX_INTERACTIVE_TeleportSafe(entToMove, posRequested);
		
		LogDebug("INTERPOLATE(): strEntityToMove="<<strEntityToMove <<",strTarget="<<strTarget <<",entToMoveId="<<entToMove->idString() <<",entTargetId="<<(entTarget?entTarget->idString():"null") <<",posTarget="<< vec3fToStr(posTarget)<<",posFrom="<< vec3fToStr(posFrom)<<",fDistMax="<<fDistMax<<",posRequested="<< vec3fToStr(posRequested)<<",bLimitDist="<< bLimitDist<<",bAbsPosFrom="<<bAbsPosFrom <<",bPosTarget="<< bPosTarget<<",fContextDist="<< fContextDist);
		
		return Success;
	}

};

class UsePathCommand : public Command {
	
public:
	
	UsePathCommand() : Command("usepath", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string type = context.getWord();
		
		DebugScript(' ' << type);
		
		ARX_USE_PATH * aup = context.getEntity()->usepath;
		if(!aup) {
			ScriptWarning << "no path set";
			return Failed;
		}
		
		if(type == "b") {
			aup->aupflags &= ~ARX_USEPATH_PAUSE;
			aup->aupflags &= ~ARX_USEPATH_FORWARD;
			aup->aupflags |= ARX_USEPATH_BACKWARD;
		} else if(type == "f") {
			aup->aupflags &= ~ARX_USEPATH_PAUSE;
			aup->aupflags |= ARX_USEPATH_FORWARD;
			aup->aupflags &= ~ARX_USEPATH_BACKWARD;
		} else if(type == "p") {
			aup->aupflags |= ARX_USEPATH_PAUSE;
			aup->aupflags &= ~ARX_USEPATH_FORWARD;
			aup->aupflags &= ~ARX_USEPATH_BACKWARD;
		} else {
			ScriptWarning << "unknown usepath type: " << type;
			return Failed;
		}
		
		return Success;
	}
	
};

class UnsetControlledZoneCommand : public Command {
	
public:
	
	UnsetControlledZoneCommand() : Command("unsetcontrolledzone") { }
	
	Result execute(Context & context) override {
		
		std::string zone = context.getWord();
		
		DebugScript(' ' << zone);
		
		Zone * ap = getZoneByName(zone);
		if(!ap) {
			ScriptWarning << "unknown zone: " << zone;
			return Failed;
		}
		
		ap->controled.clear();
		
		return Success;
	}
	
};

class SetPathCommand : public Command {
	
public:
	
	SetPathCommand() : Command("setpath", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		bool wormspecific = false;
		bool followdir = false;
		HandleFlags("wf") {
			wormspecific = test_flag(flg, 'w');
			followdir = test_flag(flg, 'f');
		}
		
		std::string name = context.getWord();
		
		DebugScript(' ' << options << ' ' << name);
		
		Entity * io = context.getEntity();
		if(name == "none") {
			delete io->usepath;
			io->usepath = nullptr;
		} else {
			
			const Path * ap = getPathByName(name);
			if(!ap) {
				ScriptWarning << "unknown path: " << name;
				return Failed;
			}
			
			delete io->usepath;
			io->usepath = nullptr;
			
			ARX_USE_PATH * aup = new ARX_USE_PATH;
			aup->_starttime = aup->_curtime = g_gameTime.now();
			aup->aupflags = ARX_USEPATH_FORWARD;
			if(wormspecific) {
				aup->aupflags |= ARX_USEPATH_WORM_SPECIFIC | ARX_USEPATH_FLAG_ADDSTARTPOS;
			}
			if(followdir) {
				aup->aupflags |= ARX_USEPATH_FOLLOW_DIRECTION;
			}
			aup->lastWP = -1;
			aup->path = ap;
			io->usepath = aup;
		}
		
		return Success;
	}
	
};

class SetControlledZoneCommand : public Command {
	
public:
	
	SetControlledZoneCommand() : Command("setcontrolledzone", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string name = context.getWord();
		
		DebugScript(' ' << name);
		
		Zone * ap = getZoneByName(name);
		if(!ap) {
			ScriptWarning << "unknown zone: " << name;
			return Failed;
		}
		
		ap->controled = context.getEntity()->idString();
		
		return Success;
	}
	
};

void initAnimationNumbers() {
	
	animations["wait"] = ANIM_WAIT;
	animations["wait2"] = ANIM_WAIT2;
	animations["walk"] = ANIM_WALK;
	animations["walk1"] = ANIM_WALK;
	animations["walk2"] = ANIM_WALK2;
	animations["walk3"] = ANIM_WALK3;
	animations["walk_backward"] = ANIM_WALK_BACKWARD;
	animations["walk_ministep"] = ANIM_WALK_MINISTEP;
	animations["wait_short"] = ANIM_WAIT_SHORT;
	animations["walk_sneak"] = ANIM_WALK_SNEAK;
	animations["action"] = ANIM_ACTION;
	animations["action1"] = ANIM_ACTION;
	animations["action2"] = ANIM_ACTION2;
	animations["action3"] = ANIM_ACTION3;
	animations["action4"] = ANIM_ACTION4;
	animations["action5"] = ANIM_ACTION5;
	animations["action6"] = ANIM_ACTION6;
	animations["action7"] = ANIM_ACTION7;
	animations["action8"] = ANIM_ACTION8;
	animations["action9"] = ANIM_ACTION9;
	animations["action10"] = ANIM_ACTION10;
	animations["hit1"] = ANIM_HIT1;
	animations["hit"] = ANIM_HIT1;
	animations["hold_torch"] = ANIM_HOLD_TORCH;
	animations["hit_short"] = ANIM_HIT_SHORT;
	animations["strike1"] = ANIM_STRIKE1;
	animations["strike"] = ANIM_STRIKE1;
	animations["shield_start"] = ANIM_SHIELD_START;
	animations["shield_cycle"] = ANIM_SHIELD_CYCLE;
	animations["shield_hit"] = ANIM_SHIELD_HIT;
	animations["shield_end"] = ANIM_SHIELD_END;
	animations["strafe_right"] = ANIM_STRAFE_RIGHT;
	animations["strafe_left"] = ANIM_STRAFE_LEFT;
	animations["strafe_run_left"] = ANIM_STRAFE_RUN_LEFT;
	animations["strafe_run_right"] = ANIM_STRAFE_RUN_RIGHT;
	animations["die"] = ANIM_DIE;
	animations["dagger_ready_part_1"] = ANIM_DAGGER_READY_PART_1;
	animations["dagger_ready_part_2"] = ANIM_DAGGER_READY_PART_2;
	animations["dagger_unready_part_1"] = ANIM_DAGGER_UNREADY_PART_1;
	animations["dagger_unready_part_2"] = ANIM_DAGGER_UNREADY_PART_2;
	animations["dagger_wait"] = ANIM_DAGGER_WAIT;
	animations["dagger_strike_left_start"] = ANIM_DAGGER_STRIKE_LEFT_START;
	animations["dagger_strike_left_cycle"] = ANIM_DAGGER_STRIKE_LEFT_CYCLE;
	animations["dagger_strike_left"] = ANIM_DAGGER_STRIKE_LEFT;
	animations["dagger_strike_right_start"] = ANIM_DAGGER_STRIKE_RIGHT_START;
	animations["dagger_strike_right_cycle"] = ANIM_DAGGER_STRIKE_RIGHT_CYCLE;
	animations["dagger_strike_right"] = ANIM_DAGGER_STRIKE_RIGHT;
	animations["dagger_strike_top_start"] = ANIM_DAGGER_STRIKE_TOP_START;
	animations["dagger_strike_top_cycle"] = ANIM_DAGGER_STRIKE_TOP_CYCLE;
	animations["dagger_strike_top"] = ANIM_DAGGER_STRIKE_TOP;
	animations["dagger_strike_bottom_start"] = ANIM_DAGGER_STRIKE_BOTTOM_START;
	animations["dagger_strike_bottom_cycle"] = ANIM_DAGGER_STRIKE_BOTTOM_CYCLE;
	animations["dagger_strike_bottom"] = ANIM_DAGGER_STRIKE_BOTTOM;
	animations["death_critical"] = ANIM_DEATH_CRITICAL;
	animations["run"] = ANIM_RUN;
	animations["run1"] = ANIM_RUN;
	animations["run2"] = ANIM_RUN2;
	animations["run3"] = ANIM_RUN3;
	animations["run_backward"] = ANIM_RUN_BACKWARD;
	animations["talk_neutral"] = ANIM_TALK_NEUTRAL;
	animations["talk_angry"] = ANIM_TALK_ANGRY;
	animations["talk_happy"] = ANIM_TALK_HAPPY;
	animations["talk_neutral_head"] = ANIM_TALK_NEUTRAL_HEAD;
	animations["talk_angry_head"] = ANIM_TALK_ANGRY_HEAD;
	animations["talk_happy_head"] = ANIM_TALK_HAPPY_HEAD;
	animations["bare_ready"] = ANIM_BARE_READY;
	animations["bare_unready"] = ANIM_BARE_UNREADY;
	animations["bare_wait"] = ANIM_BARE_WAIT;
	animations["bare_strike_left_start"] = ANIM_BARE_STRIKE_LEFT_START;
	animations["bare_strike_left_cycle"] = ANIM_BARE_STRIKE_LEFT_CYCLE;
	animations["bare_strike_left"] = ANIM_BARE_STRIKE_LEFT;
	animations["bare_strike_right_start"] = ANIM_BARE_STRIKE_RIGHT_START;
	animations["bare_strike_right_cycle"] = ANIM_BARE_STRIKE_RIGHT_CYCLE;
	animations["bare_strike_right"] = ANIM_BARE_STRIKE_RIGHT;
	animations["bare_strike_top_start"] = ANIM_BARE_STRIKE_TOP_START;
	animations["bare_strike_top_cycle"] = ANIM_BARE_STRIKE_TOP_CYCLE;
	animations["bare_strike_top"] = ANIM_BARE_STRIKE_TOP;
	animations["bare_strike_bottom_start"] = ANIM_BARE_STRIKE_BOTTOM_START;
	animations["bare_strike_bottom_cycle"] = ANIM_BARE_STRIKE_BOTTOM_CYCLE;
	animations["bare_strike_bottom"] = ANIM_BARE_STRIKE_BOTTOM;
	animations["1h_ready_part_1"] = ANIM_1H_READY_PART_1;
	animations["1h_ready_part_2"] = ANIM_1H_READY_PART_2;
	animations["1h_unready_part_1"] = ANIM_1H_UNREADY_PART_1;
	animations["1h_unready_part_2"] = ANIM_1H_UNREADY_PART_2;
	animations["1h_wait"] = ANIM_1H_WAIT;
	animations["1h_strike_left_start"] = ANIM_1H_STRIKE_LEFT_START;
	animations["1h_strike_left_cycle"] = ANIM_1H_STRIKE_LEFT_CYCLE;
	animations["1h_strike_left"] = ANIM_1H_STRIKE_LEFT;
	animations["1h_strike_right_start"] = ANIM_1H_STRIKE_RIGHT_START;
	animations["1h_strike_right_cycle"] = ANIM_1H_STRIKE_RIGHT_CYCLE;
	animations["1h_strike_right"] = ANIM_1H_STRIKE_RIGHT;
	animations["1h_strike_top_start"] = ANIM_1H_STRIKE_TOP_START;
	animations["1h_strike_top_cycle"] = ANIM_1H_STRIKE_TOP_CYCLE;
	animations["1h_strike_top"] = ANIM_1H_STRIKE_TOP;
	animations["1h_strike_bottom_start"] = ANIM_1H_STRIKE_BOTTOM_START;
	animations["1h_strike_bottom_cycle"] = ANIM_1H_STRIKE_BOTTOM_CYCLE;
	animations["1h_strike_bottom"] = ANIM_1H_STRIKE_BOTTOM;
	animations["2h_ready_part_1"] = ANIM_2H_READY_PART_1;
	animations["2h_ready_part_2"] = ANIM_2H_READY_PART_2;
	animations["2h_unready_part_1"] = ANIM_2H_UNREADY_PART_1;
	animations["2h_unready_part_2"] = ANIM_2H_UNREADY_PART_2;
	animations["2h_wait"] = ANIM_2H_WAIT;
	animations["2h_strike_left_start"] = ANIM_2H_STRIKE_LEFT_START;
	animations["2h_strike_left_cycle"] = ANIM_2H_STRIKE_LEFT_CYCLE;
	animations["2h_strike_left"] = ANIM_2H_STRIKE_LEFT;
	animations["2h_strike_right_start"] = ANIM_2H_STRIKE_RIGHT_START;
	animations["2h_strike_right_cycle"] = ANIM_2H_STRIKE_RIGHT_CYCLE;
	animations["2h_strike_right"] = ANIM_2H_STRIKE_RIGHT;
	animations["2h_strike_top_start"] = ANIM_2H_STRIKE_TOP_START;
	animations["2h_strike_top_cycle"] = ANIM_2H_STRIKE_TOP_CYCLE;
	animations["2h_strike_top"] = ANIM_2H_STRIKE_TOP;
	animations["2h_strike_bottom_start"] = ANIM_2H_STRIKE_BOTTOM_START;
	animations["2h_strike_bottom_cycle"] = ANIM_2H_STRIKE_BOTTOM_CYCLE;
	animations["2h_strike_bottom"] = ANIM_2H_STRIKE_BOTTOM;
	animations["missile_ready_part_1"] = ANIM_MISSILE_READY_PART_1;
	animations["missile_ready_part_2"] = ANIM_MISSILE_READY_PART_2;
	animations["missile_unready_part_1"] = ANIM_MISSILE_UNREADY_PART_1;
	animations["missile_unready_part_2"] = ANIM_MISSILE_UNREADY_PART_2;
	animations["missile_wait"] = ANIM_MISSILE_WAIT;
	animations["missile_strike_part_1"] = ANIM_MISSILE_STRIKE_PART_1;
	animations["missile_strike_part_2"] = ANIM_MISSILE_STRIKE_PART_2;
	animations["missile_strike_cycle"] = ANIM_MISSILE_STRIKE_CYCLE;
	animations["missile_strike"] = ANIM_MISSILE_STRIKE;
	animations["meditation"] = ANIM_MEDITATION;
	animations["cast_start"] = ANIM_CAST_START;
	animations["cast_cycle"] = ANIM_CAST_CYCLE;
	animations["cast"] = ANIM_CAST;
	animations["cast_end"] = ANIM_CAST_END;
	animations["crouch"] = ANIM_CROUCH;
	animations["crouch_walk"] = ANIM_CROUCH_WALK;
	animations["crouch_walk_backward"] = ANIM_CROUCH_WALK_BACKWARD;
	animations["crouch_strafe_left"] = ANIM_CROUCH_STRAFE_LEFT;
	animations["crouch_strafe_right"] = ANIM_CROUCH_STRAFE_RIGHT;
	animations["crouch_start"] = ANIM_CROUCH_START;
	animations["crouch_wait"] = ANIM_CROUCH_WAIT;
	animations["crouch_end"] = ANIM_CROUCH_END;
	animations["lean_right"] = ANIM_LEAN_RIGHT;
	animations["lean_left"] = ANIM_LEAN_LEFT;
	animations["levitate"] = ANIM_LEVITATE;
	animations["jump"] = ANIM_JUMP;
	animations["jump_anticipation"] = ANIM_JUMP_ANTICIPATION;
	animations["jump_up"] = ANIM_JUMP_UP;
	animations["jump_cycle"] = ANIM_JUMP_CYCLE;
	animations["jump_end"] = ANIM_JUMP_END;
	animations["jump_end_part2"] = ANIM_JUMP_END_PART2;
	animations["fight_walk_forward"] = ANIM_FIGHT_WALK_FORWARD;
	animations["fight_walk_backward"] = ANIM_FIGHT_WALK_BACKWARD;
	animations["fight_walk_ministep"] = ANIM_FIGHT_WALK_MINISTEP;
	animations["fight_strafe_right"] = ANIM_FIGHT_STRAFE_RIGHT;
	animations["fight_strafe_left"] = ANIM_FIGHT_STRAFE_LEFT;
	animations["fight_wait"] = ANIM_FIGHT_WAIT;
	animations["grunt"] = ANIM_GRUNT;
	animations["u_turn_left"] = ANIM_U_TURN_LEFT;
	animations["u_turn_right"] = ANIM_U_TURN_RIGHT;
	animations["u_turn_left_fight"] = ANIM_U_TURN_LEFT_FIGHT;
	animations["u_turn_right_fight"] = ANIM_U_TURN_RIGHT_FIGHT;
	
}

} // anonymous namespace

void setupScriptedAnimation() {
	
	initAnimationNumbers();
	
	ScriptEvent::registerCommand(std::make_unique<RotateCommand>());
	ScriptEvent::registerCommand(std::make_unique<ForceAnimCommand>());
	ScriptEvent::registerCommand(std::make_unique<ForceAngleCommand>());
	ScriptEvent::registerCommand(std::make_unique<PlayAnimCommand>());
	ScriptEvent::registerCommand(std::make_unique<LoadAnimCommand>());
	ScriptEvent::registerCommand(std::make_unique<MoveCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetControlledZoneCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetPathCommand>());
	ScriptEvent::registerCommand(std::make_unique<UsePathCommand>());
	ScriptEvent::registerCommand(std::make_unique<UnsetControlledZoneCommand>());
	ScriptEvent::registerCommand(std::make_unique<InterpolateCommand>());
	
}

} // namespace script
