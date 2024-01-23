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

#include "script/ScriptedNPC.h"

#include <boost/algorithm/string/predicate.hpp>

#include "game/Camera.h"
#include "game/EntityManager.h"
#include "game/NPC.h"
#include "graphics/Math.h"
#include "graphics/data/Mesh.h"
#include "scene/Interactive.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"
#include "util/Cast.h"


namespace script {

namespace {

class BehaviourCommand : public Command {
	
public:
	
	/**
	 * behavior [-elsdmfa] [e?<applyAtEntityID>] <command>
	 * -e <applyAtEntityID> uses specified entity to apply this command instead of the caller
	 * -l sets flag BEHAVIOUR_LOOK_AROUND
	 * -s sets flag BEHAVIOUR_SNEAK
	 * -d sets flag BEHAVIOUR_DISTANT
	 * -m sets flag BEHAVIOUR_MAGIC
	 * -f sets flag BEHAVIOUR_FIGHT
	 * -a sets flag BEHAVIOUR_STARE_AT
	 * <command> commands:
	 *  stack|unstack|unstackall|go_home|friendly|move_to|guard|none - these have no param
	 *  wander_around|hide|look_for|flee <seconds> - these require <seconds> to perform that behavior
	 */
	BehaviourCommand() : Command("behavior", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		std::string strEntId;
		Entity * io = context.getEntity();
		
		Behaviour behavior = 0;
		HandleFlags("elsdmfa012") {
			if(flg & flag('e')) { 
				strEntId = context.getStringVar(context.getWord());
				io = entities.getById(strEntId);
			}
			behavior |= (flg & flag('l')) ? BEHAVIOUR_LOOK_AROUND : Behaviour(0);
			behavior |= (flg & flag('s')) ? BEHAVIOUR_SNEAK : Behaviour(0);
			behavior |= (flg & flag('d')) ? BEHAVIOUR_DISTANT : Behaviour(0);
			behavior |= (flg & flag('m')) ? BEHAVIOUR_MAGIC : Behaviour(0);
			behavior |= (flg & flag('f')) ? BEHAVIOUR_FIGHT : Behaviour(0);
			behavior |= (flg & flag('a')) ? BEHAVIOUR_STARE_AT : Behaviour(0);
		}
		
		std::string command = context.getWord();
		
		if(!io) { //after collecting all params
			ScriptWarning << "invalid entity id " << strEntId;
			return Failed;
		}
		
		if(options.empty()) {
			if(command == "stack") {
				DebugScript(' ' << options << ' ' << command);
				ARX_NPC_Behaviour_Stack(io);
				return Success;
			}
			if(command == "unstack") {
				DebugScript(' ' << options << ' ' << command);
				ARX_NPC_Behaviour_UnStack(io);
				return Success;
			}
			if(command == "unstackall") {
				DebugScript(' ' << options << ' ' << command);
				resetNpcBehavior(*io);
				return Success;
			}
		}
		
		float behavior_param = 0.f;
		if(command == "go_home") {
			behavior |= BEHAVIOUR_GO_HOME;
		} else if(command == "friendly") {
			io->_npcdata->movemode = NOMOVEMODE;
			behavior |= BEHAVIOUR_FRIENDLY;
		} else if(command == "move_to") {
			io->_npcdata->movemode = WALKMODE;
			behavior |= BEHAVIOUR_MOVE_TO;
		} else if(command == "flee") {
			behavior_param = context.getFloat();
			io->_npcdata->movemode = RUNMODE;
			behavior |= BEHAVIOUR_FLEE;
		} else if(command == "look_for") {
			behavior_param = context.getFloat();
			io->_npcdata->movemode = WALKMODE;
			behavior |= BEHAVIOUR_LOOK_FOR;
		} else if(command == "hide") {
			behavior_param = context.getFloat();
			io->_npcdata->movemode = WALKMODE;
			behavior |= BEHAVIOUR_HIDE;
		} else if(command == "wander_around") {
			behavior_param = context.getFloat();
			io->_npcdata->movemode = WALKMODE;
			behavior |= BEHAVIOUR_WANDER_AROUND;
		} else if(command == "guard") {
			behavior |= BEHAVIOUR_GUARD;
			io->targetinfo = EntityHandle(TARGET_NONE);
			io->_npcdata->movemode = NOMOVEMODE;
		} else if(command != "none") {
			ScriptWarning << "unexpected command: " << options << " \"" << command << '"';
		}
		
		DebugScript(' ' << options << " \"" << command << "\" " << behavior_param);
		
		ARX_NPC_Behaviour_Change(io, behavior, util::to<long>(behavior_param));
		
		return Success;
	}
	
};

class ReviveCommand : public Command {
	
public:
	
