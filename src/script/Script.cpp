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
// Code: Cyril Meynier
//
// Copyright (c) 1999-2000 ARKANE Studios SA. All rights reserved

#include "script/Script.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <stddef.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include "ai/Paths.h"

#include "cinematic/CinematicController.h"

#include "core/GameTime.h"
#include "core/Core.h"
#include "core/Config.h"
#include "core/FpsCounter.h"

#include "game/Camera.h"
#include "game/Damage.h"
#include "game/EntityManager.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Item.h"
#include "game/NPC.h"
#include "game/Player.h"

#include "gui/Dragging.h"
#include "gui/Interface.h"
#include "gui/Speech.h"
#include "gui/hud/SecondaryInventory.h"

#include "graphics/particle/ParticleEffects.h"
#include "graphics/Math.h"

#include "io/resource/PakReader.h"
#include "io/log/Logger.h"

#include "platform/Dialog.h"
#include "platform/Process.h"
#include "platform/Thread.h"
#include "platform/profiler/Profiler.h"

#include "scene/Scene.h"
#include "scene/Interactive.h"

#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"

#include "util/Number.h"
#include "util/String.h"


extern long lChangeWeapon;
extern Entity * pIOChangeWeapon;

Entity * LASTSPAWNED = nullptr;
SCRIPT_VARIABLES svar;

long FORBID_SCRIPT_IO_CREATION = 0;
std::vector<SCR_TIMER> g_scriptTimers;
static size_t g_activeScriptTimers = 0;

bool isLocalVariable(std::string_view name) {
	
	arx_assert(!name.empty());
	
	switch(name[0]) {
		case '\xA3': return true;
		case '\xA7': return true;
		case '@':    return true;
		default:     return false;
	}
	
}

std::ostream & operator<<(std::ostream & os, const SCRIPT_VAR & var) {
	
	arx_assert(!var.name.empty());
	
	os << var.name << " = ";
	
	switch(var.name[0]) {
		
		case '$':
		case '\xA3': {
			os << '\"' << var.text << '\"';
			break;
		}
		
		case '#':
		case '\xA7': {
			os << var.ival;
			break;
		}
		
		case '&':
		case '@': {
			os << var.fval;
			break;
		}
		
		default: {
			os << "(unknown variable type)" << var;
			break;
		}
		
	}
	
	return os;
}

ScriptEventName ScriptEventName::parse(std::string_view name) {
	
	for(size_t i = 1; i < SM_MAXCMD; i++) {
		std::string_view event = ScriptEvent::name(ScriptMessage(i));
		arx_assert(boost::starts_with(event, "on "));
		if(event.substr(3) == name) {
			return ScriptEventName(ScriptMessage(i));
		}
	}
	
	return ScriptEventName(name);
}

std::string_view ScriptEventName::toString() const noexcept {
	
	if(!getName().empty()) {
		arx_assert(getId() == SM_NULL);
		return getName();
	}
	
	std::string_view name = ScriptEvent::name(getId());
	arx_assert(boost::starts_with(name, "on "));
	return name.substr(3);
}

DisabledEvents ScriptEventName::toDisabledEventsMask() const noexcept {
	
	switch(getId()) {
		case SM_COLLIDE_NPC: return DISABLE_COLLIDE_NPC;
		case SM_CHAT: return DISABLE_CHAT;
		case SM_HIT: return DISABLE_HIT;
		case SM_INVENTORY2_OPEN: return DISABLE_INVENTORY2_OPEN;
		case SM_HEAR: return DISABLE_HEAR;
		case SM_UNDETECTPLAYER: return DISABLE_DETECT;
		case SM_DETECTPLAYER: return DISABLE_DETECT;
		case SM_AGGRESSION: return DISABLE_AGGRESSION;
		case SM_MAIN: return DISABLE_MAIN;
		case SM_CURSORMODE: return DISABLE_CURSORMODE;
		case SM_EXPLORATIONMODE: return DISABLE_EXPLORATIONMODE;
		default: return 0;
	}
	
}

std::ostream & operator<<(std::ostream & os, const ScriptEventName & event) {
	
	if(event == SM_EXECUTELINE) {
		return os << "executeline";
	}
	if(event == SM_DUMMY)  {
		return os << "dummy event";
	}
	if(!event.getName().empty()) {
		return os << "on " << event.getName() << " event";
	}
	
	std::string_view name = ScriptEvent::name(event.getId());
	arx_assert(boost::starts_with(name, "on "));
	return os << name << " event";
}

ScriptParameters ScriptParameters::parse(std::string_view str) {
	
	ScriptParameters result;
	
	if(str.empty()) {
		return result;
	}
	
	for(size_t start = 0; start < str.length(); ) {
		
		size_t end = str.find(' ', start);
		if(end == std::string_view::npos) {
			end = str.length();
		}
		
		result.emplace_back(str.substr(start, end - start));
		
		start = end + 1;
	}
	
	return result;
}

std::ostream & operator<<(std::ostream & os, const ScriptParameters & parameters) {
	
	os << '"';
	if(!parameters.empty()) {
		os << parameters[0];
		for(size_t i = 1; i < parameters.size(); i++) {
			os << ' ' << parameters[i];
		}
	}
	os << '"';
	
	return os;
}

size_t FindScriptPos(const EERIE_SCRIPT * es, std::string_view str) {
	
	// TODO(script-parser) remove, respect quoted strings
	
	if(str.size() >= 2 && str[0] == '>' && str[1] == '>') { // uses the cache only for GoTo/GoSub calls
		auto it = es->shortcutCalls.find(std::string(str));
		if(it != es->shortcutCalls.end()) {
			return it->second;
		}
	}
	
	for(size_t pos = 0; pos < es->data.size(); pos++) {
		
		pos = es->data.find(str, pos);
		if(pos == std::string::npos || pos + str.length() >= es->data.size()) {
			return size_t(-1);
		}
		
		if(u8(es->data[pos + str.length()]) > 32) {
			continue;
		}
		
		// Check if the line is commented out!
		if(script::seekBackwardsForCommentToken(es->data, pos) == size_t(-1)) {
			return pos + str.length();
		}
		//for(size_t p = pos; es->data[p] != '/' || es->data[p + 1] != '/'; p--) {
			//if(es->data[p] == '\n' || p == 0) {
				//return pos + str.length();
			//}
		//}
		
	}
	
	return size_t(-1);
}

ScriptResult SendMsgToAllIO(Entity * sender, const ScriptEventName & event,
                            const ScriptParameters & parameters) {
	
	ScriptResult ret = ACCEPT;
	
	for(Entity & entity : entities) {
		if(SendIOScriptEvent(sender, &entity, event, parameters) == REFUSE) {
			ret = REFUSE;
		}
	}
	
	return ret;
}

void ARX_SCRIPT_ResetObject(Entity * io, bool init) {
	
	if(!io)
		return;
	
	io->m_disabledEvents = 0;
	
	// Now go for Script INIT/RESET depending on Mode
	EntityHandle num = io->index();
	
	if(entities[num] && entities[num]->script.valid) {
		if(init) {
			ScriptEvent::send(&entities[num]->script, nullptr, entities[num], SM_INIT);
		}
		if(entities[num]) {
			entities[num]->mainevent = SM_MAIN;
		}
	}
	
	// Do the same for Local Script
	if(entities[num] && entities[num]->over_script.valid && init) {
		ScriptEvent::send(&entities[num]->over_script, nullptr, entities[num], SM_INIT);
	}
	
	// Sends InitEnd Event
	if(init) {
		if(entities[num] && entities[num]->script.valid) {
			ScriptEvent::send(&entities[num]->script, nullptr, entities[num], SM_INITEND);
		}
		if(entities[num] && entities[num]->over_script.valid) {
			ScriptEvent::send(&entities[num]->over_script, nullptr, entities[num], SM_INITEND);
		}
	}
	
	if(entities[num]) {
		entities[num]->gameFlags &= ~GFLAG_NEEDINIT;
	}
	
}

void ARX_SCRIPT_Reset(Entity * io, bool init) {
	
	// Release Script Over-Script Local Variables
	io->m_variables.clear();
	
	if(!io->scriptload) {
		ARX_SCRIPT_ResetObject(io, init);
	}
	
}

void ARX_SCRIPT_ResetAll(bool init) {
	for(Entity & entity : entities) {
		if(!entity.scriptload) {
			ARX_SCRIPT_Reset(&entity, init);
		}
	}
}

void ARX_SCRIPT_AllowInterScriptExec() {
	
	ARX_PROFILE_FUNC();
	
	// FIXME static local variable
	static long ppos = 0;
	
	if(g_gameTime.isPaused()) {
		return;
	}
	
	long heartbeat_count = std::min(long(entities.size()), 10l);
	
	for(long n = 0; n < heartbeat_count; n++) {
		
		EntityHandle i = EntityHandle(ppos++);
		if(i.handleData() >= long(entities.size())) {
			ppos = 0;
			return;
		}
		
		if(entities[i] == nullptr || !(entities[i]->gameFlags & GFLAG_ISINTREATZONE)) {
			continue;
		}
		
		// Copy the even name to a local variable as it may change during execution
		// and cause unexpected behavior in SendIOScriptEvent
		ScriptEventName event = entities[i]->mainevent;
		SendIOScriptEvent(nullptr, entities[i], event);
		
	}
	
}

void ReleaseScript(EERIE_SCRIPT * es) {
	
	if(!es) {
		return;
	}
	
	es->valid = false;
	
	es->data.clear();
	
	memset(es->shortcut, 0, sizeof(es->shortcut));
	
}

static Entity * getEntityParam(std::string_view variable, size_t offset, const script::Context & context) {
	
	if(variable.length() >= offset) {
		return entities.getById(variable.substr(offset), context.getEntity());
	}
	
	return context.getEntity();
}

static Spell * getSpellParam(std::string_view variable, size_t offset) {
	
	if(variable.length() >= offset) {
		return spells.getById(variable.substr(offset));
	}
	
	return nullptr;
}

struct Date {
	std::uint16_t year;
	std::uint8_t month;
	std::uint8_t day;
};

static Date getSystemTime() {
	
	Date s_frameSystemTime = { 0, 0, 0 };
	PlatformInstant s_frameSystemTimeFrame = 0;
	
	if(s_frameSystemTimeFrame != g_platformTime.frameStart()) {
		if(config.misc.realtimeOverride.empty()) {
			std::time_t now = std::time(nullptr);
			std::tm local_tm = *std::localtime(&now);
			s_frameSystemTime.year = static_cast<std::uint16_t>(local_tm.tm_year + 1900);
			s_frameSystemTime.month = static_cast<std::uint8_t>(local_tm.tm_mon + 1);
			s_frameSystemTime.day = static_cast<std::uint8_t>(local_tm.tm_mday);
			s_frameSystemTimeFrame = g_platformTime.frameStart();
		} else {
			size_t begin = config.misc.realtimeOverride.find_first_of("123456789");
			if(begin == std::string::npos) {
				s_frameSystemTime = { 2002, 6, 28 };
			} else {
				size_t end = config.misc.realtimeOverride.find_first_not_of("0123456789", begin + 1);
				if(end == std::string::npos) {
					end = config.misc.realtimeOverride.size();
				}
				s_frameSystemTime.year = util::parseInt(std::string_view(config.misc.realtimeOverride).substr(begin, end - begin));
				begin = config.misc.realtimeOverride.find_first_of("123456789", end);
				if(begin == std::string::npos) {
					s_frameSystemTime.month = 1;
					s_frameSystemTime.day = 1;
				} else {
					end = config.misc.realtimeOverride.find_first_not_of("0123456789", begin + 1);
					if(end == std::string::npos) {
						end = config.misc.realtimeOverride.size();
					}
					s_frameSystemTime.month = util::parseInt(std::string_view(config.misc.realtimeOverride).substr(begin, end - begin));
					begin = config.misc.realtimeOverride.find_first_of("123456789", end);
					if(begin == std::string::npos) {
						s_frameSystemTime.day = 1;
					} else {
						end = config.misc.realtimeOverride.find_first_not_of("0123456789", begin + 1);
						if(end == std::string::npos) {
							end = config.misc.realtimeOverride.size();
						}
						s_frameSystemTime.day = util::parseInt(std::string_view(config.misc.realtimeOverride).substr(begin, end - begin));
					}
				}
			}
		}
	}
	
	return s_frameSystemTime;
}

static float getDegrees(const script::Context & context, const std::string_view & name, const int & offset, const char xyz) {
	Entity * entity = nullptr;
	if (offset == -1) {
		entity = context.getEntity();
	} else {
		entity = entities.getById(name.substr(offset));
	}
	
	if( !entity ){
		return 0.f;
	}
	
	float degrees = 0.f;
	switch(xyz) {
		case 'x': degrees = (entity == entities.player() ? player.angle : entity->angle).getPitch(); break;
		case 'y': degrees = (entity == entities.player() ? player.angle : entity->angle).getYaw();   break;
		case 'z': degrees = (entity == entities.player() ? player.angle : entity->angle).getRoll();  break;
		case 'Y': 
			if( !context.getEntity() ) {
				return 0.f;
			}
			degrees = Camera::getLookAtAngle(context.getEntity()->pos, entity->pos).getYaw();  
			break;
		default: LogWarning << "invalid xyz = " << xyz; break;
	}
	
	degrees = MAKEANGLE(degrees);
	
	LogDebug("name="<<name<<",xyz="<<xyz<<",deg="<<degrees<<",pitch="<<entity->angle.getPitch()<<",yaw="<<entity->angle.getYaw()<<",roll="<<entity->angle.getRoll()<<",Ppitch="<<player.angle.getPitch()<<",Pyaw="<<player.angle.getYaw()<<",Proll="<<player.angle.getRoll()<<",Cpitch="<<context.getEntity()->angle.getPitch()<<",Cyaw="<<context.getEntity()->angle.getYaw()<<",Croll="<<context.getEntity()->angle.getRoll()); //<<",fixedDeg="<<(*fcontent)
	
	return degrees;
}

static float getLocation(const std::string_view & name, const char xyz) {
	Entity * entWorkWith = entities.getById(name.substr(11));
	float f = 99999999999.f;
	if(entWorkWith && (entWorkWith->show == SHOW_FLAG_IN_SCENE || entWorkWith->show == SHOW_FLAG_IN_INVENTORY)) {
		switch(xyz) {
			case 'x': f = entWorkWith == entities.player() ? player.pos.x : GetItemWorldPosition(entWorkWith).x; break;
			case 'y': f = entWorkWith == entities.player() ? player.pos.y : GetItemWorldPosition(entWorkWith).y; break;
			case 'z': f = entWorkWith == entities.player() ? player.pos.z : GetItemWorldPosition(entWorkWith).z; break;
			default: break;
		}
	}
	return f;
}

static float getLife(const std::string_view & name, char cType, const int offset, Entity * entOverride = nullptr) {
	Entity * ent = entOverride ? entOverride : entities.getById(name.substr(offset));
	
	if( !ent ) {
		return 0.f;
	}
	
	if(ent == entities.player()) {
		switch(cType) {
			case 'c': return player.Full_life; break; //current
			case 'm': return player.m_lifeMaxWithoutMods; break; //max
			case 'M': return player.lifePool.max; break; //ModMax
			default: arx_assert_msg(false, "invalid life type for player: '%c'", cType);
		}
	} else
	if(ent->ioflags & IO_NPC) {
		switch(cType) {
			case 'c': return ent->_npcdata->lifePool.current; break; //current
			case 'm': return ent->_npcdata->lifePool.max; break; //max
			default: arx_assert_msg(false, "invalid life type for NPC: '%c'", cType);
		}
	}
	
	LogDebug("player.Full_life="<<player.Full_life<<"," <<"player.m_lifeMaxWithoutMods="<<player.m_lifeMaxWithoutMods<<"," <<"player.lifePool.max="<<player.lifePool.max<<"," <<"cType="<<cType<<"," <<"ent="<<ent<<"," <<"name="<<name<<"," <<"offset="<<offset<<"," <<"entOverride="<<entOverride<<"," );
	
	return 0.f;
}