	ReviveCommand() : Command("revive", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		bool init = false;
		HandleFlags("i") {
			init = test_flag(flg, 'i');
		}
		
		DebugScript(' ' << options);
		
		ARX_NPC_Revive(context.getEntity(), init);
		
		return Success;
	}
	
};

class SpellcastCommand : public Command {
	
public:
	
	SpellcastCommand() : Command("spellcast", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		SpellcastFlags spflags = 0;
		GameDuration duration;
		bool haveDuration = false;
		
		if((context.getEntity()->ioflags & IO_ITEM)
		   && boost::starts_with(context.getEntity()->id().className(), "potion_")) {
			// TODO(patch-scripts) Workaround for http://arx.vg/1048
			spflags |= SPELLCAST_FLAG_ORPHAN;
		}
		
		HandleFlags("kdxmsfzo") {
			
			if(flg & flag('k')) {
				
				std::string spellname = context.getWord();
				SpellType spellid = GetSpellId(spellname);
				
				DebugScript(' ' << options << ' ' << spellname);
				
				if(context.getEntity()) {
					spells.endByCaster(context.getEntity()->index(), spellid);
				}
				
				return Success;
			}
			
			if(flg & flag('d')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST;
				duration = std::chrono::milliseconds(long(context.getFloat()));
				if(duration <= 0) {
					duration = std::chrono::milliseconds(99999999); // TODO should this be GameDuration::max()?
				}
				haveDuration = true;
			}
			if(flg & flag('x')) {
				spflags |= SPELLCAST_FLAG_NOSOUND;
			}
			if(flg & flag('m')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST | SPELLCAST_FLAG_NODRAW;
			}
			if(flg & flag('s')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST | SPELLCAST_FLAG_NOANIM;
			}
			if(flg & flag('f')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST | SPELLCAST_FLAG_NOMANA;
			}
			if(flg & flag('z')) {
				spflags |= SPELLCAST_FLAG_RESTORE;
			}
			if(flg & flag('o')) {
				spflags |= SPELLCAST_FLAG_ORPHAN;
			}
		}
		
		long level = glm::clamp(static_cast<long>(context.getFloat()), 1l, 10l);
		if(!haveDuration) {
			duration = 1s + level * 2s;
		}
		
		std::string spellname = context.getWord();
		SpellType spellid = GetSpellId(spellname);
		
		std::string target = context.getWord();
		Entity * t = entities.getById(target, context.getEntity());
		if(!t) {
			// Some scripts have a bogus (or no) target for spellcast commands.
			// The original game used the current entity in that case, so we must as well.
			t = context.getEntity();
		}
		
		if(!t || spellid == SPELL_NONE) {
			return Failed;
		}
		
		if(context.getEntity() != entities.player()) {
			spflags |= SPELLCAST_FLAG_NOCHECKCANCAST;
		}
		
		if(spellid == SPELL_LIGHTNING_STRIKE && context.getEntity()->idString() == "intro_draw_0001") {
			// TODO(patch-scripts) Workaround for http://arx.vg/93
			spflags |= SPELLCAST_FLAG_NODAMAGE;
		}
		
		DebugScript(' ' << spellname << ' ' << level << ' ' << target << ' ' << spflags << ' '
		            << std::chrono::milliseconds(duration).count());
		
		TryToCastSpell(context.getEntity(), spellid, level, t->index(), spflags, duration);
		
		return Success;
	}
	
};

class SetDetectCommand : public Command {
	
public:
	
	SetDetectCommand() : Command("setdetect", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		std::string detectvalue = context.getWord();
		
		DebugScript(' ' << detectvalue);
		
		if(detectvalue == "off") {
			context.getEntity()->_npcdata->fDetect = -1;
		} else {
			context.getEntity()->_npcdata->fDetect = glm::clamp(int(context.getFloatVar(detectvalue)), -1, 100);
		}
		
		return Success;
	}
	
};

class SetBloodCommand : public Command {
	
public:
	