ValueType getSystemVar(const script::Context & context, std::string_view name,
                       std::string & txtcontent, float * fcontent, long * lcontent) {
	
	arx_assert_msg(!name.empty() && name[0] == '^', "bad system variable: \"%s\"", std::string(name).c_str());
	
	char c = (name.length() < 2) ? '\0' : name[1];
	switch(c) {
		
		case '$': {
			
			if(boost::starts_with(name, "^$param")) {
				const ScriptParameters & params = context.getParameters();
				s32 index = util::toInt(name.substr(7)).value_or(0);
				if(index < 1 || size_t(index) > params.size()) {
					txtcontent.clear();
				} else {
					txtcontent = params[size_t(index) - 1];
				}
				return TYPE_TEXT;
			}
			
			if(name == "^$objontop") {
				txtcontent = "none";
				if(context.getEntity()) {
					MakeTopObjString(context.getEntity(), txtcontent);
				}
				return TYPE_TEXT;
			}
			
			// ^$objontop_<extraBoundaryXZ>[_<entityID>]
			if(boost::starts_with(name, "^$objontop_")) {
				Entity * ent = context.getEntity();
				float extraBoundaryXZ = 0.f;
				
				std::string_view strCheck = name.substr(11);
				size_t posEntityID = strCheck.find('_');
				if(posEntityID == std::string_view::npos) {
					posEntityID = strCheck.length();
				} else {
					ent = entities.getById(name.substr(++posEntityID)); //++ to skip the '_' before the entityID
				}
				extraBoundaryXZ = util::parseFloat(name.substr(11,posEntityID-11));
				
				txtcontent = "none";
				if(ent) {
					MakeTopObjString(ent, txtcontent, extraBoundaryXZ);
				}
				
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case '&': {
			
			if(boost::starts_with(name, "^&param")) {
				const ScriptParameters & params = context.getParameters();
				s32 index = util::toInt(name.substr(7)).value_or(0);
				if(index < 1 || size_t(index) > params.size()) {
					*fcontent = 0.f;
				} else {
					*fcontent = util::parseFloat(params[size_t(index) - 1]);
				}
				return TYPE_FLOAT;
			}
			
			if(name == "^&playerdist") {
				if(context.getEntity()) {
					*fcontent = fdist(player.pos, context.getEntity()->pos);
					return TYPE_FLOAT;
				}
			}
			
			break;
		}
		
		case '#': {
			
			if(boost::starts_with(name, "^#param")) {
				const ScriptParameters & params = context.getParameters();
				s32 index = util::toInt(name.substr(7)).value_or(0);
				if(index < 1 || size_t(index) > params.size()) {
					*lcontent = 0;
				} else {
					*lcontent = util::parseInt(params[size_t(index) - 1]);
				}
				return TYPE_LONG;
			}
			
			if(name == "^#playerdist") {
				if(context.getEntity()) {
					*lcontent = long(fdist(player.pos, context.getEntity()->pos));
					return TYPE_LONG;
				}
			}
			
			if(name == "^#timer1") {
				if(!context.getEntity() || context.getEntity()->m_scriptTimers[0] == 0) {
					*lcontent = 0;
				} else {
					*lcontent = toMsi(g_gameTime.now() - context.getEntity()->m_scriptTimers[0]);
				}
				return TYPE_LONG;
			}
			
			if(name == "^#timer2") {
				if(!context.getEntity() || context.getEntity()->m_scriptTimers[1] == 0) {
					*lcontent = 0;
				} else {
					*lcontent = toMsi(g_gameTime.now() - context.getEntity()->m_scriptTimers[1]);
				}
				return TYPE_LONG;
			}
			
			if(name == "^#timer3") {
				if(!context.getEntity() || context.getEntity()->m_scriptTimers[2] == 0) {
					*lcontent = 0;
				} else {
					*lcontent = toMsi(g_gameTime.now() - context.getEntity()->m_scriptTimers[2]);
				}
				return TYPE_LONG;
			}
			
			if(name == "^#timer4") {
				if(!context.getEntity() || context.getEntity()->m_scriptTimers[3] == 0) {
					*lcontent = 0;
				} else {
					*lcontent = toMsi(g_gameTime.now() - context.getEntity()->m_scriptTimers[3]);
				}
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 'a': {
			
			if(boost::starts_with(name, "^amount")) {
				if(context.getEntity() && (context.getEntity()->ioflags & IO_ITEM)) {
					*fcontent = context.getEntity()->_itemdata->count;
				} else {
					*fcontent = 0;
				}
				return TYPE_FLOAT;
			}
			
			if(name == "^arxdays") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 * 2 / 86400000);
				return TYPE_LONG;
			}
			
			if(name == "^arxhours") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 3600000);
				return TYPE_LONG;
			}
			
			if(name == "^arxminutes") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 60000);
				return TYPE_LONG;
			}
			
			if(name == "^arxseconds") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 1000);
				return TYPE_LONG;
			}
			
			if(name == "^arxtime") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()));
				return TYPE_LONG;
			}
			
			if(name == "^arxtime_hours") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 3600000) % 12;
				if(*lcontent == 0) {
					*lcontent = 12;
				}
				return TYPE_LONG;
			}
			
			if(name == "^arxtime_minutes") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 60000) % 60;
				if(*lcontent == 0) {
					*lcontent = 60;
				}
				return TYPE_LONG;
			}
			
			if(name == "^arxtime_seconds") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) * 6 / 1000) % 60;
				if(*lcontent == 0) {
					*lcontent = 60;
				}
				return TYPE_LONG;
			}
			
			if(name == "^angle" || boost::starts_with(name, "^angle_")) {
				Entity * entity = getEntityParam(name, 7, context);
				*fcontent = entity ? (entity == entities.player() ? player.angle : entity->angle).getYaw() : 0;
				*fcontent = MAKEANGLE(*fcontent);
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^angleto_")) {
				Entity * entity = getEntityParam(name, 9, context);
				if(!entity || !context.getEntity()) {
					*fcontent = 0.f;
				} else {
					*fcontent = Camera::getLookAtAngle(context.getEntity()->pos, entity->pos).getYaw();
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^anglex_")) { //radians
				*fcontent = 0.f;
				Entity * entity = getEntityParam(name, 8, context);
				if(entity) {
					float yaw = (entity == entities.player() ? player.angle : entity->angle).getYaw();
					*fcontent = angleToVectorXZ(yaw).x;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^anglez_")) { //radians
				*fcontent = 0.f;
				Entity * entity = getEntityParam(name, 8, context);
				if(entity) {
					float yaw = (entity == entities.player() ? player.angle : entity->angle).getYaw();
					*fcontent = angleToVectorXZ(yaw).z;
				}
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		case 'b': {
			
			if(boost::starts_with(name, "^behavior")) {
				txtcontent = "";
				if(context.getEntity() && (context.getEntity()->ioflags & IO_NPC)) {
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_LOOK_AROUND) {
						txtcontent += "l";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_SNEAK) {
						txtcontent += "s";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_DISTANT) {
						txtcontent += "d";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_MAGIC) {
						txtcontent += "m";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_FIGHT) {
						txtcontent += "f";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_GO_HOME) {
						txtcontent += "h";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_FRIENDLY) {
						txtcontent += "r";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_MOVE_TO) {
						txtcontent += "t";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_FLEE) {
						txtcontent += "e";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_LOOK_FOR) {
						txtcontent += "o";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_HIDE) {
						txtcontent += "i";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_WANDER_AROUND) {
						txtcontent += "w";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_GUARD) {
						txtcontent += "u";
					}
					if(context.getEntity()->_npcdata->behavior & BEHAVIOUR_STARE_AT) {
						txtcontent += "a";
					}
				}
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^buyprice")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_ITEM)) {
					*fcontent = static_cast<float>(context.getEntity()->_itemdata->buyPrice);
				}
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		case 'c': {
			
			if(name == "^camera") {
				txtcontent = idString(g_cameraEntity);
				return TYPE_TEXT;
			}
			
			if(name == "^caster" || boost::starts_with(name, "^caster_")) {
				Entity * caster = nullptr;
				if(Entity * entity = getEntityParam(name, 8, context)) {
					caster = (entity->ioflags & IO_NPC) ? entities.get(entity->_npcdata->summoner) : nullptr;
				} else if(Spell * spell = getSpellParam(name, 8)) {
					caster = entities.get(spell->m_caster);
				}
				txtcontent = idString(caster);
				return TYPE_TEXT;
			}
			
			if(name == "^class") {
				txtcontent = context.getEntity() ? context.getEntity()->className() : "";
				return TYPE_TEXT;
			}
			
			if(name == "^class" || boost::starts_with(name, "^class_")) {
				txtcontent = EntityId(name.substr(7)).className();
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'd': {
			if(boost::starts_with(name, "^debugcalledfrom_")) { // ^debugcalledfrom_<indexFromLastOnTheCallStack>
				txtcontent = "void";
				std::string str = context.getGoSubCallStack("", "", "\n", util::parseInt(name.substr(17)));
				size_t i = str.find(context.strCallStackHighlight, 0);
				if(i != std::string::npos) {
					i += context.strCallStackHighlight.size();
					size_t i2 = str.find(context.strCallStackHighlight, i);
					txtcontent = str.substr(i, i2 - i);
				}
				return TYPE_TEXT;
			}
			
			if(name == "^degrees") {
				*fcontent = getDegrees(context, name, -1, 'y');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degrees_")){
				*fcontent = getDegrees(context, name, 9, 'y');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degreesx_")){
				*fcontent = getDegrees(context, name, 10, 'x');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degreesy_")){
				*fcontent = getDegrees(context, name, 10, 'y');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degreesz_")){
				*fcontent = getDegrees(context, name, 10, 'z');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degreesto_")){
				*fcontent = getDegrees(context, name, 11, 'Y');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^degreesyto_")){
				*fcontent = getDegrees(context, name, 12, 'Y');
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^dist_")) {
				if(context.getEntity()) {
					if( name[6] == '[' ) {
						*fcontent = 99999999999.f;
						
						Vec3f pos = Vec3f(0.f);
						size_t iStrPosNext = 6;
						
						size_t iStrPosIni=iStrPosNext+1; //skip '['
						iStrPosNext=name.find(',',iStrPosIni);
						if(iStrPosNext == std::string::npos) { return TYPE_FLOAT; LogError << "missing 1st ','"; }
						pos.x = util::parseFloat(name.substr(iStrPosIni,iStrPosNext-iStrPosIni));
						
						iStrPosIni=iStrPosNext+1; //skip ','
						iStrPosNext=name.find(',',iStrPosIni);
						if(iStrPosNext == std::string::npos) { return TYPE_FLOAT; LogError << "missing 2nd ','"; }
						pos.y = util::parseFloat(name.substr(iStrPosIni,iStrPosNext-iStrPosIni));
						
						iStrPosIni=iStrPosNext+1; //skip ','
						iStrPosNext=name.find(']',iStrPosIni);
						if(iStrPosNext == std::string::npos) { return TYPE_FLOAT; LogError << "missing ']'"; }
						pos.z = util::parseFloat(name.substr(iStrPosIni,iStrPosNext-iStrPosIni));
						
						LogDebug(' ' << pos.x <<' ' << pos.y <<' ' << pos.z);
						
						*fcontent = fdist(context.getEntity()->pos, pos);
					} else {
						Entity * target = entities.getById(name.substr(6));
						if(target == entities.player()) {
							*fcontent = fdist(player.pos, context.getEntity()->pos);
						} else if(target
											&& (context.getEntity()->show == SHOW_FLAG_IN_SCENE
													|| context.getEntity()->show == SHOW_FLAG_IN_INVENTORY)
											&& (target->show == SHOW_FLAG_IN_SCENE
													|| target->show == SHOW_FLAG_IN_INVENTORY)) {
							*fcontent = fdist(GetItemWorldPosition(context.getEntity()), GetItemWorldPosition(target));
						} else {
							*fcontent = 99999999999.f;
						}
					}
					return TYPE_FLOAT;
				}
			}
			
			if(boost::starts_with(name, "^demo")) {
				*lcontent = (g_resources->getReleaseType() & PakReader::Demo) ? 1 : 0;
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^durability")) {
				*fcontent = (context.getEntity()) ? context.getEntity()->durability : 0.f;
				return TYPE_FLOAT;
			}
			
			if(name == "^dragged") {
				txtcontent = idString(g_draggedEntity);
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'f': {
			
			if(boost::starts_with(name, "^focal")) {
				if(context.getEntity() && (context.getEntity()->ioflags & IO_CAMERA)) {
					*fcontent = context.getEntity()->_camdata->cam.focal;
					return TYPE_FLOAT;
				}
			}
			
			if(boost::starts_with(name, "^fighting")) {
				*lcontent = long(ARX_PLAYER_IsInFightMode());
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^fps")) {
				*fcontent = g_fpsCounter.FPS;
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		case 'g': {
			
			if(name == "^gore") {
				*lcontent = 1;
				return TYPE_LONG;
			}
			
			if(name == "^gamedays") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) / 86400000);
				return TYPE_LONG;
			}
			
			if(name == "^gamehours") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) / 3600000);
				return TYPE_LONG;
			}
			
			if(name == "^gameminutes") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) / 60000);
				return TYPE_LONG;
			}
			
			if(name == "^gameseconds") {
				*lcontent = static_cast<long>(toMsi(g_gameTime.now()) / 1000);
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 'h': {
			
			if(name == "^hover") {
				txtcontent = idString(FlyingOverIO);
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^hover_")) {
				txtcontent = idString( GetFirstInterAtPos( DANAEMouse, util::parseFloat(name.substr(7)) ) );
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'i': {
			
			if(boost::starts_with(name, "^inzone_")) {
				std::string_view zone = name.substr(8);
				Zone * ap = getZoneByName(zone);
				*lcontent = 0;
				if(context.getEntity() && ap) {
					if(ARX_PATH_IsPosInZone(ap, context.getEntity()->pos)) {
						*lcontent = 1;
					}
				}
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^ininitpos")) {
				*lcontent = 0;
				if(context.getEntity()) {
					Vec3f pos = GetItemWorldPosition(context.getEntity());
					if(pos == context.getEntity()->initpos)
						*lcontent = 1;
				}
				return TYPE_LONG;
			}
			
			if(name == "^ininventory") {
				txtcontent = idString(locateInInventories(context.getEntity()).container);
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^inplayerinventory")) {
				*lcontent = IsInPlayerInventory(context.getEntity()) ? 1 : 0;
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 'l': {
			
			if(name == "^life") {
				*fcontent = getLife(name, 'c', 0, context.getEntity());
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^life_")) {
				*fcontent = getLife(name, 'c', 6);
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^lifemax_")) {
				*fcontent = getLife(name, 'm', 9);
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^lifemodmax_")) {
				*fcontent = getLife(name, 'M', 12);
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^last_spawned")) {
				txtcontent = idString(LASTSPAWNED);
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^locationx_")) {
				*fcontent = getLocation(name, 'x');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^locationy_")) {
				*fcontent = getLocation(name, 'y');
				return TYPE_FLOAT;
			}
			if(boost::starts_with(name, "^locationz_")) {
				*fcontent = getLocation(name, 'z');
				return TYPE_FLOAT;
			}
			
			if(name == "^lootinventory") {
				txtcontent = idString(
					(g_secondaryInventoryHud.isVisible() && g_secondaryInventoryHud.isOpen()) ? 
					g_secondaryInventoryHud.getEntity() : nullptr );
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^sellprice")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_ITEM)) {
					*fcontent = static_cast<float>(context.getEntity()->_itemdata->sellPrice);
				}
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		case 'm': {
			
			if(boost::starts_with(name, "^me")) {
				txtcontent = idString(context.getEntity());
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^maxlife")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_NPC)) {
					*fcontent = context.getEntity()->_npcdata->lifePool.max;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^mana")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_NPC)) {
					*fcontent = context.getEntity()->_npcdata->manaPool.current;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^maxmana")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_NPC)) {
					*fcontent = context.getEntity()->_npcdata->manaPool.max;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^myspell_")) {
				SpellType id = GetSpellId(name.substr(9));
				if(id != SPELL_NONE) {
					if(spells.getSpellByCaster(context.getEntity()->index(), id)) {
						*lcontent = 1;
						return TYPE_LONG;
					}
				}
				*lcontent = 0;
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^maxdurability")) {
				*fcontent = (context.getEntity()) ? context.getEntity()->max_durability : 0.f;
				return TYPE_FLOAT;
			}
			
			if(name == "^lootinventory") {
				txtcontent = idString(
					(g_secondaryInventoryHud.isVisible() && g_secondaryInventoryHud.isOpen()) ? 
					g_secondaryInventoryHud.getEntity() : nullptr );
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'n': {
			
			if(boost::starts_with(name, "^npcinsight")) {
				txtcontent = idString(context.getEntity() ? getFirstNpcInSight(*context.getEntity()) : nullptr);
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'o': {
			
			if(name == "^onscreen" || boost::starts_with(name, "^onscreen_")) {
				Entity * entity = getEntityParam(name, 10, context);
				if(isInCinematic() || !entity) {
					*lcontent = -2;
				} else {
					switch(getEntityVisibility(*entity)) {
						case EntityInactive: [[fallthrough]];
						case EntityNotInView: [[fallthrough]];
						case EntityFullyOccluded: {
							*lcontent = -1;
							break;
						}
						case EntityVisibilityUnknown: {
							*lcontent = 0;
							break;
						}
						case EntityVisible: {
							*lcontent = 1;
							break;
						}
						case EntityInFocus: {
							*lcontent = 2;
							break;
						}
					}
				}
				return TYPE_LONG;
			}
			
			if(name == "^offscreen" || boost::starts_with(name, "^offscreen_")) {
				Entity * entity = getEntityParam(name, 11, context);
				if(isInCinematic() || !entity) {
					*lcontent = 2;
				} else {
					*lcontent = (getEntityVisibility(*entity, true) < EntityVisibilityUnknown ? 1 : 0);
				}
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 'p': {
			
			if(boost::starts_with(name, "^price")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_ITEM)) {
					*fcontent = static_cast<float>(context.getEntity()->_itemdata->buyPrice);
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_zone")) {
				Zone * zone = entities.player()->inzone;
				txtcontent = (zone ? zone->name : "none");
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^player_life")) {
				*fcontent = player.Full_life; // TODO why not player.life like everywhere else?
				return TYPE_FLOAT;
			}

			if(boost::starts_with(name, "^player_mana")) {
				*fcontent = player.manaPool.current;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^poisoned")) {
				*fcontent = 0;
				if(context.getEntity() && (context.getEntity()->ioflags & IO_NPC)) {
					*fcontent = context.getEntity()->_npcdata->poisonned;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^poisonous")) {
				*fcontent = (context.getEntity()) ? context.getEntity()->poisonous : 0.f;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^possess_")) {
				Entity * target = entities.getById(name.substr(9));
				if(IsInPlayerInventory(target)) {
					*lcontent = 1;
				} else if(isEquippedByPlayer(target)) {
					*lcontent = 2;
				} else {
					*lcontent = 0;
				}
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^player_gold")) {
				*fcontent = static_cast<float>(player.gold);
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_maxlife")) {
				*fcontent = player.lifePool.max;
				return TYPE_FLOAT;
			}

			if(boost::starts_with(name, "^player_maxmana")) {
				*fcontent = player.manaPool.max;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_attribute_strength")) {
				*fcontent = player.m_attributeFull.strength;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_attribute_dexterity")) {
				*fcontent = player.m_attributeFull.dexterity;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_attribute_constitution")) {
				*fcontent = player.m_attributeFull.constitution;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_attribute_mind")) {
				*fcontent = player.m_attributeFull.mind;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_stealth")) {
				*fcontent = player.m_skillFull.stealth;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_mecanism")) {
				*fcontent = player.m_skillFull.mecanism;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_intuition")) {
				*fcontent = player.m_skillFull.intuition;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_etheral_link")) {
				*fcontent = player.m_skillFull.etheralLink;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_object_knowledge")) {
				*fcontent = player.m_skillFull.objectKnowledge;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_casting")) {
				*fcontent = player.m_skillFull.casting;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_projectile")) {
				*fcontent = player.m_skillFull.projectile;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_close_combat")) {
				*fcontent = player.m_skillFull.closeCombat;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_skill_defense")) {
				*fcontent = player.m_skillFull.defense;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_hunger")) {
				*fcontent = player.hunger;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^player_poison")) {
				*fcontent = player.poison;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^playercasting")) {
				for(const Spell & spell : spells.byCaster(EntityHandle_Player)) {
					if(spell.m_type == SPELL_LIFE_DRAIN
					   || spell.m_type == SPELL_HARM
					   || spell.m_type == SPELL_FIRE_FIELD
					   || spell.m_type == SPELL_ICE_FIELD
					   || spell.m_type == SPELL_LIGHTNING_STRIKE
					   || spell.m_type == SPELL_MASS_LIGHTNING_STRIKE) {
						*lcontent = 1;
						return TYPE_LONG;
					}
				}
				*lcontent = 0;
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^playerspell_")) {
				std::string_view temp = name.substr(13);
				
				SpellType id = GetSpellId(temp);
				if(id != SPELL_NONE) {
					if(spells.getSpellByCaster(EntityHandle_Player, id)) {
						*lcontent = 1;
						return TYPE_LONG;
					}
				}
				
				if(temp == "invisibility" && entities.player()->invisibility > 0.3f) {
					*lcontent = 1;
					return TYPE_LONG;
				}
				
				*lcontent = 0;
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 'r': {
			
			if(boost::starts_with(name, "^realdist_")) {
				if(context.getEntity()) {
					
					Entity * target = entities.getById(name.substr(10));
					if(target == entities.player()) {
						if(context.getEntity()->requestRoomUpdate) {
							UpdateIORoom(context.getEntity());
						}
						RoomHandle playerRoom = ARX_PORTALS_GetRoomNumForPosition(player.pos, RoomPositionForCamera);
						*fcontent = SP_GetRoomDist(context.getEntity()->pos, player.pos, context.getEntity()->room, playerRoom);
					} else if(target
					          && (context.getEntity()->show == SHOW_FLAG_IN_SCENE
					              || context.getEntity()->show == SHOW_FLAG_IN_INVENTORY)
					          && (target->show == SHOW_FLAG_IN_SCENE
					              || target->show == SHOW_FLAG_IN_INVENTORY)) {
						
						Vec3f pos  = GetItemWorldPosition(context.getEntity());
						Vec3f pos2 = GetItemWorldPosition(target);
						
						if(context.getEntity()->requestRoomUpdate) {
							UpdateIORoom(context.getEntity());
						}
						
						if(target->requestRoomUpdate) {
							UpdateIORoom(target);
						}
						
						*fcontent = SP_GetRoomDist(pos, pos2, context.getEntity()->room, target->room);
						
					} else {
						// Out of this world item
						*fcontent = 99999999999.f;
					}
					return TYPE_FLOAT;
				}
			}
			
			if(name == "^realtime_year") {
				*lcontent = getSystemTime().year;
				return TYPE_LONG;
			}
			
			if(name == "^realtime_month") {
				*lcontent = getSystemTime().month;
				return TYPE_LONG;
			}
			
			if(name == "^realtime_day") {
				*lcontent = getSystemTime().day;
				return TYPE_LONG;
			}
			
			if(boost::starts_with(name, "^repairprice_")) {
				Entity * target = entities.getById(name.substr(13));
				if(target) {
					*fcontent = ARX_DAMAGES_ComputeRepairPrice(target, context.getEntity());
				} else {
					*fcontent = 0;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^rnd_")) {
				std::string_view max = name.substr(5);
				// TODO should max be inclusive or exclusive?
				// if inclusive, use proper integer random, otherwise fix rnd()?
				if(!max.empty()) {
					*fcontent = Random::getf(0.f, util::parseFloat(max));
				} else {
					*fcontent = 0;
				}
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^rune_")) {
				std::string_view temp = name.substr(6);
				*lcontent = 0;
				if(temp == "aam") {
					*lcontent = player.rune_flags & FLAG_AAM;
				} else if(temp == "cetrius") {
					*lcontent = player.rune_flags & FLAG_CETRIUS;
				} else if(temp == "comunicatum") {
					*lcontent = player.rune_flags & FLAG_COMUNICATUM;
				} else if(temp == "cosum") {
					*lcontent = player.rune_flags & FLAG_COSUM;
				} else if(temp == "folgora") {
					*lcontent = player.rune_flags & FLAG_FOLGORA;
				} else if(temp == "fridd") {
					*lcontent = player.rune_flags & FLAG_FRIDD;
				} else if(temp == "kaom") {
					*lcontent = player.rune_flags & FLAG_KAOM;
				} else if(temp == "mega") {
					*lcontent = player.rune_flags & FLAG_MEGA;
				} else if(temp == "morte") {
					*lcontent = player.rune_flags & FLAG_MORTE;
				} else if(temp == "movis") {
					*lcontent = player.rune_flags & FLAG_MOVIS;
				} else if(temp == "nhi") {
					*lcontent = player.rune_flags & FLAG_NHI;
				} else if(temp == "rhaa") {
					*lcontent = player.rune_flags & FLAG_RHAA;
				} else if(temp == "spacium") {
					*lcontent = player.rune_flags & FLAG_SPACIUM;
				} else if(temp == "stregum") {
					*lcontent = player.rune_flags & FLAG_STREGUM;
				} else if(temp == "taar") {
					*lcontent = player.rune_flags & FLAG_TAAR;
				} else if(temp == "tempus") {
					*lcontent = player.rune_flags & FLAG_TEMPUS;
				} else if(temp == "tera") {
					*lcontent = player.rune_flags & FLAG_TERA;
				} else if(temp == "vista") {
					*lcontent = player.rune_flags & FLAG_VISTA;
				} else if(temp == "vitae") {
					*lcontent = player.rune_flags & FLAG_VITAE;
				} else if(temp == "yok") {
					*lcontent = player.rune_flags & FLAG_YOK;
				}
				return TYPE_LONG;
			}
			
			break;
		}
		
		case 's': {
			
			if(boost::starts_with(name, "^sender")) {
				txtcontent = idString(context.getSender());
				return TYPE_TEXT;
			}
			
			if(boost::starts_with(name, "^scale")) {
				*fcontent = (context.getEntity()) ? context.getEntity()->scale * 100.f : 0.f;
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^speaking")) {
				if(context.getEntity() && getSpeechForEntity(*context.getEntity())) {
					*lcontent = 1;
					return TYPE_LONG;
				}
				*lcontent = 0;
				return TYPE_LONG;
			}
			
			if(name == "^spell" || boost::starts_with(name, "^spell_")) {
				Entity * entity = getEntityParam(name, 7, context);
				Spell * lastSpell = nullptr;
				if(entity) {
					for(Spell & spell : spells.byCaster(entity->index())) {
						if(!lastSpell || lastSpell->m_timcreation < spell.m_timcreation) {
							lastSpell = &spell;
						}
					}
				}
				txtcontent = lastSpell ? lastSpell->idString() : "none";
				return TYPE_TEXT;
			}
			
			if(name == "^spelllevel") {
				*fcontent = player.spellLevel();
				return TYPE_FLOAT;
			}
			
			if(boost::starts_with(name, "^spelllevel_")) {
				Spell * spell = getSpellParam(name, 12);
				*fcontent = spell ? spell->m_level : -1;
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		case 't': {
			
			if(boost::starts_with(name, "^target")) {
				txtcontent = idString(context.getEntity() ? entities.get(context.getEntity()->targetinfo) : nullptr);
				return TYPE_TEXT;
			}
			
			break;
		}
		
		case 'v': {
			
			if(name == "^viewx" || boost::starts_with(name, "^viewx_")) {
				Entity * entity = getEntityParam(name, 7, context);
				*fcontent = entity ? angleToVector(entity == entities.player() ? player.angle : entity->angle).x : 0;
				return TYPE_FLOAT;
			}
			
			if(name == "^viewy" || boost::starts_with(name, "^viewy_")) {
				Entity * entity = getEntityParam(name, 7, context);
				*fcontent = entity ? angleToVector(entity == entities.player() ? player.angle : entity->angle).y : 0;
				return TYPE_FLOAT;
			}
			
			if(name == "^viewz" || boost::starts_with(name, "^viewz_")) {
				Entity * entity = getEntityParam(name, 7, context);
				*fcontent = entity ? angleToVector(entity == entities.player() ? player.angle : entity->angle).z : 0;
				return TYPE_FLOAT;
			}
			
			break;
		}
		
		default: break;
		
	}
	
	*lcontent = 0;
	return TYPE_LONG;
}

void ARX_SCRIPT_Free_All_Global_Variables() {
	svar.clear();
}

void CloneLocalVars(Entity * ioo, Entity * io) {
	
	if(!ioo || !io) {
		return;
	}
	
	ioo->m_variables = io->m_variables;
}

static SCRIPT_VAR * GetVarAddress(SCRIPT_VARIABLES & svf, std::string_view name) {
	
	for(SCRIPT_VAR & var : svf) {
		if(var.name == name) {
			return &var;
		}
	}
	
	return nullptr;
}

const SCRIPT_VAR * GetVarAddress(const SCRIPT_VARIABLES & svf, std::string_view name) {
	
	for(const SCRIPT_VAR & var : svf) {
		if(var.name == name) {
			return &var;
		}
	}
	
	return nullptr;
}

static SCRIPT_VAR * getOrCreateScriptVariable(SCRIPT_VARIABLES & svf, std::string_view name) {
	
	if(SCRIPT_VAR * existing = GetVarAddress(svf, name)) {
		return existing;
	}
	
	return &svf.emplace_back(name);
}

long GETVarValueLong(const SCRIPT_VARIABLES & svf, std::string_view name) {
	const SCRIPT_VAR * tsv = GetVarAddress(svf, name);
	return tsv ? tsv->ival : 0;
}

float GETVarValueFloat(const SCRIPT_VARIABLES & svf, std::string_view name) {
	const SCRIPT_VAR * tsv = GetVarAddress(svf, name);
	return tsv ? tsv->fval : 0.f;
}

std::string_view GETVarValueText(const SCRIPT_VARIABLES & svf, std::string_view name) {
	const SCRIPT_VAR * tsv = GetVarAddress(svf, name);
	return tsv ? std::string_view(tsv->text) : "";
}

SCRIPT_VAR * SETVarValueLong(SCRIPT_VARIABLES & svf, std::string_view name, long val) {
	SCRIPT_VAR * tsv = getOrCreateScriptVariable(svf, name);
	tsv->ival = val;
	return tsv;
}

SCRIPT_VAR * SETVarValueFloat(SCRIPT_VARIABLES & svf, std::string_view name, float val) {
	SCRIPT_VAR * tsv = getOrCreateScriptVariable(svf, name);
	tsv->fval = val;
	return tsv;
}

SCRIPT_VAR * SETVarValueText(SCRIPT_VARIABLES & svf, std::string_view name, std::string && val) {
	SCRIPT_VAR * tsv = getOrCreateScriptVariable(svf, name);
	tsv->text = std::move(val);
	return tsv;
}

struct QueuedEvent {
	
	bool exists;
	Entity * sender;
	Entity * entity;
	ScriptEventName event;
	ScriptParameters parameters;
	
	void clear() {
		exists = false;
		sender = nullptr;
		entity = nullptr;
		event = ScriptEventName();
		parameters.clear();
	}
	
};

// TODO use a queue
static QueuedEvent g_eventQueue[800];

void ARX_SCRIPT_EventStackInit() {
	ARX_SCRIPT_EventStackClear(false); // Clear everything in the stack
}

void ARX_SCRIPT_EventStackClear(bool check_exist) {
	LogDebug("clearing event queue");
	for(QueuedEvent & event : g_eventQueue) {
		if(!check_exist || event.exists) {
			event.clear();
		}
	}
}

void ARX_SCRIPT_EventStackClearForIo(Entity * io) {
	for(QueuedEvent & event : g_eventQueue) {
		if(event.exists && event.entity == io) {
			LogDebug("clearing queued " << event.event << " for " << io->idString());
			event.clear();
		}
	}
}

void ARX_SCRIPT_EventStackExecute(size_t limit) {
	
	ARX_PROFILE_FUNC();
	
	size_t count = 0;
	
	for(QueuedEvent & event : g_eventQueue) {
		
		if(!event.exists) {
			continue;
		}
		
		if(ValidIOAddress(event.entity)) {
			Entity * sender = ValidIOAddress(event.sender) ? event.sender : nullptr;
			LogDebug("running queued " << event.event << " for " << event.entity->idString());
			SendIOScriptEvent(sender, event.entity, event.event, event.parameters);
		} else {
			LogDebug("could not run queued " << event.event
			         << " params=\"" << event.parameters << "\" - entity vanished");
		}
		event.clear();
		
		// Abort if the event limit was reached
		if(++count >= limit) {
			return;
		}
		
	}
	
}

void ARX_SCRIPT_EventStackExecuteAll() {
	ARX_SCRIPT_EventStackExecute(std::numeric_limits<size_t>::max());
}

void Stack_SendIOScriptEvent(Entity * sender, Entity * entity, const ScriptEventName & event,
                             const ScriptParameters & parameters) {
	for(QueuedEvent & entry : g_eventQueue) {
		if(!entry.exists) {
			entry.sender = sender;
			entry.entity = entity;
			entry.event = event;
			entry.parameters = parameters;
			entry.exists = true;
			return;
		}
	}
}

ScriptResult SendIOScriptEvent(Entity * sender, Entity * entity, const ScriptEventName & event,
                               const ScriptParameters & parameters) {
	
	ARX_PROFILE_FUNC();
	
	if(!entity) {
		return REFUSE;
	}
	
	EntityHandle num = entity->index();
	if(!entities[num]) {
		return REFUSE;
	}
	
	// Send the event to the instance script first
	if(entities[num]->over_script.valid) {
		ScriptResult ret = ScriptEvent::send(&entities[num]->over_script, sender, entities[num], event, parameters);
		if(ret == REFUSE || ret == DESTRUCTIVE || !entities[num]) {
			return !entities[num] ? REFUSE : ret;
		}
	}
	
	// If the instance script did not refuse the event also send it to the class script
	return ScriptEvent::send(&entities[num]->script, sender, entities[num], event, parameters);
}

ScriptResult SendInitScriptEvent(Entity * io) {
	
	if(!io) {
		return REFUSE;
	}
	
	EntityHandle num = io->index();
	
	if(entities[num] && entities[num]->script.valid) {
		ScriptEvent::send(&entities[num]->script, nullptr, entities[num], SM_INIT);
	}
	
	if(entities[num] && entities[num]->over_script.valid) {
		ScriptEvent::send(&entities[num]->over_script, nullptr, entities[num], SM_INIT);
	}
	
	if(entities[num] && entities[num]->script.valid) {
		ScriptEvent::send(&entities[num]->script, nullptr, entities[num], SM_INITEND);
	}
	
	if(entities[num] && entities[num]->over_script.valid) {
		ScriptEvent::send(&entities[num]->over_script, nullptr, entities[num], SM_INITEND);
	}
	
	return ACCEPT;
}

std::string getDefaultScriptTimerName(Entity * io, std::string_view prefix) {
	
	for(size_t i = 1; ; i++) {
		std::ostringstream oss;
		oss << prefix << '_' << i;
		if(!scriptTimerExists(io, oss.str())) {
			return oss.str();
		}
	}
	
}

SCR_TIMER & createScriptTimer(Entity * io, std::string && name) {
	
	arx_assert(g_activeScriptTimers <= g_scriptTimers.size());
	
	g_activeScriptTimers++;
	
	if(g_activeScriptTimers != g_scriptTimers.size() + 1) {
		for(SCR_TIMER & timer : g_scriptTimers) {
			if(!timer.exist) {
				timer = SCR_TIMER(io, std::move(name));
				return timer;
			}
		}
	}
	
	return g_scriptTimers.emplace_back(io, std::move(name));
}

size_t ARX_SCRIPT_CountTimers() {
	return g_activeScriptTimers;
}

static void clearTimer(SCR_TIMER & timer) {
	if(timer.exist) {
		LogDebug("clearing timer " << timer.name);
		timer.name.clear();
		timer.exist = 0;
		g_activeScriptTimers--;
	}
}

void ARX_SCRIPT_Timer_Clear_By_Name_And_IO(std::string_view timername, Entity * io) {
	for(SCR_TIMER & timer : g_scriptTimers) {
		if(timer.exist && timer.io == io && timer.name == timername) {
			clearTimer(timer);
		}
	}
}

void ARX_SCRIPT_Timer_Clear_All_Locals_For_IO(Entity * io) {
	for(SCR_TIMER & timer : g_scriptTimers) {
		if(timer.exist && timer.io == io && timer.es == &io->over_script) {
			clearTimer(timer);
		}
	}
}

void ARX_SCRIPT_Timer_ClearAll() {
	g_scriptTimers.clear();
	g_activeScriptTimers = 0;
}

void ARX_SCRIPT_Timer_Clear_For_IO(Entity * io) {
	for(SCR_TIMER & timer : g_scriptTimers) {
		if(timer.exist && timer.io == io) {
			clearTimer(timer);
		}
	}
}

bool scriptTimerExists(Entity * io, std::string_view name) {
	
	if(g_activeScriptTimers != 0) {
		for(const SCR_TIMER & timer : g_scriptTimers) {
			if(timer.exist && timer.io == io && timer.name == name) {
				return true;
			}
		}
	}
	
	return false;
}

static bool Manage_Specific_RAT_Timer(SCR_TIMER * st) {
	
	arx_assert(st->name == "_r_a_t_");
	
	Entity * io = st->io;
	GetTargetPos(io);
	Vec3f target = io->target - io->pos;
	target = glm::normalize(target);
	Vec3f targ = VRotateY(target, Random::getf(-30.f, 30.f));
	target = io->target + targ * 100.f;
	
	if(ARX_INTERACTIVE_ConvertToValidPosForIO(io, &target)) {
		ARX_INTERACTIVE_Teleport(io, target);
		Vec3f pos = io->pos;
		pos.y += io->physics.cyl.height * 0.5f;
		
		ARX_PARTICLES_Add_Smoke(pos, 3, 20);
		AddRandomSmoke(*io, 20);
		MakeCoolFx(io->pos);
		io->setOwner(nullptr);
		io->show = SHOW_FLAG_IN_SCENE;
		
		for(long kl = 0; kl < 10; kl++) {
			FaceTarget2(io);
		}
		
		io->gameFlags &= ~GFLAG_INVISIBILITY;
		st->count = 1;
	} else {
		st->count++;
		st->interval = st->interval / 2;
		if(st->interval < 100ms)
			st->interval = 100ms;
		
		return true;
	}
	
	return false;
}

void ARX_SCRIPT_Timer_Check() {
	
	ARX_PROFILE_FUNC();
	
	if(g_activeScriptTimers == 0) {
		return;
	}
	
	for(SCR_TIMER & timer : g_scriptTimers) {
		
		if(!timer.exist) {
			continue;
		}
		
		GameInstant now = g_gameTime.now();
		GameInstant fire_time = timer.start + timer.interval;
		arx_assert(timer.start <= now);
		if(fire_time > now) {
			// Timer not ready to fire yet
			continue;
		}
		
		// Skip heartbeat timer events for far away objects
		if(timer.idle && !(timer.io->gameFlags & GFLAG_ISINTREATZONE)) {
			if(timer.interval == 0) {
				timer.start = now;
			} else {
				s64 increment = toMsi(now - timer.start) / toMsi(timer.interval); // TODO handle interval 0
				timer.start += timer.interval * increment;
			}
			// TODO print full 64-bit time
			arx_assert_msg(timer.start <= now && (timer.interval == 0 || timer.start + timer.interval > now),
			               "start=%ld wait=%ld now=%ld",
			               long(toMsi(timer.start)), long(toMsi(timer.interval)), long(toMsi(now)));
			continue;
		}
		
		const EERIE_SCRIPT * es = timer.es;
		Entity * io = timer.io;
		size_t pos = timer.pos;
		
		if(!es && Manage_Specific_RAT_Timer(&timer)) {
			continue;
		}
		
		#ifdef ARX_DEBUG
		std::string name = timer.name;
		#endif
		
		if(timer.count == 1) {
			clearTimer(timer);
		} else {
			if(timer.count != 0) {
				timer.count--;
			}
			if(timer.interval == 0) {
				timer.start = now;
			} else {
				timer.start += timer.interval;
			}
		}
		
		if(es && ValidIOAddress(io)) {
			LogDebug("running timer \"" << name << "\" for entity " << io->idString());
			ScriptEvent::resume(es, io, pos, &timer);
		} else {
			LogDebug("could not run timer \"" << name << "\" - entity vanished");
		}
		
	}
}

void ARX_SCRIPT_Init_Event_Stats() {
	
	ScriptEvent::totalCount = 0;
	
	for(Entity & entity : entities) {
		entity.stat_count = 0;
		entity.stat_sent = 0;
	}
	
}

Entity * ARX_SCRIPT_Get_IO_Max_Events() {
	
	long max = -1;
	Entity * result = nullptr;
	
	for(Entity & entity : entities) {
		if(entity.stat_count > max) {
			result = &entity;
			max = entity.stat_count;
		}
	}
	
	return result;
}

Entity * ARX_SCRIPT_Get_IO_Max_Events_Sent() {
	
	long max = -1;
	Entity * result = nullptr;
	
	for(Entity & entity : entities) {
		if(entity.stat_sent > max) {
			result = &entity;
			max = entity.stat_sent;
		}
	}
	
	return result;
}

void ManageCasseDArme(Entity * io) {
	
	if(!(io->type_flags & (OBJECT_TYPE_DAGGER | OBJECT_TYPE_1H | OBJECT_TYPE_2H | OBJECT_TYPE_BOW)))
		return;
	
	Entity * pObjMin = nullptr;
	Entity * pObjMax = nullptr;
	Entity * pObjFIX = nullptr;
	
	for(auto slot : entities.player()->inventory->slotsInGrid<util::GridZYXIterator>()) {
		Entity * bagEntity = slot.entity;
		
		if(bagEntity && bagEntity != io
		   && (bagEntity->type_flags & (OBJECT_TYPE_DAGGER | OBJECT_TYPE_1H | OBJECT_TYPE_2H | OBJECT_TYPE_BOW))
		) {
			
			if(   (io->ioflags & IO_ITEM)
			   && (bagEntity->ioflags & IO_ITEM)
			   && bagEntity->_itemdata->equipitem
			) {
				if(bagEntity->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value == io->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value) {
					pIOChangeWeapon = bagEntity;
					lChangeWeapon = 2;
					return;
				} else {
					if(bagEntity->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value > io->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value) {
						if(pObjMin) {
							if(bagEntity->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value < pObjMin->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value) {
								pObjMin = bagEntity;
							}
						} else {
							pObjMin = bagEntity;
						}
					} else {
						if(bagEntity->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value < io->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value) {
							if(pObjMax) {
								if(bagEntity->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value > pObjMax->_itemdata->equipitem->elements[IO_EQUIPITEM_ELEMENT_Damages].value) {
									pObjMax = bagEntity;
								}
							} else {
								pObjMax = bagEntity;
							}
						}
					}
				}
			} else {
				if(!pObjFIX) {
					pObjFIX = bagEntity;
				}
			}
		}
		
		if(pObjMax) {
			pIOChangeWeapon = pObjMax;
			lChangeWeapon = 2;
		} else {
			if(pObjMin) {
				pIOChangeWeapon = pObjMin;
				lChangeWeapon = 2;
			} else {
				if(pObjFIX) {
					pIOChangeWeapon = pObjFIX;
					lChangeWeapon = 2;
				}
			}
		}
	}
	
}

bool writeScriptAtModDumpFolder(const res::path & pathModdedDump, const std::string & esdatPatched, const std::string & esdatOriginal) {
	if(esdatOriginal.size() == esdatPatched.size() && esdatOriginal == esdatPatched) { // because testing the size first, allows just passing an empty esdatOriginal to force writing esdatPatched
		return true; // ok, nothing changed
	}
	
	res::path folder = pathModdedDump.parent();
	LogDebug("folder=" << pathModdedDump << ", fl=" << pathModdedDump);
	std::filesystem::create_directories(folder.string());
	
	static std::ofstream flModdedDump;
	flModdedDump.open(pathModdedDump.string(), std::ios_base::trunc); //std::ios_base::app);
	if(!flModdedDump.fail()) {
		flModdedDump << esdatPatched << "\n";
		flModdedDump.flush();
		flModdedDump.close();
		LogDebug("wrote " << pathModdedDump);
		return true;
	}
	
	LogError << "Failed to write mod dump file '" << pathModdedDump.string() << "'";
	
	return false;
}

size_t detectAndFixGoToGoSubParam(std::string & line) { // transform goto/gosub param var=value into var value (replace '=' with space)
	static std::string strSearchRegex = "_*g_*o_*(t_*o|s_*u_*b)_*";
	static std::regex * reSearch = nullptr;
	reSearch = util::prepareRegex(reSearch, strSearchRegex.c_str());
	if(!reSearch) {
		LogCritical << "invalid regex: " << strSearchRegex;
		return 0;
	}
	
	if (std::regex_search(line, *reSearch)) {
		/**
		 * create a strict/precise match as possible.
		 * For \xBB read ScriptedLand.cpp GotoCommand::createParamVar() (\xAB is never on the left side),
		 * also \xBB{0,1} means that a GoSub param can also set a normal var using it's full name
		 * (instead of an auto var name based on a short name).
		 */
		static std::string strReplaceRegex = "([ \t][@\xA3\xA7][\xBB]{0,1}[a-z0-9_]*)=([^ \t])";
		static std::regex * reReplace = nullptr;
		reReplace = util::prepareRegex(reReplace, strReplaceRegex.c_str());
		if(!reReplace) {
			LogCritical << "invalid regex: " << strReplaceRegex;
			return 0;
		}
		
		std::string strLineBefore = line;
		line = std::regex_replace(line, *reReplace, "$1 $2");
		if(strLineBefore != line) {
			return 1; // TODO count diff chars?
		}
	}
	
	return 0;
}
size_t adaptScriptCode(std::string & line) {
	return detectAndFixGoToGoSubParam(line);
}
/**
 * Necessary because of other parts of the code that seek back for the single line comment token "//" !
 * IMPORTANT: This is destructive. Will replace initial chars of each line in the multiline comment with "//", but only in the RAM.
 */
bool detectAndTransformMultilineCommentIntoSingleLineComments(std::string & esdat, res::path & pathScript) {
	std::stringstream ssErrMsg;
	ssErrMsg << "MultilineCommentScript at '" << pathScript.string();
	
	std::string strNL = esdat.find("\r\n", 0) != std::string::npos ? "\r\n" : "\n";
	
	std::vector<std::string> lines;
	boost::split(lines, esdat, boost::is_any_of("\n"));
	
	esdat="";
	bool bSeekBeginMLC = true;
	size_t lineCount = 0;
	size_t countMLC = 0;
	size_t countSLC = 0;
	size_t countASC = 0;
	for(std::string line : lines) {
		lineCount++;
		
		countASC += adaptScriptCode(line);
		
		if(bSeekBeginMLC) {
			size_t posBeginMLC = line.find("/*");
			if(posBeginMLC == std::string::npos) { // not found, is normal line
				esdat += line + "\n";
				continue;
			} else { // is begin of multiline comment?
				size_t posComment = line.find("//");
				if(posComment != std::string::npos && posComment < posBeginMLC) { // a line like "// ... /* " is not a begin of a multiline comment
					esdat += line + "\n";
					continue;
				} else {  // is begin of multiline comment!
					// replaces "/*" with "//"
					line[posBeginMLC  ] = '/';
					line[posBeginMLC+1] = '/';
					countSLC++;
					bSeekBeginMLC = false;
					countMLC++;
				}
			}
		} else { //seek end
			size_t posEndMLC = line.find("*/");
			
			if(line.size() == 1) { // transform into empty line
				line=" ";
			} else if(line.size() >= 2) { // transform in single line comment
				line[0] = '/';
				line[1] = '/';
				countSLC++;
			}
			
			if(posEndMLC == std::string::npos) { // not found, is simple commented line
				esdat += line + "\n";
				continue;
			} else { // is end of multiline comment
				if((posEndMLC+2+(strNL.size()-1)) < line.size()) {
					LogError << ssErrMsg.str() << "' [line=" << lineCount << "]: " << "the closing '*/' token shall always be followed by a newline. line content is: \"" << line << "\""; // show just a simple user instruction. must have a '\n', otherwise auto adding a newline here would make the line calculation, of other messages, miss the original script! obs.: do not use arx_assert_msg() as mod developers (and end users too) may cause this by editing .asl files!
				}
				esdat += line + "\n";
				bSeekBeginMLC = true;
			}
		}
	}
	
	if((countMLC+countSLC+countASC) > 0) {
		LogDebug("Converted " << countMLC << " multiline comment(s) into " << countSLC << " single line comments and adapted " << countASC << " lines of code at " << pathScript.string());
		return true;
	}
	
	return false;
}

void fixLineEnding(std::string & strData, char cLineEndingMode) {
	switch(cLineEndingMode) {
		case 'w':
			boost::replace_all(strData, "\r", "");
			boost::replace_all(strData, "\n", "\r\n");
			break;
		case 'l':
			boost::replace_all(strData, "\r\n", "\n");
			break;
		case '.': // to ignore
			break;
		default: arx_assert_msg(false, "invalid LineEndingMode = '%c'", cLineEndingMode); break;
	}
}

void fixTo8859_15(std::string strFilename, std::string & strData) { // only chars that matters for now from the most used format UTF-8 only
	if(strData.find_first_of("\xC2\xE2\x82\xAC") != std::string::npos) {
		LogWarning << "fixing data to ISO-8859-15 read from '" << strFilename << "'";
		boost::replace_all(strData, "\xC2", ""); // UTF-8 seems to only prepend special chars with 0xC2, so remove it
		boost::replace_all(strData, "\xE2\x82\xAC", "\xA4"); // least this one, needs conversion
	}
}

std::string loadAndFixScriptData(std::string strFilename, std::ifstream & file, char cLineEndingMode) {
	return fixScriptData(strFilename, loadFileDataAndCloseIt(file), cLineEndingMode);
}
std::string loadFileDataAndCloseIt(std::ifstream & file) {
	std::stringstream fileData;
	fileData << file.rdbuf();
	file.close();
	return fileData.str();
}
std::string fixScriptData(std::string strFilename, std::string strData, char cLineEndingMode) {
	strData = util::toLowercase(strData);
	fixLineEnding(strData, cLineEndingMode);
	fixTo8859_15(strFilename, strData);
	return strData;
}

/* MultiLineCommentSectionToggleTrick commenting this line will uncomment the block just below it and auto comment the subsequent block!
void loadScript(EERIE_SCRIPT & script, res::path & pathScript) {
	loadScript(script, g_resources->getFile(pathScript), pathScript);
}
void loadScript(EERIE_SCRIPT & script, PakFile * file, res::path & pathScript) {
	if(!file) {
		return;
	}
	
	script.valid = true;
	
	script.data = util::toLowercase(file->read());
	
	ARX_SCRIPT_ComputeShortcuts(script);
}
/*/
void loadScript(EERIE_SCRIPT & script, res::path & pathScript) {
	loadScript(script, g_resources->getFile(pathScript), pathScript);
}
void loadScript(EERIE_SCRIPT & script, PakFile * fileInput, res::path & pathScript) {
	
	if(!fileInput) {
		return;
	}
	
	res::path pathModdedDump;
	pathModdedDump = std::string() + "modsdump/" + pathScript.string();
	
	script.valid = true;
	script.file = pathScript.string();
	
	std::string strScriptData;
	
	static const char * moddingOpt = std::getenv("ARX_MODDING"); // set ARX_MODDING=1 to let dumped scripts cache always be loaded, this will also ignore changes to mod files and to original files that would be patched/overriden
	int moddingMode = 0;
	if(moddingOpt) {
		moddingMode = util::parseInt(moddingOpt);
		if(moddingMode < 0) moddingMode = 0; // as the end user can cause this error, just auto-fix it.
	}
	static bool bShowModeOnce = true;
	if(bShowModeOnce) {
		LogInfo << "Modding mode (" << moddingMode << "): " << (moddingMode == 0 ? "using cached modded scripts if available" : "developer mode always apply patches, overrides and appends, letting you edit .asl files without restarting the game");
		bShowModeOnce = false;
	}
	
	bool usingFileFromCache = false;
	if(moddingMode == 0) {
		std::ifstream fileModCache(pathModdedDump.string());
		if (fileModCache.is_open()) {
			strScriptData = fixScriptData(pathModdedDump.string(), loadFileDataAndCloseIt(fileModCache), '.');
			script.file = pathModdedDump.string();
			usingFileFromCache = true;
		}
	}
	
	std::string strScriptDataOriginal; // there is no need to read original data if using the cache that is meant to be fast
	if(!usingFileFromCache) {
		strScriptDataOriginal = fileInput->read();
		strScriptData = strScriptDataOriginal;
		char cLineEndingMode = strScriptData.find("\r\n") != std::string::npos ? 'w' : 'l';
		
		strScriptData = fixScriptData(pathScript.string(), strScriptData, cLineEndingMode);
		
		std::string strBaseModPath = "mods";
		std::string strFlModLoadOrder = strBaseModPath + "/" + "modloadorder.cfg";
		static std::vector<std::string> vModList;
		static std::string strModListFileData;
		if(strModListFileData.size() == 0 || moddingMode) { // detects changes in the load order in case of modding mode
			std::ifstream flLoadOrder(strFlModLoadOrder);
			std::string strModListFileDataNew = loadFileDataAndCloseIt(flLoadOrder);
			if(strModListFileDataNew != strModListFileData) {
				if(vModList.size() > 0) {
					LogInfo << "Mod load order file change detected, reloading.";
				}
				vModList.clear();
				strModListFileData = strModListFileDataNew;
			}
		}
		if(vModList.size() == 0) {
			//the last in the list wins.
			std::ifstream flLoadOrder(strFlModLoadOrder);
			if (flLoadOrder.is_open()) {
				std::string line;
				LogInfo << "Mod load order file found: " << strFlModLoadOrder;
				while (std::getline(flLoadOrder, line)) {
					//strModListFileData += line + "\n"; // should detect \r\n
					if(line.size() == 0) continue; //empty lines are ignored
					if(line[0] == '#') continue; //lines beggining with # are comments and will be ignored
					vModList.push_back(line.c_str());
					LogInfo << " ├─ Mod: " << line;
				}
				flLoadOrder.close();
				LogInfo << " └─ Ended collecting mod load order.";
			}
		}
		
		//* 1st) Patches are meant to be applied at originals (of other mods) and vanilla scripts.
		//* 2nd) Overrides are prepended script code, they will be found before others.
		//* 3rd) Appended code shall have unique identifiers as will be at the end, is more a mod developer helper as it's contents could also be at overrides file as the final result wont change.
		//* To apply a patch in a script code override, create a new folder (mod) containing it and being loaded after.
		int logInfoForScript = 0;
		size_t modOverrideApplyCount = 0;
		size_t modPatchApplyCount = 0;
		size_t modAppendApplyCount = 0;
		for(std::string strMod : vModList) {
			std::string strModBase = std::string() + strBaseModPath + "/" + strMod + "/" + pathScript.string();
			int cleanTo = strBaseModPath.size() + 1 + strMod.size() + 1;
			res::path pathModOverride = strModBase + ".override.asl"; // the final .asl is to keep it easy to be detected by code editors
			res::path pathModPatch = strModBase + ".patch";
			res::path pathModAppend = strModBase + ".append.asl"; // this file is more useful to mod developers that want to split test code from the main mod file. combine with a patch to let the test code be reachable.
			int logInfoForMod = 0;
			int logInfoAppliedForMod = 0;
			
			// apply diff patch
			while(true) { // this loop let the user or mod developer take action w/o requiring to restart the game
				std::ifstream fileModPatch(pathModPatch.string());
				if (fileModPatch.is_open()) {
					if(logInfoForScript == 0) {
						LogInfo << "Modding script file: " << pathScript.string();
						logInfoForScript++;
					}
					if(logInfoForMod == 0) {
						LogInfo << "├─ Mod name: " << strMod;
						logInfoForMod++;
					}

					std::string strFileDataPatch = loadFileDataAndCloseIt(fileModPatch);
					
					res::path pathModPatchToApply;
					if(strFileDataPatch.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos) {
						std::ofstream fileModPatchLowerCase;
						res::path pathModPatchLowerCase = pathModPatch.string()+".lowercase.patch";
						fileModPatchLowerCase.open(pathModPatchLowerCase.string(), std::ios_base::trunc);
						if(fileModPatchLowerCase.fail()) {
							arx_assert_msg(false, "failed to write required lowercase patch file '%s'", pathModPatchLowerCase.string().c_str());
						}
						
						strFileDataPatch = fixScriptData(pathModPatch.string(), strFileDataPatch, cLineEndingMode);
						fileModPatchLowerCase << strFileDataPatch;
						fileModPatchLowerCase.flush();
						fileModPatchLowerCase.close();
						LogInfo << "│   ├─ lower case patch : " << pathModPatchLowerCase.string().substr(cleanTo);
						pathModPatchToApply = pathModPatchLowerCase;
					} else {
						pathModPatchToApply = pathModPatch;
					}
					
					res::path pathScriptToBePatched = pathModdedDump;
					writeScriptAtModDumpFolder(pathScriptToBePatched, strScriptData, strScriptDataOriginal);
					
					std::string strPatchOutputFile = pathModPatchToApply.string() + ".log";
					std::string strCmd = std::string() + "patch \"" + pathScriptToBePatched.string() + "\" \"" + pathModPatchToApply.string() + "\" 2>&1 >\"" + strPatchOutputFile + "\"";
					int retCmdPatch = platform::runUserCommand(strCmd.c_str());
					if(retCmdPatch != 0) { // these below are important to show also to end users installing mods, so they can remove the mod, update it, or try to fix by themselves.
						std::stringstream ssPatchingOutput;
						std::ifstream fileOutputMsg(strPatchOutputFile);
						if (fileOutputMsg.is_open()) {
							ssPatchingOutput << fileOutputMsg.rdbuf();
							fileOutputMsg.close();
						}
						
						// TODO terminal input is not capturable? is there some way to let it work w/o system windowed popup? while debugging with nemiver at least, none of these work to wait terminal user input (hitting enter does nothing, so it wont continue running, the app stays there forever): std::cin >> dummy; dummy = platform::runUserCommand("read"); getchar(); do { ... } while (std::cin.get() != '\n');
						std::string strTitle = "Modding";
						if(platform::askOkCancelCustomUserSystemPopupCommand(strTitle, std::string() + "ERROR: Applying a mod patch failed.\n [SCRIPT] '" + pathScriptToBePatched.string() + "'", ssPatchingOutput.str(), pathModPatch.string())) { // pathModPatch will be lower cased again
							platform::showInfoDialog(std::string() + "ArxLibertatis" + strTitle + "\n" + "After editing:\n [PATCH] '" + pathModPatch.string() + "'\nClose this dialog to retry the patch.", "ArxLibertatis" + strTitle); // TODO zenity is not showing the specified title
							continue;
						}
						
						LogError << "[Description] Failed to patch the script (err=" << retCmdPatch << ") '" << pathScriptToBePatched.string() << "' using the mod patch file '" << pathModPatchToApply.string() << "'. See the above output at '" << strPatchOutputFile << "'\n";
						#ifdef ARX_DEBUG
						LogError << "[PatchCommandOutput] " << ssPatchingOutput.str();
						#endif
						LogError << "[RequestUserAction] Fix, update or remove the patch. Retrying in 3s ...\n"; 
						Thread::sleep(3000ms);
						continue;
					}
					
					std::ifstream fileModPatched(pathScriptToBePatched.string());
					if (fileModPatched.is_open()) {
						strScriptData = loadAndFixScriptData(pathScriptToBePatched.string(), fileModPatched, cLineEndingMode);
						LogInfo << "│   ├─ applied patch    : " << pathModPatchToApply.string().substr(cleanTo);;
						modPatchApplyCount++;
					} else {
						arx_assert_msg(false, "failed to load the patched script '%s' after using the mod patch file '%s'", pathScriptToBePatched.string().c_str(), pathModPatchToApply.string().c_str());
					}
				}
				
				break;
			}
			
			// apply simple override. prepends script code for GoTo/GoSub calls and events. The last prepended wins.
			std::ifstream fileModOverride(pathModOverride.string());
			if (fileModOverride.is_open()) {
				if(logInfoForScript == 0) {
					LogInfo << "Modding script file: " << pathScript.string();
					logInfoForScript++;
				}
				if(logInfoForMod == 0) {
					LogInfo << "├─ Mod name: " << strMod;
					logInfoForMod++;
				}
				
				strScriptData = fixScriptData(pathModOverride.string(), loadFileDataAndCloseIt(fileModOverride) + "\n" + strScriptData, cLineEndingMode);
				LogInfo << "│   ├─ applied overrides: " << pathModOverride.string().substr(cleanTo);;
				modOverrideApplyCount++;
				logInfoAppliedForMod++;
			}
			
			// apply simple append. Should be accompained by a patch also to let the appended code be reachable.
			std::ifstream fileModAppend(pathModAppend.string());
			if (fileModAppend.is_open()) {
				if(logInfoForScript == 0) {
					LogInfo << "Modding script file: " << pathScript.string();
					logInfoForScript++;
				}
				if(logInfoForMod == 0) {
					LogInfo << "├─ Mod name: " << strMod;
					logInfoForMod++;
				}
				
				strScriptData = fixScriptData(pathModAppend.string(), strScriptData + "\n" + loadFileDataAndCloseIt(fileModAppend), cLineEndingMode);
				LogInfo << "│   ├─ applied append   : " << pathModAppend.string().substr(cleanTo);;
				modAppendApplyCount++;
				logInfoAppliedForMod++;
			}
			
			if(logInfoAppliedForMod > 0) {
				LogInfo << "│   └─ Ended applying all for: " << strMod;
				// else? LogWarning << "│   └─ Nothing found to apply at: " << strMod;
			}
		}
		
		if((modOverrideApplyCount + modPatchApplyCount + modAppendApplyCount) > 0) {
			writeScriptAtModDumpFolder(pathModdedDump, strScriptData, strScriptDataOriginal);
			script.file = pathModdedDump.string();
			LogInfo << "└─ All Mods: Dumping applied result(s) of " << modOverrideApplyCount << " override(s), " << modPatchApplyCount << " patch(es) and " << modAppendApplyCount << " append(s) at: " << pathModdedDump;
		}
	}
	
	if(detectAndTransformMultilineCommentIntoSingleLineComments(strScriptData, pathModdedDump)) {
		writeScriptAtModDumpFolder(pathModdedDump, strScriptData, strScriptDataOriginal);
	}
	
	script.data = strScriptData;
	
	ARX_SCRIPT_ComputeShortcuts(script);
	
}
//*/