	SetBloodCommand() : Command("setblood", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		float r = context.getFloat();
		float g = context.getFloat();
		float b = context.getFloat();
		
		DebugScript(' ' << r << ' ' << g << ' ' << b);
		
		context.getEntity()->_npcdata->blood_color = Color::rgb(r, g, b);
		
		return Success;
	}
	
};

class SetSpeedCommand : public Command {
	
public:
	
	SetSpeedCommand() : Command("setspeed", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		float speed = glm::clamp(context.getFloat(), 0.f, 10.f);
		
		DebugScript(' ' << speed);
		
		context.getEntity()->basespeed = speed;
		
		return Success;
	}
	
};

class SetStareFactorCommand : public Command {
	
public:
	
	SetStareFactorCommand() : Command("setstarefactor", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		float stare_factor = context.getFloat();
		
		DebugScript(' ' << stare_factor);
		
		context.getEntity()->_npcdata->stare_factor = stare_factor;
		
		return Success;
	}
	
};

class SetNPCStatCommand : public Command {
	
public:
	
	SetNPCStatCommand() : Command("setnpcstat", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		std::string stat = context.getWord();
		float value = context.getFloat();
		
		DebugScript(' ' << stat << ' ' << value);
		
		if(!ARX_NPC_SetStat(*context.getEntity(), stat, value)) {
			ScriptWarning << "unknown stat name: " << stat << ' ' << value;
			return Failed;
		}
		
		return Success;
	}
	
};

class SetXPValueCommand : public Command {
	
public:
	
	SetXPValueCommand() : Command("setxpvalue", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		float xpvalue = context.getFloat();
		if(xpvalue < 0) {
			xpvalue = 0;
		}
		
		DebugScript(' ' << xpvalue);
		
		context.getEntity()->_npcdata->xpvalue = long(xpvalue);
		
		return Success;
	}
	
};

class SetMoveModeCommand : public Command {
	
public:
	
	/**
	 * setmovemode [-e <applyAtEntityID>] <mode>
	 */
	SetMoveModeCommand() : Command("setmovemode", IO_NPC) { }
	
	Result execute(Context & context) override {
		std::string strEntId;
		
		HandleFlags("e") {
			if(flg & flag('e')) {
				strEntId = context.getStringVar(context.getWord());
			}
		}
		
		std::string mode = context.getWord();
		
		Entity * io = strEntId == "" ? context.getEntity() : entities.getById(strEntId);
		
		if(!io) { //after collecting all params
			ScriptWarning << "invalid entity id " << strEntId;
			return Failed;
		}
		
		DebugScript(' ' << mode);
		
		if(mode == "walk") {
			ARX_NPC_ChangeMoveMode(io, WALKMODE);
		} else if(mode == "run") {
			ARX_NPC_ChangeMoveMode(io, RUNMODE);
		} else if(mode == "none") {
			ARX_NPC_ChangeMoveMode(io, NOMOVEMODE);
		} else if(mode == "sneak") {
			ARX_NPC_ChangeMoveMode(io, SNEAKMODE);
		} else {
			ScriptWarning << "unexpected mode: " << mode;
			return Failed;
		}
		
		return Success;
	}
	
};

class SetLifeCommand : public Command {
	
public:
	
	SetLifeCommand() : Command("setlife", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		float life = context.getFloat();
		
		DebugScript(' ' << life);
		
		IO_NPCDATA & npc = *context.getEntity()->_npcdata;
		
		npc.lifePool.max = npc.lifePool.current = life;
		
		return Success;
	}
	
};

class SetTargetCommand : public Command {
	
public:
	
	/**
	 * settarget [-sane] [e?<applyAtEntityID>] ["object"] <"path"|"none"|targetID>
	 * "object" - is just ignored, does not modify any result
	 * "path" - changes the target mode to TARGET_PATH
	 * "none" - changes the target mode to TARGET_NONE
	 * targetID - specifies the entity ID to be used as target
	 * -s sets PATHFIND_ONCE flag
	 * -a sets PATHFIND_ALWAYS flag
	 * -n sets PATHFIND_NO_UPDATE flag
	 * -e <applyAtEntityID> uses specified entity to apply this command instead of the caller
	 */
	SetTargetCommand() : Command("settarget", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string strEntId;
		Entity * io = context.getEntity();
		
		bool bFlagS = false;
		bool bFlagA = false;
		bool bFlagN = false;
		HandleFlags("sane") {
			if(flg & flag('s')) bFlagS=true;
			if(flg & flag('a')) bFlagA=true;
			if(flg & flag('n')) bFlagN=true;
			if(flg & flag('e')) {
				strEntId = context.getStringVar(context.getWord());
				io = entities.getById(strEntId);
			}
		}
		
		std::string target = context.getWord();
		if(target == "object") {
			target = context.getWord();
		}
		
		if(!io) { //after collecting all params
			ScriptWarning << "invalid entity id " << strEntId;
			return Failed;
		}
		
		if(io->ioflags & IO_NPC) {
			io->_npcdata->pathfind.flags &= ~(PATHFIND_ALWAYS | PATHFIND_ONCE | PATHFIND_NO_UPDATE);
		
			if( bFlagS ) {
				io->_npcdata->pathfind.flags |= PATHFIND_ONCE;
			}
			if( bFlagA ) {
				io->_npcdata->pathfind.flags |= PATHFIND_ALWAYS;
			}
			if( bFlagN ) {
				io->_npcdata->pathfind.flags |= PATHFIND_NO_UPDATE;
			}
		}
		
		EntityHandle old_target = EntityHandle(-12);
		if(io->ioflags & IO_NPC) {
			if(io->_npcdata->reachedtarget) {
				old_target = io->targetinfo;
			}
			if(io->_npcdata->behavior & (BEHAVIOUR_FLEE | BEHAVIOUR_WANDER_AROUND)) {
				old_target = EntityHandle(-12);
			}
		}
		
		target = context.getStringVar(target);
		Entity * entTarget = entities.getById(target, io);
		
		DebugScript(' ' << options << ' ' << target);
		
		if(io->ioflags & IO_CAMERA) {
			io->_camdata->translatetarget = Vec3f(0.f);
		}
		
		EntityHandle ehIndex = EntityHandle();
		if(entTarget != nullptr) {
			ehIndex = io->targetinfo = entTarget->index();
			GetTargetPos(io);
		}
		
		if(target == "path") {
			io->targetinfo = EntityHandle(TARGET_PATH);
			GetTargetPos(io);
		} else if(target == "none") {
			io->targetinfo = EntityHandle(TARGET_NONE);
		}
		
		if(old_target != ehIndex) {
			if(io->ioflags & IO_NPC) {
				io->_npcdata->reachedtarget = 0;
			}
			ARX_NPC_LaunchPathfind(io, ehIndex);
		}
		
		return Success;
	}
	
};

class ForceDeathCommand : public Command {
	
public:
	
	ForceDeathCommand() : Command("forcedeath", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string target = context.getWord();
		
		DebugScript(' ' << target);
		
		Entity * t = entities.getById(target, context.getEntity());
		if(!t) {
			ScriptWarning << "unknown target: " << target;
			return Failed;
		}
		
		ARX_DAMAGES_ForceDeath(*t, context.getEntity());
		
		return Success;
	}
	
};

class PathfindCommand : public Command {
	
public:
	
	PathfindCommand() : Command("pathfind", IO_NPC) { }
	
	Result execute(Context & context) override {
		
		std::string target = context.getWord();
		
		DebugScript(' ' << target);
		
		ARX_NPC_LaunchPathfind(context.getEntity(), entities.getIndexById(target));
		
		return Success;
	}
	
};

} // anonymous namespace

void setupScriptedNPC() {
	
	ScriptEvent::registerCommand(std::make_unique<BehaviourCommand>());
	ScriptEvent::registerCommand(std::make_unique<ReviveCommand>());
	ScriptEvent::registerCommand(std::make_unique<SpellcastCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetDetectCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetBloodCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetSpeedCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetStareFactorCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetNPCStatCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetXPValueCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetMoveModeCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetLifeCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetTargetCommand>());
	ScriptEvent::registerCommand(std::make_unique<ForceDeathCommand>());
	ScriptEvent::registerCommand(std::make_unique<PathfindCommand>());
	
}

} // namespace script
