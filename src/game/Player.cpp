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

#include "game/Player.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <random>
#include <stddef.h>

#include <boost/random/uniform_int_distribution.hpp>

#include "animation/Animation.h"
#include "animation/AnimationRender.h"

#include "cinematic/CinematicController.h"

#include "ai/Anchors.h"
#include "ai/PathFinderManager.h"
#include "ai/Paths.h"

#include "core/Application.h"
#include "core/Localisation.h"
#include "core/GameTime.h"
#include "core/Core.h"

#include "game/Damage.h"
#include "game/EntityManager.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Item.h"
#include "game/Missile.h"
#include "game/NPC.h"
#include "game/effect/Quake.h"
#include "game/magic/Precast.h"
#include "game/spell/FlyingEye.h"
#include "game/spell/Cheat.h"

#include "gui/CharacterCreation.h"
#include "gui/Dragging.h"
#include "gui/Hud.h"
#include "gui/Menu.h"
#include "gui/Text.h"
#include "gui/Notification.h"
#include "gui/Speech.h"
#include "gui/Interface.h"
#include "gui/MiniMap.h"
#include "gui/hud/PlayerInventory.h"

#include "graphics/BaseGraphicsTypes.h"
#include "graphics/Color.h"
#include "graphics/Draw.h"
#include "graphics/GlobalFog.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Math.h"
#include "graphics/Renderer.h"
#include "graphics/Vertex.h"
#include "graphics/data/TextureContainer.h"
#include "graphics/effects/Decal.h"
#include "graphics/effects/Fade.h"
#include "graphics/effects/Fog.h"
#include "graphics/particle/ParticleManager.h"
#include "graphics/particle/ParticleEffects.h"
#include "graphics/particle/MagicFlare.h"
#include "graphics/particle/Spark.h"

#include "io/resource/ResourcePath.h"
#include "io/resource/PakReader.h"
#include "io/fs/Filesystem.h"
#include "io/log/Logger.h"

#include "math/Angle.h"
#include "math/Random.h"
#include "math/Vector.h"

#include "physics/Collisions.h"
#include "physics/Attractors.h"
#include "physics/Projectile.h"

#include "platform/Environment.h"
#include "platform/Platform.h"
#include "platform/profiler/Profiler.h"
#include "platform/Thread.h"

#include "scene/ChangeLevel.h"
#include "scene/Scene.h"
#include "scene/GameSound.h"
#include "scene/Interactive.h"
#include "scene/Light.h"
#include "scene/LoadLevel.h"
#include "scene/Object.h"

#include "script/Script.h"

extern bool REQUEST_SPEECH_SKIP;
extern bool DONT_ERASE_PLAYER;
extern bool GLOBAL_MAGIC_MODE;

static const float WORLD_GRAVITY = 0.1f;
static const float JUMP_GRAVITY = 0.02f;
static const float STEP_DISTANCE = 120.f;
static const float TARGET_DT = 1000.f / 30.f;

extern Vec3f PUSH_PLAYER_FORCE;
extern long COLLIDED_CLIMB_POLY;

static const float ARX_PLAYER_SKILL_STEALTH_MAX = 100.f;

ARXCHARACTER player;
EERIE_3DOBJ * hero = nullptr;
float currentdistance = 0.f;
float CURRENT_PLAYER_COLOR = 0;
AnimationDuration PLAYER_ROTATION = 0;

bool USE_PLAYERCOLLISIONS = true;
bool BLOCK_PLAYER_CONTROLS = false;
bool WILLRETURNTOCOMBATMODE = false;

static GameInstant LastHungerSample = 0;
static GameInstant ROTATE_START = 0;

// Player Anims FLAGS/Vars
ANIM_HANDLE * herowaitbook = nullptr;
ANIM_HANDLE * herowait_2h = nullptr;

std::vector<std::string> g_playerKeyring;

static unsigned long FALLING_TIME = 0;

std::vector<std::string> g_playerQuestLogEntries;

[[nodiscard]] bool ARXCHARACTER::hasAllRunes(const std::array<Rune, 6> & runes) const noexcept {
	return std::all_of(runes.begin(), runes.end(), [this](Rune rune) {
		return rune == RUNE_NONE || hasRune(rune);
	});
}

bool ARX_PLAYER_IsInFightMode() {
	arx_assert(entities.player());
	
	if(player.Interface & INTER_COMBATMODE) {
		return true;
	}
	
	const AnimLayer & layer1 = entities.player()->animlayer[1];
	
	if(layer1.cur_anim) {
		
		const auto & alist = entities.player()->anims;
		
		if(   layer1.cur_anim == alist[ANIM_BARE_READY]
		   || layer1.cur_anim == alist[ANIM_BARE_UNREADY]
		   || layer1.cur_anim == alist[ANIM_DAGGER_READY_PART_1]
		   || layer1.cur_anim == alist[ANIM_DAGGER_READY_PART_2]
		   || layer1.cur_anim == alist[ANIM_DAGGER_UNREADY_PART_1]
		   || layer1.cur_anim == alist[ANIM_DAGGER_UNREADY_PART_2]
		   || layer1.cur_anim == alist[ANIM_1H_READY_PART_1]
		   || layer1.cur_anim == alist[ANIM_1H_READY_PART_2]
		   || layer1.cur_anim == alist[ANIM_1H_UNREADY_PART_1]
		   || layer1.cur_anim == alist[ANIM_1H_UNREADY_PART_2]
		   || layer1.cur_anim == alist[ANIM_2H_READY_PART_1]
		   || layer1.cur_anim == alist[ANIM_2H_READY_PART_2]
		   || layer1.cur_anim == alist[ANIM_2H_UNREADY_PART_1]
		   || layer1.cur_anim == alist[ANIM_2H_UNREADY_PART_2]
		   || layer1.cur_anim == alist[ANIM_MISSILE_READY_PART_1]
		   || layer1.cur_anim == alist[ANIM_MISSILE_READY_PART_2]
		   || layer1.cur_anim == alist[ANIM_MISSILE_UNREADY_PART_1]
		   || layer1.cur_anim == alist[ANIM_MISSILE_UNREADY_PART_2]
		   )
			return true;
	}

	return false;
}

//! Init/Reset player Keyring structures
void ARX_KEYRING_Init() {
	g_playerKeyring.clear();
}

//! Add a key to Keyring
void ARX_KEYRING_Add(std::string_view key) {
	g_playerKeyring.emplace_back(key);
}

/*!
 * \return player "front pos" for sound purpose
 */
Vec3f ARX_PLAYER_FrontPos() {
	Vec3f pos = player.pos;
	pos += angleToVectorXZ(player.angle.getYaw()) * 100.f;
	pos += Vec3f(0.f, 100.f, 0.f); // XXX use -100 here ?
	return pos;
}

//! Reset all extra-rotation groups of player
void ARX_PLAYER_RectifyPosition() {
	arx_assert(entities.player());
	
	Entity * io = entities.player();
	if(io->_npcdata->ex_rotate) {
		for(Anglef & rotation : io->_npcdata->ex_rotate->group_rotate) {
			rotation = Anglef();
		}
	}
	
}

void ARX_PLAYER_KillTorch() {
	
	if(Entity * torch = player.torch) {
		ARX_SOUND_PlaySFX(g_snd.TORCH_END);
		ARX_SOUND_Stop(player.torch_loop);
		player.torch_loop = audio::SourcedSample();
		lightHandleGet(torchLightHandle)->m_exists = false;
		player.torch = nullptr;
		torch->updateOwner();
	}
	
}

void ARX_PLAYER_ClickedOnTorch(Entity * io) {
	
	if(!io) {
		return;
	}
	
	if(io->durability > 0 && (!player.torch || io != player.torch)) {
		// Remove the torch from the player inventory (and other ownerships) when equipping it
		// Do this early to make space for the unequipped old torch
		io->setOwner(nullptr);
	}
	
	if(Entity * oldTorch = player.torch) {
		InventoryPos pos = locateInInventories(oldTorch);
		oldTorch->setOwner(nullptr);
		giveToPlayer(oldTorch, pos);
		if(io == oldTorch) {
			return;
		}
	}
	
	if(io->durability > 0) {
		
		if(io->ignition > 0) {
			lightHandleDestroy(io->ignit_light);
			ARX_SOUND_Stop(io->ignit_sound);
			io->ignit_sound = audio::SourcedSample();
			io->ignition = 0;
		}
		
		ARX_SOUND_PlaySFX(g_snd.TORCH_START);
		player.torch_loop = ARX_SOUND_PlaySFX_loop(g_snd.TORCH_LOOP, nullptr, 1.f);
		
		player.torch = io;
		io->setOwner(entities.player());
		
	}
	
}

static void ARX_PLAYER_ManageTorch() {
	
	if(player.torch) {
		
		player.torch->ignition = 0;
		player.torch->durability -= g_framedelay * 0.0001f;
		
		if(player.torch->durability <= 0) {
			player.torch->destroy();
			arx_assert(player.torch_loop == audio::SourcedSample());
			arx_assert(lightHandleGet(torchLightHandle)->m_exists == false);
			arx_assert(!player.torch);
		}
		
	}
	
}

//! Init/Reset player Quest structures
void ARX_PLAYER_Quest_Init() {
	g_playerQuestLogEntries.clear();
	g_playerBook.clearJournal();
}

void ARX_Player_Rune_Add(RuneFlag rune) {
	
	size_t spellsBefore = std::count_if(spellicons.begin(), spellicons.end(), [](const SPELL_ICON & spell) {
		return !spell.bSecret && player.hasAllRunes(spell.symbols);
	});
	
	player.rune_flags |= rune;
	
	size_t spellsAfter = std::count_if(spellicons.begin(), spellicons.end(), [](const SPELL_ICON & spell) {
		return !spell.bSecret && player.hasAllRunes(spell.symbols);
	});
	
	if(spellsAfter > spellsBefore) {
		g_hudRoot.bookIconGui.requestFX();
		g_hudRoot.bookIconGui.requestHalo();
	}
	
}

void ARX_Player_Rune_Remove(RuneFlag rune) {
	player.rune_flags &= ~rune;
}

//! Add quest "quest" to player Questbook
void ARX_PLAYER_Quest_Add(std::string_view quest) {
	g_playerQuestLogEntries.emplace_back(quest);
	g_playerBook.clearJournal();
}

//! Removes player invisibility by killing Invisibility spells on him
void ARX_PLAYER_Remove_Invisibility() {
	spells.endByCaster(EntityHandle_Player, SPELL_INVISIBILITY);
}

static PlayerSkill getAttributeSkillModifiers(const PlayerAttribute & attribute) {
	
	PlayerSkill skillMod;
	
	skillMod.stealth         = attribute.dexterity * 2.f;
	skillMod.mecanism        = attribute.dexterity + attribute.mind;
	skillMod.intuition       = attribute.mind * 2.f;
	skillMod.etheralLink     = attribute.mind * 2.f;
	skillMod.objectKnowledge = attribute.mind * 1.5f + attribute.dexterity * 0.5f + attribute.strength * 0.5f;
	skillMod.casting         = attribute.mind * 2.f;
	skillMod.projectile      = attribute.dexterity * 2.f + attribute.strength;
	skillMod.closeCombat     = attribute.dexterity + attribute.strength * 2.f;
	skillMod.defense         = attribute.constitution * 3.f;
	
	return skillMod;
}

static PlayerMisc getMiscStats(const PlayerAttribute & attribute, const PlayerSkill & skill) {
	
	PlayerMisc stats;
	
	stats.armorClass   = std::floor(std::max(1.f, skill.defense * 0.1f - 1.0f));
	stats.resistMagic  = std::floor(attribute.mind * 2.f * (1.f + skill.casting * 0.005f)); // TODO why *?
	stats.resistPoison = std::floor(attribute.constitution * 2.f + skill.defense * 0.25f);
	stats.criticalHit  = attribute.dexterity * 2.f + skill.closeCombat * 0.2f - 18.f;
	stats.damages      = std::max(1.f, attribute.strength * 0.5f - 5.f);
	
	return stats;
}

/*!
 * \brief Compute secondary attributes for player
 */
static void ARX_PLAYER_ComputePlayerStats() {
	
	player.m_lifeMaxWithoutMods = player.m_attribute.constitution * (player.level + 2);
	player.m_manaMaxWithoutMods = player.m_attribute.mind * (player.level + 1);
	
}

/*!
 * \brief Compute FULL versions of player stats including Equiped Items and spells,
 *        and any other effect altering them.
 */
void ARX_PLAYER_ComputePlayerFullStats() {
	
	ARX_PLAYER_ComputePlayerStats();
	
	// Reset modifier values
	player.m_attributeMod = PlayerAttribute();
	player.m_skillMod = PlayerSkill();
	player.m_miscMod = PlayerMisc();
	
	// TODO why do this now and not after skills/stats have been calculated?
	ARX_EQUIPMENT_IdentifyAll();
	
	// TODO why not use relative modfiers?
	float fFullAimTime = getEquipmentBaseModifier(IO_EQUIPITEM_ELEMENT_AimTime);
	float fCalcHandicap = (player.m_attributeFull.dexterity - 10.f) * 20.f;
	
	player.Full_AimTime = std::chrono::duration<float, std::milli>(fFullAimTime);
	if(player.Full_AimTime <= 0) {
		player.Full_AimTime = 1500ms;
	}
	
	player.Full_AimTime -= std::chrono::duration<float, std::milli>(fCalcHandicap);
	if(player.Full_AimTime <= 1500ms) {
		player.Full_AimTime = 1500ms;
	}
	
	// TODO make these calculations moddable
	
	// External modifiers
	
	// Calculate for modifiers from spells
	{
		float armor = spells.getTotalSpellCasterLevelOnTarget(EntityHandle_Player, SPELL_ARMOR)
		              - spells.getTotalSpellCasterLevelOnTarget(EntityHandle_Player, SPELL_LOWER_ARMOR);
		player.m_miscMod.armorClass += armor;
	}
	{
		float bless = spells.getTotalSpellCasterLevelOnTarget(EntityHandle_Player, SPELL_BLESS)
		              - spells.getTotalSpellCasterLevelOnTarget(EntityHandle_Player, SPELL_CURSE);
		player.m_attributeMod.strength += bless;
		player.m_attributeMod.constitution += bless;
		player.m_attributeMod.dexterity += bless;
		player.m_attributeMod.mind += bless;
	}
	
	// Calculate for modifiers from cheats
	if(cur_mr == CHEAT_ENABLED) {
		PlayerAttribute attributeMod;
		attributeMod.strength = 1;
		attributeMod.mind = 10;
		attributeMod.constitution = 1;
		attributeMod.dexterity = 10;
		player.m_attributeMod.add(attributeMod);
		
		PlayerSkill skillMod;
		skillMod.stealth = 5;
		skillMod.mecanism = 5;
		skillMod.intuition = 100;
		skillMod.etheralLink = 100;
		skillMod.objectKnowledge = 100;
		skillMod.casting = 5;
		skillMod.projectile = 5;
		skillMod.closeCombat = 5;
		skillMod.defense = 100;
		player.m_skillMod.add(skillMod);
		
		PlayerMisc miscMod;
		miscMod.resistMagic = 100;
		miscMod.resistPoison = 100;
		miscMod.criticalHit = 5;
		miscMod.damages = 2;
		miscMod.armorClass = 100;
		player.m_miscMod.add(miscMod);
		
		player.Full_AimTime = 100ms;
	}
	if(cur_mx == CHEAT_ENABLED) {
		PlayerAttribute attributeMod;
		attributeMod.strength = 5;
		attributeMod.mind = 5;
		attributeMod.constitution = 5;
		attributeMod.dexterity = 5;
		player.m_attributeMod.add(attributeMod);
		
		PlayerSkill skillMod;
		skillMod.stealth = 50;
		skillMod.mecanism = 50;
		skillMod.intuition = 50;
		skillMod.etheralLink = 50;
		skillMod.objectKnowledge = 50;
		skillMod.casting = 50;
		skillMod.projectile = 50;
		skillMod.closeCombat = 50;
		skillMod.defense = 50;
		player.m_skillMod.add(skillMod);
		
		PlayerMisc miscMod;
		miscMod.resistMagic = 10;
		miscMod.resistPoison = 10;
		miscMod.criticalHit = 50;
		miscMod.damages = 10;
		miscMod.armorClass = 20;
		player.m_miscMod.add(miscMod);
		
		player.Full_AimTime = 100ms;
	}
	if(player.m_cheatPnuxActive) {
		PlayerAttribute attributeMod;
		attributeMod.strength = float(Random::get(0, 5));
		attributeMod.mind = float(Random::get(0, 5));
		attributeMod.constitution = float(Random::get(0, 5));
		attributeMod.dexterity = float(Random::get(0, 5));
		player.m_attributeMod.add(attributeMod);
		
		PlayerSkill skillMod;
		skillMod.stealth = float(Random::get(0, 20));
		skillMod.mecanism = float(Random::get(0, 20));
		skillMod.intuition = float(Random::get(0, 20));
		skillMod.etheralLink = float(Random::get(0, 20));
		skillMod.objectKnowledge = float(Random::get(0, 20));
		skillMod.casting = float(Random::get(0, 20));
		skillMod.projectile = float(Random::get(0, 20));
		skillMod.closeCombat = float(Random::get(0, 20));
		skillMod.defense = float(Random::get(0, 30));
		player.m_skillMod.add(skillMod);
		
		PlayerMisc miscMod;
		miscMod.resistMagic = float(Random::get(0, 20));
		miscMod.resistPoison = float(Random::get(0, 20));
		miscMod.criticalHit = float(Random::get(0, 20));
		miscMod.damages = float(Random::get(0, 20));
		miscMod.armorClass = float(Random::get(0, 20));
		player.m_miscMod.add(miscMod);
	}
	if(cur_rf == CHEAT_ENABLED) {
		PlayerAttribute attributeMod;
		attributeMod.mind = 10;
		player.m_attributeMod.add(attributeMod);
		
		PlayerSkill skillMod;
		skillMod.casting = 100;
		skillMod.etheralLink = 100;
		skillMod.objectKnowledge = 100;
		player.m_skillMod.add(skillMod);
		
		PlayerMisc miscMod;
		miscMod.resistMagic = 20;
		miscMod.resistPoison = 20;
		miscMod.damages = 1;
		miscMod.armorClass = 5;
		player.m_miscMod.add(miscMod);
	}
	
	
	/////////////////////////////////////////////////////////////////////////////////////
	// Attributes
	
	// Calculate base attributes
	PlayerAttribute attributeBase = player.m_attribute;
	
	// Calculate equipment modifiers for attributes
	player.m_attributeMod.strength += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_STRENGTH, attributeBase.strength
	);
	player.m_attributeMod.dexterity += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_DEXTERITY, attributeBase.dexterity
	);
	player.m_attributeMod.constitution += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_CONSTITUTION, attributeBase.constitution
	);
	player.m_attributeMod.mind += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_MIND, attributeBase.mind
	);
	
	// Calculate full alltributes
	player.m_attributeFull.strength = std::max(0.f, attributeBase.strength + player.m_attributeMod.strength);
	player.m_attributeFull.dexterity = std::max(0.f, attributeBase.dexterity + player.m_attributeMod.dexterity);
	player.m_attributeFull.constitution = std::max(0.f, attributeBase.constitution + player.m_attributeMod.constitution);
	player.m_attributeFull.mind = std::max(0.f, attributeBase.mind + player.m_attributeMod.mind);
	
	
	/////////////////////////////////////////////////////////////////////////////////////
	// Skills
	
	// Calculate base skills
	PlayerSkill skillBase = player.m_skill;
	skillBase.add(getAttributeSkillModifiers(player.m_attributeFull));
	
	// Calculate equipment modifiers for skills
	player.m_skillMod.stealth += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Stealth, skillBase.stealth
	);
	player.m_skillMod.mecanism += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Mecanism, skillBase.mecanism
	);
	player.m_skillMod.intuition += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Intuition, skillBase.intuition
	);
	player.m_skillMod.etheralLink += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Etheral_Link, skillBase.etheralLink
	);
	player.m_skillMod.objectKnowledge += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Object_Knowledge, skillBase.objectKnowledge
	);
	player.m_skillMod.casting += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Casting, skillBase.casting
	);
	player.m_skillMod.projectile += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Projectile, skillBase.projectile
	);
	player.m_skillMod.closeCombat += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Close_Combat, skillBase.closeCombat
	);
	player.m_skillMod.defense += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Defense, skillBase.defense
	);
	
	// Calculate full skills
	player.m_skillFull.stealth = skillBase.stealth + player.m_skillMod.stealth;
	player.m_skillFull.mecanism = skillBase.mecanism + player.m_skillMod.mecanism;
	player.m_skillFull.intuition = skillBase.intuition + player.m_skillMod.intuition;
	player.m_skillFull.etheralLink = skillBase.etheralLink + player.m_skillMod.etheralLink;
	player.m_skillFull.objectKnowledge = skillBase.objectKnowledge + player.m_skillMod.objectKnowledge;
	player.m_skillFull.casting = skillBase.casting + player.m_skillMod.casting;
	player.m_skillFull.projectile = skillBase.projectile + player.m_skillMod.projectile;
	player.m_skillFull.closeCombat = skillBase.closeCombat + player.m_skillMod.closeCombat;
	player.m_skillFull.defense = skillBase.defense + player.m_skillMod.defense;
	
	
	/////////////////////////////////////////////////////////////////////////////////////
	// Other stats
	
	// Calculate base stats
	PlayerMisc miscBase = getMiscStats(player.m_attributeFull, player.m_skillFull);
	
	// Calculate equipment modifiers for stats
	player.m_miscMod.armorClass += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Armor_Class, miscBase.armorClass
	);
	player.m_miscMod.resistMagic += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Resist_Magic, miscBase.resistMagic
	);
	player.m_miscMod.resistPoison += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Resist_Poison, miscBase.resistPoison
	);
	player.m_miscMod.criticalHit += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Critical_Hit, miscBase.criticalHit
	);
	player.m_miscMod.damages += getEquipmentModifier(
		IO_EQUIPITEM_ELEMENT_Damages, miscBase.damages
	);
	
	// Calculate full stats
	player.m_miscFull.armorClass = std::max(0.f, miscBase.armorClass + player.m_miscMod.armorClass);
	player.m_miscFull.resistMagic = std::max(0.f, miscBase.resistMagic + player.m_miscMod.resistMagic);
	player.m_miscFull.resistPoison = std::max(0.f, miscBase.resistPoison + player.m_miscMod.resistPoison);
	player.m_miscFull.criticalHit = std::max(0.f, miscBase.criticalHit + player.m_miscMod.criticalHit);
	player.m_miscFull.damages = std::max(1.f, miscBase.damages + player.m_miscMod.damages
	                                          + player.m_skillFull.closeCombat * 0.1f);
	
	
	/////////////////////////////////////////////////////////////////////////////////////
	
	player.Full_life = player.lifePool.current;
	player.lifePool.max = player.m_attributeFull.constitution * (player.level + 2);
	player.lifePool.current = std::min(player.lifePool.current, player.lifePool.max);
	player.manaPool.max = player.m_attributeFull.mind * (player.level + 1);
	player.manaPool.current = std::min(player.manaPool.current, player.manaPool.max);
}

/*!
 * \brief Creates a Fresh hero
 */
void ARX_PLAYER_MakeFreshHero() {
	
	player.m_attribute.strength = 6;
	player.m_attribute.mind = 6;
	player.m_attribute.dexterity = 6;
	player.m_attribute.constitution = 6;

	PlayerSkill skill;
	skill.stealth = 0;
	skill.mecanism = 0;
	skill.intuition = 0;
	skill.etheralLink = 0;
	skill.objectKnowledge = 0;
	skill.casting = 0;
	skill.projectile = 0;
	skill.closeCombat = 0;
	skill.defense = 0;
	
	player.m_skillOld = player.m_skill = skill;
	
	player.Attribute_Redistribute = 16;
	player.Skill_Redistribute = 18;

	player.level = 0;
	player.xp = 0;
	player.poison = 0.f;
	player.hunger = 100.f;
	player.skin = 0;
	if(entities.player()) {
		entities.player()->inventory->setBags(1);
	}
	
	ARX_PLAYER_ComputePlayerStats();
	player.rune_flags = 0;

	player.SpellToMemorize.bSpell = false;
}

void ARX_SPSound() {
	ARX_SOUND_PlayCinematic("kra_zoha_equip", false);
}

void ARX_PLAYER_MakeSpHero()
{
	ARX_SPSound();
	player.m_attribute.strength = 12;
	player.m_attribute.mind = 12;
	player.m_attribute.dexterity = 12;
	player.m_attribute.constitution = 12;

	PlayerSkill skill;
	skill.stealth = 5;
	skill.mecanism = 5;
	skill.intuition = 5;
	skill.etheralLink = 5;
	skill.objectKnowledge = 5;
	skill.casting = 5;
	skill.projectile = 5;
	skill.closeCombat = 5;
	skill.defense = 5;
	
	player.m_skillOld = player.m_skill = skill;

	player.Attribute_Redistribute = 6;
	player.Skill_Redistribute = 10;

	player.level = 1;
	player.xp = 0;
	player.poison = 0.f;
	player.hunger = 100.f;
	player.skin = MAX_CHEAT_PLAYER_SKIN;

	ARX_PLAYER_ComputePlayerStats();
	player.lifePool.current = player.m_lifeMaxWithoutMods;
	player.manaPool.current = player.m_manaMaxWithoutMods;

	player.rune_flags = RuneFlags::all();
	player.SpellToMemorize.bSpell = false;
	
	g_characterCreation.resetCheat();
}

/*!
 * \brief Creates an Average hero
 */
void ARX_PLAYER_MakeAverageHero() {
	
	ARX_PLAYER_MakeFreshHero();
	
	player.m_attribute.strength += 4;
	player.m_attribute.mind += 4;
	player.m_attribute.dexterity += 4;
	player.m_attribute.constitution += 4;
	
	player.m_skill.stealth += 2;
	player.m_skill.mecanism += 2;
	player.m_skill.intuition += 2;
	player.m_skill.etheralLink += 2;
	player.m_skill.objectKnowledge += 2;
	player.m_skill.casting += 2;
	player.m_skill.projectile += 2;
	player.m_skill.closeCombat += 2;
	player.m_skill.defense += 2;
	
	player.Attribute_Redistribute = 0;
	player.Skill_Redistribute = 0;
	
	player.level = 0;
	player.xp = 0;
	player.hunger = 100.f;
	
	ARX_PLAYER_ComputePlayerStats();
}

/*!
 * \brief Quickgenerate a random hero for a new play thru
 */
void ARX_PLAYER_QuickGeneration() {
	
	unsigned char old_skin = player.skin;
	ARX_PLAYER_MakeFreshHero();
	player.skin = old_skin;
	
	static std::string strPreferedRoleplayClassOrder = [](){return platform::getEnvironmentVariableValueString(strPreferedRoleplayClassOrder, "ARX_ScriptCodeEditorCommand", Logger::LogLevel::Info, "use 3 letters: w t m. warrior, thief and mage. ex.: \"mtw\" means mage will receive the best values, then thief and finally warrior.", "vanilla").getString();}();
	ARX_PLAYER_RandomizeRoleplayClass(18.f, 18.f, strPreferedRoleplayClassOrder);
	
	player.level = 0;
	player.xp = 0;
	player.hunger = 100.f;
	
	ARX_PLAYER_ComputePlayerStats();
}

bool ARX_PLAYER_ResetAttributesAndSkills(float fMinAttrs, float fMinSkills) { // fMinAttrs < 1 will not reset, fMinSkills < 0 wont reset
	float fSum = 0.f;
	float fMinSum = 0.f;
	
	// attributes
	if(fMinAttrs >= 1) {
		fSum =
			player.m_attribute.strength +
			player.m_attribute.mind +
			player.m_attribute.dexterity +
			player.m_attribute.constitution;
			
		fMinSum = (fMinAttrs * 4);
		if(fSum < 0) {
			LogError << "attributes sum " << fSum << " is less than requested " << fMinSum;
			return false;
		}
		float fRemaining = fSum - fMinSum;
		arx_assert(fRemaining <= 255);
		
		float fAdjust = (fRemaining - static_cast<int>(fRemaining)) / 9.f; // div by tot skills
		player.Attribute_Redistribute += static_cast<unsigned char>(fRemaining); // trunc
		player.m_attribute.strength =
			player.m_attribute.mind =
			player.m_attribute.dexterity =
			player.m_attribute.constitution = (fMinAttrs + fAdjust);
	}
	
	// skills
	if(fMinSkills >= 0) {
		fSum =
			player.m_skill.stealth +
			player.m_skill.mecanism +
			player.m_skill.intuition +
			player.m_skill.etheralLink +
			player.m_skill.objectKnowledge +
			player.m_skill.casting +
			player.m_skill.projectile +
			player.m_skill.closeCombat +
			player.m_skill.defense;
			
		fMinSum = (fMinSkills * 9);
		if(fSum < fMinSum) {
			LogError << "skills sum " << fSum << " is less than requested " << fMinSum;
			return false;
		}
		float fRemaining = fSum - fMinSum;
		arx_assert(fRemaining <= 255);
		
		float fAdjust = (fRemaining - static_cast<int>(fRemaining)) / 9.f; // div by tot skills
		player.Skill_Redistribute += static_cast<unsigned char>(fRemaining); // trunc
		player.m_skill.stealth =
			player.m_skill.mecanism =
			player.m_skill.intuition =
			player.m_skill.etheralLink =
			player.m_skill.objectKnowledge =
			player.m_skill.casting =
			player.m_skill.projectile =
			player.m_skill.closeCombat =
			player.m_skill.defense = (fMinSkills + fAdjust);
	}
	
	return true;
}

bool ARX_PLAYER_Randomize(float maxAttribute, float maxSkill) { // vanilla code, less random, more balanced towards random mixed class with not too discrepant values
	while(player.Attribute_Redistribute) {
		float rn = Random::getf();

		if(rn < 0.25f && player.m_attribute.strength < maxAttribute) {
			player.m_attribute.strength++;
			player.Attribute_Redistribute--;
		} else if(rn < 0.5f && player.m_attribute.mind < maxAttribute) {
			player.m_attribute.mind++;
			player.Attribute_Redistribute--;
		} else if(rn < 0.75f && player.m_attribute.dexterity < maxAttribute) {
			player.m_attribute.dexterity++;
			player.Attribute_Redistribute--;
		} else if(player.m_attribute.constitution < maxAttribute) {
			player.m_attribute.constitution++;
			player.Attribute_Redistribute--;
		} else {
			break; // if reaches here, some points will remain available
		}
	}

	while(player.Skill_Redistribute) {
		float rn = Random::getf();

		if(rn < 0.11f && player.m_skill.stealth < maxSkill) {
			player.m_skill.stealth++;
			player.Skill_Redistribute--;
		} else if(rn < 0.22f && player.m_skill.mecanism < maxSkill) {
			player.m_skill.mecanism++;
			player.Skill_Redistribute--;
		} else if(rn < 0.33f && player.m_skill.intuition < maxSkill) {
			player.m_skill.intuition++;
			player.Skill_Redistribute--;
		} else if(rn < 0.44f && player.m_skill.etheralLink < maxSkill) {
			player.m_skill.etheralLink++;
			player.Skill_Redistribute--;
		} else if(rn < 0.55f && player.m_skill.objectKnowledge < maxSkill) {
			player.m_skill.objectKnowledge++;
			player.Skill_Redistribute--;
		} else if(rn < 0.66f && player.m_skill.casting < maxSkill) {
			player.m_skill.casting++;
			player.Skill_Redistribute--;
		} else if(rn < 0.77f && player.m_skill.projectile < maxSkill) {
			player.m_skill.projectile++;
			player.Skill_Redistribute--;
		} else if(rn < 0.88f && player.m_skill.closeCombat < maxSkill) {
			player.m_skill.closeCombat++;
			player.Skill_Redistribute--;
		} else if(player.m_skill.defense < maxSkill) {
			player.m_skill.defense++;
			player.Skill_Redistribute--;
		} else {
			break; // if reaches here, some points will remain available
		}
	}
	
	return player.Skill_Redistribute > 0 || player.Attribute_Redistribute > 0;
}

bool ARX_PLAYER_RandomizeRoleplayClass(float maxAttribute, float maxSkill, std::string roleplayClassPreferedOrder) {
	if(roleplayClassPreferedOrder == "vanilla") {
		return ARX_PLAYER_Randomize(maxAttribute, maxSkill);
	}
	
	if(roleplayClassPreferedOrder.size() < 3 || roleplayClassPreferedOrder.find("m") == std::string::npos || roleplayClassPreferedOrder.find("w") == std::string::npos || roleplayClassPreferedOrder.find("t") == std::string::npos) {
		LogError << "invalid roleplayClassPreferedOrder = " << roleplayClassPreferedOrder << ". it must contain [m]age [w]arrior [t]hief in any order you prefer your roleplay classes to be set as Maximum Medium Minimum preference ex.: mwt means mage is perfered over warrior that is prefered over thief.";
		return false;
	}
	
	if(maxAttribute <= 0) LogWarning << "attributes won't be randomized if max <= 0.";
	if(maxSkill     <= 0) LogWarning <<     "skills won't be randomized if max <= 0.";
	
	static std::random_device rndDev;
	static std::mt19937 rng{rndDev()}; 
	static std::uniform_real_distribution<float> urd(0.0, 1.0);
	
	float sr = static_cast<int>(player.Skill_Redistribute);
	float sum = 0;
	if(maxSkill > 0) {
		while(true) {
			sum = 0;
			
			std::deque<float> rndSkills;
			for(int iTotSkills = 0; iTotSkills < 9; iTotSkills++) {
				float rnf = 0.f;
				int iTotRnd = Random::get(1, 10);
				for(int iR2 = 0; iR2 < iTotRnd; iR2++) rnf += urd(rng);
				rnf = static_cast<float>(std::fmod(rnf, 1.0));
				rndSkills.push_back(rnf * maxSkill);
				LogDebug("iTotRnd=" << iTotRnd << " rndSkill=" << rndSkills[rndSkills.size()-1]);
			}
			std::ranges::sort(rndSkills); //, std::ranges::greater());
			
			//PlayerSkill psBefore = player.m_skill;
			for(int iMinToMax = 2; iMinToMax >= 0; iMinToMax--) {
				std::vector<float> ps3;
				for(int i3 = 0; i3 < 3; i3++) {
					ps3.push_back(rndSkills[0]); // removes the 3 from the beggining
					rndSkills.pop_front();
				}
				int iIndex;
				switch(roleplayClassPreferedOrder[iMinToMax]) {
					case 't':
						sum += player.m_skill.stealth = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.mecanism = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.intuition = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						break;
					case 'm':
						sum += player.m_skill.etheralLink = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.objectKnowledge = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.casting = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						break;
					case 'w':
						sum += player.m_skill.projectile = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.closeCombat = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						sum += player.m_skill.defense = ps3[iIndex = Random::get(0, ps3.size()-1)];
						ps3.erase(ps3.begin() + iIndex);
						break;
				}
			}
			
			LogDebug( sum << "/" << sr << ", rpgOrder=" << roleplayClassPreferedOrder
				<< ", Ts=" << player.m_skill.stealth
				<< ", Tm=" << player.m_skill.mecanism
				<< ", Ti=" << player.m_skill.intuition
				<< ", Mel=" << player.m_skill.etheralLink
				<< ", Mok=" << player.m_skill.objectKnowledge
				<< ", Mcs=" << player.m_skill.casting
				<< ", Wp=" << player.m_skill.projectile
				<< ", Wcc=" << player.m_skill.closeCombat
				<< ", Wd=" << player.m_skill.defense
			);
			
			if(sum <= sr) break;
			
			LogInfo << "retrying random rolls (overflowed " << sum << " > " << sr << ")";
		}
		
		if(sum < sr) {
			float fRemaining = sr - sum;
			arx_assert(fRemaining <= 255);
			float fAdjust = (fRemaining - static_cast<int>(fRemaining)) / 9.f; // div by tot skills
			if(fAdjust > 0.f) {
				player.m_skill.stealth += fAdjust;
				player.m_skill.mecanism += fAdjust;
				player.m_skill.intuition += fAdjust;
				player.m_skill.etheralLink += fAdjust;
				player.m_skill.objectKnowledge += fAdjust;
				player.m_skill.casting += fAdjust;
				player.m_skill.projectile += fAdjust;
				player.m_skill.closeCombat += fAdjust;
				player.m_skill.defense += fAdjust;
			}
			player.Skill_Redistribute += static_cast<unsigned char>(fRemaining); // trunc
			if(player.Skill_Redistribute > 0) {
				LogInfo << "Distribute remaining skill points " << static_cast<int>(player.Skill_Redistribute) << " with vanilla algorithm."; // this is good to escape the requested class and add some unpredictness
				ARX_PLAYER_Randomize(maxAttribute, maxSkill);
			}
		}
		
		/*
		while(iSR > 0) {
			//Thread::sleep(PlatformDuration((1s * Random::getf())/10.f));
			//float rn = (Random::getf() + Random::getf() + Random::getf() + Random::getf() + Random::getf()) / 5;
			static std::random_device rndDev;
			static std::mt19937 rng{rndDev()}; 
			static std::uniform_int_distribution<int> urd(0,100);
			//float rn = (urd(rng) + urd(rng) + urd(rng)) / 3000.f;
			int rni;
			int iTot = Random::get(1, 10);
			for(int i = 0; i < iTot; i++) rni += urd(rng);
			rni %= 100;
			//rn /= iTot * 10.f;
			float rn = rni / 10.f;
			
			for(int iMinToMax = 0; iMinToMax < 3 && iSR > 0; iMinToMax++) {
				switch(cOrder[iMinToMax]) { // first try lower chances as vanilla
					case 't':
						if(rn < fThief[1] && player.m_skill.stealth < maxSkill) {
							player.m_skill.stealth++;
							iSR--;
						} else if(rn < fThief[2] && player.m_skill.mecanism < maxSkill) {
							player.m_skill.mecanism++;
							iSR--;
						} else if(rn < fThief[3] && player.m_skill.intuition < maxSkill) {
							player.m_skill.intuition++;
							iSR--;
						}
						break;
					case 'm':
						if(rn < fMage[1] && player.m_skill.etheralLink < maxSkill) {
							player.m_skill.etheralLink++;
							iSR--;
						} else if(rn < fMage[2] && player.m_skill.objectKnowledge < maxSkill) {
							player.m_skill.objectKnowledge++;
							iSR--;
						} else if(rn < fMage[3] && player.m_skill.casting < maxSkill) {
							player.m_skill.casting++;
							iSR--;
						}
						break;
					case 'w':
						if(rn < fWarrior[1] && player.m_skill.projectile < maxSkill) {
							player.m_skill.projectile++;
							iSR--;
						} else if(rn < fWarrior[2] && player.m_skill.closeCombat < maxSkill) {
							player.m_skill.closeCombat++;
							iSR--;
						} else if(rn < fWarrior[3] && player.m_skill.defense < maxSkill) {
							player.m_skill.defense++;
							iSR--;
						}
						break;
				}
			}
			
			LogDebug( iSR << ", rnd=" << rn << ", iTot=" << iTot
				<< ", Ts=" << player.m_skill.stealth
				<< ", Tm=" << player.m_skill.mecanism
				<< ", Ti=" << player.m_skill.intuition
				<< ", Mel=" << player.m_skill.etheralLink
				<< ", Mok=" << player.m_skill.objectKnowledge
				<< ", Mcs=" << player.m_skill.casting
				<< ", Wp=" << player.m_skill.projectile
				<< ", Wcc=" << player.m_skill.closeCombat
				<< ", Wd=" << player.m_skill.defense
			);
			
			if(
				player.m_skill.stealth >= maxSkill &&
				player.m_skill.mecanism >= maxSkill &&
				player.m_skill.intuition >= maxSkill &&
				player.m_skill.etheralLink >= maxSkill &&
				player.m_skill.objectKnowledge >= maxSkill &&
				player.m_skill.casting >= maxSkill &&
				player.m_skill.projectile >= maxSkill &&
				player.m_skill.closeCombat >= maxSkill &&
				player.m_skill.defense >= maxSkill
			) {
				arx_assert(
					player.m_skill.stealth == maxSkill &&
					player.m_skill.mecanism == maxSkill &&
					player.m_skill.intuition == maxSkill &&
					player.m_skill.etheralLink == maxSkill &&
					player.m_skill.objectKnowledge == maxSkill &&
					player.m_skill.casting == maxSkill &&
					player.m_skill.projectile == maxSkill &&
					player.m_skill.closeCombat == maxSkill &&
					player.m_skill.defense == maxSkill
				);
				arx_assert(
					player.m_skill.stealth +
					player.m_skill.mecanism +
					player.m_skill.intuition +
					player.m_skill.etheralLink +
					player.m_skill.objectKnowledge +
					player.m_skill.casting +
					player.m_skill.projectile +
					player.m_skill.closeCombat +
					player.m_skill.defense
					<= 255 );
				break;
			}
		}
		*/
		/*
		arx_assert(
			player.m_skill.stealth +
			player.m_skill.mecanism +
			player.m_skill.intuition +
			player.m_skill.etheralLink +
			player.m_skill.objectKnowledge +
			player.m_skill.casting +
			player.m_skill.projectile +
			player.m_skill.closeCombat +
			player.m_skill.defense
			<= 255 );
		arx_assert(iSR >= 0);
		player.Skill_Redistribute = static_cast<unsigned char>(iSR);
		*/
	}
	
	int iAR = static_cast<int>(player.Attribute_Redistribute);
	if(maxAttribute > 0) {
		while(iAR > 0) {
			std::vector<float> StrMndDex = {0.f, 0.f, 0.f};
			float fPref = 1.f;
			for(int iMinToMax = 2; iMinToMax >= 0; iMinToMax--) {
				switch(roleplayClassPreferedOrder[iMinToMax]) {
					case 'w': StrMndDex[0] = (fPref * 0.25f); break;
					case 'm': StrMndDex[1] = (fPref * 0.25f); break;
					case 't': StrMndDex[2] = (fPref * 0.25f); break;
				}
				fPref += 1.f;
			}
			
			float rn = urd(rng); // for time based seed could add: Thread::sleep(PlatformDuration((1s * Random::getf())/10.f));
			if(rn < StrMndDex[0] && player.m_attribute.strength < maxAttribute) {
				player.m_attribute.strength++;
				iAR--;
			} else if(rn < StrMndDex[1] && player.m_attribute.mind < maxAttribute) {
				player.m_attribute.mind++;
				iAR--;
			} else if(rn < StrMndDex[2] && player.m_attribute.dexterity < maxAttribute) {
				player.m_attribute.dexterity++;
				iAR--;
			} else if(player.m_attribute.constitution < maxAttribute) {
				player.m_attribute.constitution++;
				iAR--;
			}
			
			if(
				player.m_attribute.strength >= maxAttribute &&
				player.m_attribute.mind >= maxAttribute &&
				player.m_attribute.dexterity >= maxAttribute &&
				player.m_attribute.constitution >= maxAttribute
			) {
				arx_assert(
					player.m_attribute.strength == maxAttribute &&
					player.m_attribute.mind == maxAttribute &&
					player.m_attribute.dexterity == maxAttribute &&
					player.m_attribute.constitution == maxAttribute
				);
				arx_assert(
					player.m_attribute.strength +
					player.m_attribute.mind +
					player.m_attribute.dexterity +
					player.m_attribute.constitution
					<= 255 );
				break;
			}
		}
		arx_assert(iAR >= 0);
		player.Attribute_Redistribute = static_cast<unsigned char>(iAR);
	}
	
	return sr > 0 || iAR > 0;
}

/*!
 * \brief Returns necessary Experience for a given level
 */
long GetXPforLevel(short level)
{
	const long XP_FOR_LEVEL[] = {
		0,
		2000,
		4000,
		6000,
		10000,
		16000,
		26000,
		42000,
		68000,
		110000,
		178000,
		300000,
		450000,
		600000,
		750000
	};

	long xpNeeded;
	if(level < short(std::size(XP_FOR_LEVEL)))
		xpNeeded = XP_FOR_LEVEL[level];
	else
		xpNeeded = level * 60000;
	return xpNeeded;
}

/*!
 * \brief Manages Player Level Up event
 */
static void ARX_PLAYER_LEVEL_UP() {
	ARX_SOUND_PlayInterface(g_snd.PLAYER_LEVEL_UP);
	player.level++;
	player.Skill_Redistribute += 15;
	player.Attribute_Redistribute++;
	ARX_PLAYER_ComputePlayerStats();
	player.lifePool.current = player.m_lifeMaxWithoutMods;
	player.manaPool.current = player.m_manaMaxWithoutMods;
	player.m_skillOld = player.m_skill;
	SendIOScriptEvent(nullptr, entities.player(), "level_up");
}

/*!
 * \brief Modify player XP by adding "val" to it
 */
void ARX_PLAYER_Modify_XP(long val) {
	
	player.xp += val;
	
	for(short i = player.level + 1; i < 11; i++) {
		if(player.xp >= GetXPforLevel(i)) {
			ARX_PLAYER_LEVEL_UP();
		}
	}
}

/*!
 * \brief Function to poison player by "val" poison level
 */
void ARX_PLAYER_Poison(float val) {
	// Make a poison saving throw to see if player is affected
	if(Random::getf(0.f, 100.f) > player.m_miscFull.resistPoison) {
		player.poison += val;
		ARX_SOUND_PlayInterface(g_snd.PLAYER_POISONED);
	}
}

/*!
 * \brief updates some player stats depending on time
 *
 * Updates: life/mana recovery, poison evolution, hunger, invisibility
 */
void ARX_PLAYER_FrameCheck(PlatformDuration delta) {
	
	ARX_PROFILE_FUNC();
	
	if(delta > 0) {
		
		float Framedelay = toMsf(delta);
		
		UpdateIOInvisibility(entities.player());
		// Natural LIFE recovery
		float inc = 0.00008f * Framedelay * (player.m_attributeFull.constitution + player.m_attributeFull.strength * 0.5f + player.m_skillFull.defense) * 0.02f;
		
		if(player.lifePool.current > 0.f) {
			float inc_hunger = 0.00008f * Framedelay * (player.m_attributeFull.constitution + player.m_attributeFull.strength * 0.5f) * 0.02f;

			// Check for player hungry sample playing
			if((player.hunger > 10.f && player.hunger - inc_hunger <= 10.f)
			   || (player.hunger < 10.f && g_gameTime.now() > LastHungerSample + 180s)) {
				
				LastHungerSample = g_gameTime.now();

				if(!BLOCK_PLAYER_CONTROLS) {
					if(!getSpeechForEntity(*entities.player())) {
						ARX_SPEECH_AddSpeech(*entities.player(), "player_off_hungry", ANIM_TALK_NEUTRAL, ARX_SPEECH_FLAG_NOTEXT);
					}
				}
			}

			player.hunger -= inc_hunger * .5f;

			if(player.hunger < -10.f)
				player.hunger = -10.f;

			if(!BLOCK_PLAYER_CONTROLS) {
				if(player.hunger < 0.f)
					player.lifePool.current -= inc * 0.5f;
				else
					player.lifePool.current += inc;
			}
			
			// Natural MANA recovery
			float recoveredMana = 0.0000008f * Framedelay * ((player.m_attributeFull.mind + player.m_skillFull.etheralLink) * 10);
			
			player.manaPool.current = std::min(player.manaPool.current + recoveredMana, player.manaPool.max);
		}
		
		player.lifePool.current = std::min(player.lifePool.current, player.lifePool.max);
		
		// Now Checks Poison Progression
		if(!BLOCK_PLAYER_CONTROLS)
			if(player.poison > 0.f) {
				float cp = player.poison * Framedelay * 0.00025f;
				float faster = 10.f - player.poison;
				if(faster < 0.f) {
					faster = 0.f;
				}
				if(Random::getf(0.f, 100.f) > player.m_miscFull.resistPoison + faster) {
					float dmg = cp * (1.0f / 3);
					if(player.lifePool.current - dmg <= 0.f) {
						damagePlayer(dmg, DAMAGE_TYPE_POISON);
					} else {
						player.lifePool.current -= dmg;
					}
					player.poison -= cp * 0.1f;
				} else {
					player.poison -= cp;
				}
			}

		if(player.poison < 0.1f)
			player.poison = 0.f;
	}
}
TextureContainer * PLAYER_SKIN_TC = nullptr;

void ARX_PLAYER_Restore_Skin() {
	
	res::path tx;
	res::path tx2;
	res::path tx3;
	res::path tx4;

	switch(player.skin) {
		case 0:
			tx  = "graph/obj3d/textures/npc_human_base_hero_head";
			tx2 = "graph/obj3d/textures/npc_human_chainmail_hero_head";
			tx3 = "graph/obj3d/textures/npc_human_chainmail_mithril_hero_head";
			tx4 = "graph/obj3d/textures/npc_human_leather_hero_head";
			break;
		case MAX_CHEAT_PLAYER_SKIN:
			tx  = "graph/obj3d/textures/npc_human_cm_hero_head";
			tx2 = "graph/obj3d/textures/npc_human_chainmail_hero_head";
			tx3 = "graph/obj3d/textures/npc_human_chainmail_mithril_hero_head";
			tx4 = "graph/obj3d/textures/npc_human_leather_hero_head";
			break;
		case EXTRA_PLAYER_SKIN:
			tx  = "graph/obj3d/textures/npc_human__base_hero_head";
			tx2 = "graph/obj3d/textures/npc_human_chainmail_hero_head";
			tx3 = "graph/obj3d/textures/npc_human_chainmail_mithril_hero_head";
			tx4 = "graph/obj3d/textures/npc_human_leather_hero_head";
			break;
		default : {
				std::string skinIndex = std::to_string(static_cast<int>(player.skin + 1));
				tx  = "graph/obj3d/textures/npc_human_base_hero" + skinIndex + "_head";
				tx2 = "graph/obj3d/textures/npc_human_chainmail_hero" + skinIndex + "_head";
				tx3 = "graph/obj3d/textures/npc_human_chainmail_mithril_hero" + skinIndex + "_head";
				tx4 = "graph/obj3d/textures/npc_human_leather_hero" + skinIndex + "_head";
			}
	}

	TextureContainer * tmpTC;
	
	// TODO maybe it would be better to replace the textures in the player object instead of replacing the texture data for all objects that use these textures

	if(PLAYER_SKIN_TC && !tx.empty())
		PLAYER_SKIN_TC->LoadFile(tx);

	tmpTC = TextureContainer::Find("graph/obj3d/textures/npc_human_chainmail_hero_head");
	if(tmpTC && !tx2.empty())
		tmpTC->LoadFile(tx2);

	tmpTC = TextureContainer::Find("graph/obj3d/textures/npc_human_chainmail_mithril_hero_head");
	if(tmpTC && !tx3.empty())
		tmpTC->LoadFile(tx3);

	tmpTC = TextureContainer::Find("graph/obj3d/textures/npc_human_leather_hero_head");
	if(tmpTC && !tx4.empty())
		tmpTC->LoadFile(tx4);
}

/*!
 * \brief Load Mesh & anims for hero
 */
void ARX_PLAYER_LoadHeroAnimsAndMesh() {
	
	hero = loadObject("graph/obj3d/interactive/npc/human_base/human_base.teo", false).release();
	PLAYER_SKIN_TC = TextureContainer::Load("graph/obj3d/textures/npc_human_base_hero_head");
	
	herowaitbook = EERIE_ANIMMANAGER_Load("graph/obj3d/anims/npc/human_wait_book.tea");
	EERIE_ANIMMANAGER_Load("graph/obj3d/anims/npc/human_normal_wait.tea");
	herowait_2h = EERIE_ANIMMANAGER_Load("graph/obj3d/anims/npc/human_wait_book_2handed.tea");
	
	Entity * io = new Entity("graph/obj3d/interactive/player/player", EntityInstance(-1));
	arx_assert_msg(io->index() == EntityHandle_Player, "player entity didn't get index 0");
	arx_assert(entities.player() == io);
	arx_assert(io->idString() == "player");
	
	io->obj = hero;
	
	player.skin = 0;
	ARX_PLAYER_Restore_Skin();
	ARX_INTERACTIVE_Show_Hide_1st(entities.player(), false);
	ARX_INTERACTIVE_HideGore(entities.player(), false);
	
	ANIM_Set(player.bookAnimation[0], herowaitbook);
	player.bookAnimation[0].flags |= EA_LOOP;
	
	io->_npcdata = new IO_NPCDATA;
	
	io->ioflags = IO_NPC;
	io->_npcdata->lifePool.max = io->_npcdata->lifePool.current = 10.f;
	io->_npcdata->vvpos = -99999.f;
	
	io->armormaterial = "leather";
	res::path pathPlayerScript = "graph/obj3d/interactive/player/player.asl";
	loadScript(io->script, pathPlayerScript);
	
	if(EERIE_OBJECT_GetGroup(io->obj, "head") &&
	   EERIE_OBJECT_GetGroup(io->obj, "neck") &&
	   EERIE_OBJECT_GetGroup(io->obj, "chest") &&
	   EERIE_OBJECT_GetGroup(io->obj, "belt")) {
		io->_npcdata->ex_rotate = new EERIE_EXTRA_ROTATE();
		io->_npcdata->ex_rotate->group_number[0] = EERIE_OBJECT_GetGroup(io->obj, "head");
		io->_npcdata->ex_rotate->group_number[1] = EERIE_OBJECT_GetGroup(io->obj, "neck");
		io->_npcdata->ex_rotate->group_number[2] = EERIE_OBJECT_GetGroup(io->obj, "chest");
		io->_npcdata->ex_rotate->group_number[3] = EERIE_OBJECT_GetGroup(io->obj, "belt");
		io->_npcdata->ex_rotate->group_number[4] = EERIE_OBJECT_GetGroup(io->obj, "left_shoulder");
		io->_npcdata->ex_rotate->group_number[5] = EERIE_OBJECT_GetGroup(io->obj, "right_shoulder");
		for(Anglef & rotation : io->_npcdata->ex_rotate->group_rotate) {
			rotation = Anglef();
		}
	}
	
	io->inventory = std::make_unique<Inventory>(io, Vec2s(16, 3));
	
	ARX_INTERACTIVE_RemoveGoreOnIO(entities.player());
	
}

float Falling_Height = 0;

static void ARX_PLAYER_StartFall() {
	
	FALLING_TIME = 1;
	Falling_Height = 50.f;
	EERIEPOLY * ep = CheckInPoly(player.pos);

	if(ep) {
		Falling_Height = player.pos.y;
	}
}

/*!
 * \brief Called When player has just died
 */
void ARX_PLAYER_BecomesDead() {
	arx_assert(entities.player());
		
	// a mettre au final
	BLOCK_PLAYER_CONTROLS = true;
	
	player.Interface = 0;
	g_note.clear();
	player.DeadTime = 0;
	
	spells.endByCaster(EntityHandle_Player);
}

float LASTPLAYERA = 0;
extern long ON_PLATFORM;
long LAST_ON_PLATFORM = 0;
extern long MOVE_PRECEDENCE;
extern bool EXTERNALVIEW;

static void ARX_PLAYER_Manage_Visual_End(ANIM_HANDLE * request0_anim, ANIM_HANDLE * request3_anim,
                                         bool request0_loop, bool request0_stopend) {
	
	Entity * io = entities.player();
	
	AnimLayer & layer0 = io->animlayer[0];
	AnimLayer & layer3 = io->animlayer[3];
	
	if(request0_anim && request0_anim != layer0.cur_anim) {
		AcquireLastAnim(io);
		ResetAnim(layer0);
		layer0.cur_anim = request0_anim;
		layer0.flags = EA_STATICANIM;
		
		if(request0_loop)
			layer0.flags |= EA_LOOP;
		
		if(request0_stopend)
			layer0.flags |= EA_STOPEND;
		
		if(request0_anim == io->anims[ANIM_U_TURN_LEFT]
		   || request0_anim == io->anims[ANIM_U_TURN_RIGHT]
		   || request0_anim == io->anims[ANIM_U_TURN_RIGHT_FIGHT]
		   || request0_anim == io->anims[ANIM_U_TURN_LEFT_FIGHT]
		) {
			layer0.flags |= EA_EXCONTROL;
		}
	}
	
	if(request3_anim && request3_anim != layer3.cur_anim) {
		AcquireLastAnim(io);
		ResetAnim(layer3);
		layer3.cur_anim = request3_anim;
		layer3.flags = EA_STATICANIM;
	}
	
	io->physics = player.physics;
	
	player.m_lastMovement = player.m_currentMovement;
	
}

/*!
 * \brief Choose the set of animations to use to represent current player situation.
 */
void ARX_PLAYER_Manage_Visual() {
	arx_assert(entities.player());
	
	ARX_PROFILE_FUNC();
	
	GameInstant now = g_gameTime.now();
	
	if(player.m_currentMovement & PLAYER_ROTATE) {
		if(ROTATE_START == 0) {
			ROTATE_START = now;
		}
	} else if(ROTATE_START != 0) {
		GameDuration elapsed = now - ROTATE_START;
		if(elapsed > 100ms) {
			ROTATE_START = 0;
		}
	}
	
	Entity * io = entities.player();
	
	if(!BLOCK_PLAYER_CONTROLS && cur_mx == CHEAT_ENABLED) {
		io->halo.color = Color3f::red;
		io->halo.flags |= HALO_ACTIVE;
		io->halo.radius = 20.f;
		player.lifePool.current = std::min(player.lifePool.current + g_framedelay * 0.1f, player.lifePool.max);
		player.manaPool.current = std::min(player.manaPool.current + g_framedelay * 0.1f, player.manaPool.max);
	}
	
	if(cur_mr == CHEAT_ENABLED) {
		player.lifePool.current = std::min(player.lifePool.current + g_framedelay * 0.05f, player.lifePool.max);
		player.manaPool.current = std::min(player.manaPool.current + g_framedelay * 0.05f, player.manaPool.max);
	}
	
	io->pos = player.basePosition();
	
	if(player.jumpphase == NotJumping && !LAST_ON_PLATFORM) {
		float t;
		EERIEPOLY * ep = CheckInPoly(player.pos, &t);
		if(ep && io->pos.y > t - 30.f && io->pos.y < t) {
			player.onfirmground = true;
		}
	}
	
	ComputeVVPos(io);
	io->pos.y = io->_npcdata->vvpos;
	
	if(!(player.m_currentMovement & PLAYER_CROUCH) && player.physics.cyl.height > -150.f) {
		float old = player.physics.cyl.height;
		player.physics.cyl.height = player.baseHeight();
		player.physics.cyl.origin = player.basePosition();
		float anything = CheckAnythingInCylinder(player.physics.cyl, entities.player());
		if(anything < 0.f) {
			player.m_currentMovement |= PLAYER_CROUCH;
			player.physics.cyl.height = old;
		}
	}
	
	if(player.lifePool.current > 0) {
		io->angle = Anglef(0.f, 180.f - player.angle.getYaw(), 0.f);
	}
	
	io->gameFlags |= GFLAG_ISINTREATZONE;
	
	AnimLayer & layer0 = io->animlayer[0];
	const AnimLayer & layer1 = io->animlayer[1];
	AnimLayer & layer3 = io->animlayer[3];
	
	const auto & alist = io->anims;
	
	if(layer0.flags & EA_FORCEPLAY) {
		if(layer0.flags & EA_ANIMEND) {
			layer0.flags &= ~EA_FORCEPLAY;
			layer0.flags |= EA_STATICANIM;
			io->move = io->lastmove = Vec3f(0.f);
		} else {
			layer0.flags &= ~EA_STATICANIM;
			player.pos = g_moveto = player.pos + io->move;
			io->pos = player.basePosition();
			player.m_lastMovement = player.m_currentMovement;
			return;
		}
	}
	
	ANIM_HANDLE * request0_anim = nullptr;
	ANIM_HANDLE * request3_anim = nullptr;
	bool request0_loop = true;
	
	if(io->ioflags & IO_FREEZESCRIPT) {
		player.m_lastMovement = player.m_currentMovement;
		return;
	}
	
	if(player.lifePool.current <= 0) {
		HERO_SHOW_1ST = -1;
		io->animlayer[1].cur_anim = nullptr;
		ARX_PLAYER_Manage_Visual_End(alist[ANIM_DIE], request3_anim, false, true);
		return;
	}
	
	if(   player.m_currentMovement == 0
	   || player.m_currentMovement == PLAYER_MOVE_STEALTH
	   || (player.m_currentMovement & PLAYER_ROTATE)
	) {
		if(player.Interface & INTER_COMBATMODE) {
			request0_anim = alist[ANIM_FIGHT_WAIT];
		} else if(EXTERNALVIEW) {
			request0_anim = alist[ANIM_WAIT];
		} else {
			request0_anim = alist[ANIM_WAIT_SHORT];
		}
		
		request0_loop = true;
	}
	
	if(ROTATE_START != 0
	   && player.angle.getPitch() > 60.f
	   && player.angle.getPitch() < 180.f
	   && LASTPLAYERA > 60.f
	   && LASTPLAYERA < 180.f
	) {
		if(PLAYER_ROTATION < 0) {
			if(player.Interface & INTER_COMBATMODE)
				request0_anim = alist[ANIM_U_TURN_LEFT_FIGHT];
			else
				request0_anim = alist[ANIM_U_TURN_LEFT];
		} else {
			if(player.Interface & INTER_COMBATMODE)
				request0_anim = alist[ANIM_U_TURN_RIGHT_FIGHT];
			else
				request0_anim = alist[ANIM_U_TURN_RIGHT];
		}
		
		request0_loop = true;
		
		if(layer0.cur_anim == alist[ANIM_U_TURN_LEFT] || layer0.cur_anim == alist[ANIM_U_TURN_LEFT_FIGHT]) {
			layer0.ctime -= PLAYER_ROTATION;
			if(layer0.ctime < 0) {
				layer0.ctime = 0;
			}
		} else if(layer0.cur_anim == alist[ANIM_U_TURN_RIGHT]
		          || layer0.cur_anim == alist[ANIM_U_TURN_RIGHT_FIGHT]) {
			layer0.ctime += PLAYER_ROTATION;
			if(layer0.ctime < 0) {
				layer0.ctime = 0;
			}
		}
	}
	
	LASTPLAYERA = player.angle.getPitch();
	
	{
	long tmove = player.m_currentMovement;
	
	if((tmove & PLAYER_MOVE_STRAFE_LEFT) && (tmove & PLAYER_MOVE_STRAFE_RIGHT)) {
		tmove &= ~PLAYER_MOVE_STRAFE_LEFT;
		tmove &= ~PLAYER_MOVE_STRAFE_RIGHT;
	}
	
	if(MOVE_PRECEDENCE == PLAYER_MOVE_STRAFE_LEFT)
		tmove &= ~PLAYER_MOVE_STRAFE_RIGHT;
	
	if(MOVE_PRECEDENCE == PLAYER_MOVE_STRAFE_RIGHT)
		tmove &= ~PLAYER_MOVE_STRAFE_LEFT;
	
	if(MOVE_PRECEDENCE == PLAYER_MOVE_WALK_FORWARD)
		tmove &= ~PLAYER_MOVE_WALK_BACKWARD;
	
	if(player.m_currentMovement & PLAYER_MOVE_WALK_FORWARD)
		tmove = PLAYER_MOVE_WALK_FORWARD;
	
	if(tmove & PLAYER_MOVE_STRAFE_LEFT) {
		if(player.Interface & INTER_COMBATMODE)
			request0_anim = alist[ANIM_FIGHT_STRAFE_LEFT];
		else if(player.m_currentMovement & PLAYER_MOVE_STEALTH)
			request0_anim = alist[ANIM_STRAFE_LEFT];
		else
			request0_anim = alist[ANIM_STRAFE_RUN_LEFT];
	}
	
	if(tmove & PLAYER_MOVE_STRAFE_RIGHT) {
		if(player.Interface & INTER_COMBATMODE)
			request0_anim = alist[ANIM_FIGHT_STRAFE_RIGHT];
		else if(player.m_currentMovement & PLAYER_MOVE_STEALTH)
			request0_anim = alist[ANIM_STRAFE_RIGHT];
		else
			request0_anim = alist[ANIM_STRAFE_RUN_RIGHT];
	}
	
	if(tmove & PLAYER_MOVE_WALK_BACKWARD) {
		if(player.Interface & INTER_COMBATMODE)
			request0_anim = alist[ANIM_FIGHT_WALK_BACKWARD];
		else if(player.m_currentMovement & PLAYER_MOVE_STEALTH)
			request0_anim = alist[ANIM_WALK_BACKWARD];
		else if(player.m_currentMovement & PLAYER_CROUCH)
			request0_anim = alist[ANIM_WALK_BACKWARD];
		else
			request0_anim = alist[ANIM_RUN_BACKWARD];
	}
	
	if(tmove & PLAYER_MOVE_WALK_FORWARD) {
		if(player.Interface & INTER_COMBATMODE)
			request0_anim = alist[ANIM_FIGHT_WALK_FORWARD];
		else if(player.m_currentMovement & PLAYER_MOVE_STEALTH)
			request0_anim = alist[ANIM_WALK];
		else
			request0_anim = alist[ANIM_RUN];
	}
	}
	
	if(!request0_anim) {
		if(EXTERNALVIEW)
			request0_anim = alist[ANIM_WAIT];
		else
			request0_anim = alist[ANIM_WAIT_SHORT];
		
		request0_loop = true;
	}
	
	// Finally update anim
	if(layer1.cur_anim == nullptr
	   && (layer0.cur_anim == alist[ANIM_WAIT] || layer0.cur_anim == alist[ANIM_WAIT_SHORT])
	   && !(player.m_currentMovement & PLAYER_CROUCH)
	) {
		if(!(player.m_currentMovement & PLAYER_LEAN_LEFT) || !(player.m_currentMovement & PLAYER_LEAN_RIGHT)) {
			if(player.m_currentMovement & PLAYER_LEAN_LEFT) {
				request3_anim = alist[ANIM_LEAN_LEFT];
			}
			if(player.m_currentMovement & PLAYER_LEAN_RIGHT) {
				request3_anim = alist[ANIM_LEAN_RIGHT];
			}
		}
	}
	
	if(request3_anim == nullptr
	   && layer3.cur_anim
	   && (layer3.cur_anim == alist[ANIM_LEAN_RIGHT] || layer3.cur_anim == alist[ANIM_LEAN_LEFT])
	) {
		AcquireLastAnim(io);
		layer3.cur_anim = nullptr;
	}
	
	if((player.m_currentMovement & PLAYER_CROUCH) && !(player.m_lastMovement & PLAYER_CROUCH)
	   && !player.levitate) {
		request0_anim = alist[ANIM_CROUCH_START];
		request0_loop = false;
	} else if(!(player.m_currentMovement & PLAYER_CROUCH) && (player.m_lastMovement & PLAYER_CROUCH)) {
		request0_anim = alist[ANIM_CROUCH_END];
		request0_loop = false;
	} else if(player.m_currentMovement & PLAYER_CROUCH) {
		if(layer0.cur_anim == alist[ANIM_CROUCH_START]) {
			if(!(layer0.flags & EA_ANIMEND)) {
				request0_anim = alist[ANIM_CROUCH_START];
				request0_loop = false;
			} else {
				request0_anim = alist[ANIM_CROUCH_WAIT];
				request0_loop = true;
				player.physics.cyl.height = player.crouchHeight();
			}
		} else {
			if(   request0_anim == alist[ANIM_STRAFE_LEFT]
			   || request0_anim == alist[ANIM_STRAFE_RUN_LEFT]
			   || request0_anim == alist[ANIM_FIGHT_STRAFE_LEFT]
			) {
				request0_anim = alist[ANIM_CROUCH_STRAFE_LEFT];
				request0_loop = true;
			} else if(   request0_anim == alist[ANIM_STRAFE_RIGHT]
			          || request0_anim == alist[ANIM_STRAFE_RUN_RIGHT]
			          || request0_anim == alist[ANIM_FIGHT_STRAFE_RIGHT]
			) {
				request0_anim = alist[ANIM_CROUCH_STRAFE_RIGHT];
				request0_loop = true;
			} else if(   request0_anim == alist[ANIM_WALK]
			          || request0_anim == alist[ANIM_RUN]
			          || request0_anim == alist[ANIM_FIGHT_WALK_FORWARD]
			) {
				request0_anim = alist[ANIM_CROUCH_WALK];
				request0_loop = true;
			} else if(   request0_anim == alist[ANIM_WALK_BACKWARD]
			          || request0_anim == alist[ANIM_FIGHT_WALK_BACKWARD]
			) {
				request0_anim = alist[ANIM_CROUCH_WALK_BACKWARD];
				request0_loop = true;
			} else {
				request0_anim = alist[ANIM_CROUCH_WAIT];
				request0_loop = true;
			}
		}
	}
	
	if(layer0.cur_anim == alist[ANIM_CROUCH_END] && !(layer0.flags & EA_ANIMEND)) {
		player.m_lastMovement = player.m_currentMovement;
		return;
	}
	
	if(spells.getSpellByCaster(EntityHandle_Player, SPELL_FLYING_EYE)) {
		ARX_PLAYER_Manage_Visual_End(alist[ANIM_MEDITATION], request3_anim, true, false);
		return;
	}
	
	if(spells.getSpellOnTarget(io->index(), SPELL_LEVITATE)) {
		ARX_PLAYER_Manage_Visual_End(alist[ANIM_LEVITATE], request3_anim, true, false);
		return;
	}
	
	if(player.jumpphase != NotJumping) {
		switch(player.jumpphase) {
			case NotJumping:
			break;
			case JumpStart: { // Anticipation
				FALLING_TIME = 0;
				player.jumpphase = JumpAscending;
				request0_anim = alist[ANIM_JUMP_UP];
				player.jumpstarttime = g_platformTime.frameStart();
				player.jumplastposition = -1.f;
				request0_loop = false;
				break;
			}
			case JumpAscending: { // Moving up
				request0_anim = alist[ANIM_JUMP_UP];
				if(player.jumplastposition >= 1.f) {
					player.jumpphase = JumpDescending;
					request0_anim = alist[ANIM_JUMP_CYCLE];
					ARX_PLAYER_StartFall();
				}
				request0_loop = false;
				break;
			}
			case JumpDescending: { // Post-synch
				LAST_JUMP_ENDTIME = g_platformTime.frameStart();
				if((layer0.cur_anim == alist[ANIM_JUMP_END] && (layer0.flags & EA_ANIMEND))
				   || player.onfirmground) {
					player.jumpphase = JumpEnd;
					request0_anim = alist[ANIM_JUMP_END_PART2];
				} else {
					request0_anim = alist[ANIM_JUMP_END];
				}
				request0_loop = false;
				break;
			}
			case JumpEnd: { // Post-synch
				LAST_JUMP_ENDTIME = g_platformTime.frameStart();
				if(layer0.cur_anim == alist[ANIM_JUMP_END_PART2] && (layer0.flags & EA_ANIMEND)) {
					AcquireLastAnim(io);
					player.jumpphase = NotJumping;
				} else if(layer0.cur_anim == alist[ANIM_JUMP_END_PART2]
				          && glm::abs(player.physics.velocity.x) + glm::abs(player.physics.velocity.z)
				             > (4.f / TARGET_DT)
				          && layer0.ctime > 1ms) {
					AcquireLastAnim(io);
					player.jumpphase = NotJumping;
				} else {
					request0_anim = alist[ANIM_JUMP_END_PART2];
					request0_loop = false;
				}
				break;
			}
		}
		
	}
	
	ARX_PLAYER_Manage_Visual_End(request0_anim, request3_anim, request0_loop, false);

}

/*!
 * \brief Init Local Player Data
 */
void ARX_PLAYER_InitPlayer() {
	player.Interface = INTER_MINIBOOK | INTER_MINIBACK | INTER_LIFE_MANA;
	player.physics.cyl.height = player.baseHeight();
	player.physics.cyl.radius = player.baseRadius();
	player.lifePool.current = player.m_lifeMaxWithoutMods = player.lifePool.max = 100.f;
	player.manaPool.current = player.m_manaMaxWithoutMods = player.manaPool.max = 100.f;
	player.falling = false;
	if(Entity * torch = player.torch) {
		player.torch = nullptr;
		torch->updateOwner();
	}
	player.gold = 0;
	if(entities.player()) {
		entities.player()->inventory->setBags(1);
	}
	player.doingmagic = 0;
	
	ARX_PLAYER_MakeFreshHero();
}

/*!
 * \brief Forces player orientation to look at an IO
 */
void ForcePlayerLookAtIO(Entity * io) {
	
	arx_assert(io);
	
	VertexId id = entities.player()->obj->fastaccess.view_attach;
	Vec3f pos = id ? entities.player()->obj->vertexWorldPositions[id].v : player.pos;
	
	VertexId targetId = io->obj->fastaccess.view_attach;
	Vec3f target = targetId ? io->obj->vertexWorldPositions[targetId].v : io->pos;
	
	// For the case of not already computed Vlist3... !
	if(fartherThan(target, io->pos, 400.f)) {
		target = io->pos;
	}
	
	player.desiredangle = player.angle = Camera::getLookAtAngle(pos, target);
}

/*!
 * \brief Updates Many player infos each frame
 */
void ARX_PLAYER_Frame_Update()
{
	ARX_PROFILE_FUNC();
	
	if(spells.getSpellOnTarget(EntityHandle_Player, SPELL_PARALYSE)) {
		player.m_paralysed = true;
	} else {
		entities.player()->ioflags &= ~IO_FREEZESCRIPT;
		player.m_paralysed = false;
	}

	// Reset player moveto info
	g_moveto = player.pos;

	// Reset current movement flags
	player.m_currentMovement = 0;

	// Updates player angles to desired angles
	player.angle = player.desiredangle;

	// Updates player Extra-Rotate Informations
	Entity * io = entities.player();

	if(io && io->_npcdata->ex_rotate) {
		EERIE_EXTRA_ROTATE * extraRotation = io->_npcdata->ex_rotate;
		
		float v = player.angle.getPitch();
		if(v > 160) {
			v = -(360 - v);
		}
		
		if(player.Interface & INTER_COMBATMODE) {
			if(ARX_EQUIPMENT_GetPlayerWeaponType() == WEAPON_BOW) {
				extraRotation->group_rotate[0] = Anglef(); // Head
				extraRotation->group_rotate[1] = Anglef(); // Neck
				extraRotation->group_rotate[2] = Anglef(); // Chest
				extraRotation->group_rotate[3] = Anglef(v, 0.f, 0.f); // Belt
				// Apply fine aim with the shoulders so that we don't affect the view position
				extraRotation->group_rotate[4] = player.m_bowAimRotation; // Left shoulder
				extraRotation->group_rotate[5] = player.m_bowAimRotation; // Right shoulder
				
			} else {
				extraRotation->group_rotate[0] = Anglef(v * 0.1f, 0.f, 0.f); // Head
				extraRotation->group_rotate[1] = Anglef(v * 0.1f, 0.f, 0.f); // Neck
				extraRotation->group_rotate[2] = Anglef(v * 0.4f, 0.f, 0.f); // Chest
				extraRotation->group_rotate[3] = Anglef(v * 0.4f, 0.f, 0.f); // Belt
				extraRotation->group_rotate[4] = Anglef(); // Left shoulder
				extraRotation->group_rotate[5] = Anglef(); // Right
			}
		} else {
			extraRotation->group_rotate[0] = Anglef(v * 0.25f, 0.f, 0.f); // Head
			extraRotation->group_rotate[1] = Anglef(v * 0.25f, 0.f, 0.f); // Neck
			extraRotation->group_rotate[2] = Anglef(v * 0.25f, 0.f, 0.f); // Chest
			extraRotation->group_rotate[3] = Anglef(v * 0.25f, 0.f, 0.f); // Belt
			extraRotation->group_rotate[4] = Anglef(); // Left shoulder
			extraRotation->group_rotate[5] = Anglef(); // Right shoulder
		}
	}

	ARX_PLAYER_ComputePlayerFullStats();

	player.TRAP_DETECT = player.m_skillFull.mecanism;
	player.TRAP_SECRET = player.m_skillFull.intuition;

	if(spells.getSpellOnTarget(EntityHandle_Player, SPELL_DETECT_TRAP))
		player.TRAP_DETECT = 100.f;

	ARX_PLAYER_ManageTorch();
}

/*!
 * \brief Emit player step noise
 */
static void ARX_PLAYER_MakeStepNoise() {
	
	if(spells.getSpellOnTarget(EntityHandle_Player, SPELL_LEVITATE)) {
		return;
	}
	
	if(USE_PLAYERCOLLISIONS) {
		float volume = ARX_NPC_AUDIBLE_VOLUME_DEFAULT;
		float factor = ARX_NPC_AUDIBLE_FACTOR_DEFAULT;
		
		if(player.m_currentMovement & PLAYER_MOVE_STEALTH) {
			float skill_stealth = player.m_skillFull.stealth / ARX_PLAYER_SKILL_STEALTH_MAX;
			volume -= ARX_NPC_AUDIBLE_VOLUME_RANGE * skill_stealth;
			factor += ARX_NPC_AUDIBLE_FACTOR_RANGE * skill_stealth;
		}
		
		Vec3f pos = player.basePosition();
		ARX_NPC_NeedStepSound(entities.player(), pos, volume, factor);
	}
	
	while(currentdistance >= STEP_DISTANCE) {
		currentdistance -= STEP_DISTANCE;
	}
}

extern bool bGCroucheToggle;

static long LAST_FIRM_GROUND = 1;
static long TRUE_FIRM_GROUND = 1;
float lastposy = -9999999.f;
PlatformInstant REQUEST_JUMP = 0;

PlatformInstant LAST_JUMP_ENDTIME = 0;

static bool Valid_Jump_Pos() {
	
	if(LAST_ON_PLATFORM || player.climbing) {
		return true;
	}
	
	Cylinder tmpp = Cylinder(player.basePosition(), player.physics.cyl.radius * 0.85f, player.physics.cyl.height);
	
	float tmp = CheckAnythingInCylinder(tmpp, entities.player(),
	                                    CFLAG_PLAYER | CFLAG_JUST_TEST);
	if(tmp <= 20.f) {
		return true;
	}
	
	long hum = 0;
	for(size_t vv = 0; vv < 360; vv += 20) {
		tmpp.origin = player.basePosition();
		tmpp.origin += angleToVectorXZ(float(vv)) * 20.f;
		
		tmpp.radius = player.physics.cyl.radius;
		float anything = CheckAnythingInCylinder(tmpp, entities.player(), CFLAG_JUST_TEST);
		if(anything > 10) {
			hum = 1;
			break;
		}
	}
	if(!hum) {
		return true;
	}
	
	if(COLLIDED_CLIMB_POLY) {
		player.climbing = true;
		return true;
	}
	
	return (tmp <= 50.f);
}

static void setPlayerPositionColor() {
	
	float grnd_color = GetColorz(Vec3f(player.pos.x, player.pos.y + 90, player.pos.z)) - 15.f;
	if(CURRENT_PLAYER_COLOR < grnd_color) {
		CURRENT_PLAYER_COLOR += g_framedelay * (1.0f / 8);
		CURRENT_PLAYER_COLOR = std::min(CURRENT_PLAYER_COLOR, grnd_color);
	}
	if(CURRENT_PLAYER_COLOR > grnd_color) {
		CURRENT_PLAYER_COLOR -= g_framedelay * (1.0f / 4);
		CURRENT_PLAYER_COLOR = std::max(CURRENT_PLAYER_COLOR, grnd_color);
	}
	
}

static void PlayerMovementIterate(float DeltaTime) {
	
	float d = 0;
	
	if(USE_PLAYERCOLLISIONS) {
		// A jump is requested so let's go !
		if(REQUEST_JUMP != 0) {
			if((player.m_currentMovement & PLAYER_CROUCH)
			   || player.physics.cyl.height > player.baseHeight()) {
				float old = player.physics.cyl.height;
				player.physics.cyl.height = player.baseHeight();
				player.physics.cyl.origin = player.basePosition();
				float anything = CheckAnythingInCylinder(player.physics.cyl, entities.player(), CFLAG_JUST_TEST);
				if(anything < 0.f) {
					player.m_currentMovement |= PLAYER_CROUCH;
					player.physics.cyl.height = old;
					REQUEST_JUMP = 0;
				} else {
					bGCroucheToggle = false;
					player.m_currentMovement &= ~PLAYER_CROUCH;
					player.physics.cyl.height = player.baseHeight();
				}
			}
			
			if(!Valid_Jump_Pos()) {
				REQUEST_JUMP = 0;
			}
			
			if(REQUEST_JUMP != 0) {
				PlatformDuration t = g_platformTime.frameStart() - REQUEST_JUMP;
				if(t >= 0 && t <= 350ms) {
					REQUEST_JUMP = 0;
					spawnAudibleSound(player.pos, *entities.player());
					ARX_SPEECH_AddSpeech(*entities.player(), "player_jump", ANIM_TALK_NEUTRAL, ARX_SPEECH_FLAG_NOTEXT);
					player.onfirmground = false;
					player.jumpphase = JumpStart;
				}
			}
		}
		
		if(entities.player()->_npcdata->climb_count != 0.f && g_framedelay > 0) {
			entities.player()->_npcdata->climb_count -= MAX_ALLOWED_CLIMBS_PER_SECOND * g_framedelay * 0.1f;
			if(entities.player()->_npcdata->climb_count < 0) {
				entities.player()->_npcdata->climb_count = 0.f;
			}
		}
		
		
		CollisionFlags levitate = 0;
		if(player.climbing) {
			levitate = CFLAG_LEVITATE;
		}
		
		if(player.levitate) {
			if(player.physics.cyl.height != player.levitateHeight()) {
				float old = player.physics.cyl.height;
				player.physics.cyl.height = player.levitateHeight();
				player.physics.cyl.origin = player.basePosition();
				float anything = CheckAnythingInCylinder(player.physics.cyl, entities.player());
				if(anything < 0.f) {
					player.physics.cyl.height = old;
					
					spells.endByTarget(EntityHandle_Player, SPELL_LEVITATE);
				}
			}
			
			if(player.physics.cyl.height == player.levitateHeight()) {
				levitate = CFLAG_LEVITATE;
				player.climbing = false;
				bGCroucheToggle = false;
				player.m_currentMovement &= ~PLAYER_CROUCH;
			}
			
		} else if(player.physics.cyl.height == player.levitateHeight()) {
			player.physics.cyl.height = player.baseHeight();
		}
		
		if(player.jumpphase != JumpAscending && !levitate) {
			player.physics.cyl.origin = player.basePosition();
		}
		
		if(glm::abs(lastposy - player.pos.y) < DeltaTime * 0.1f) {
			TRUE_FIRM_GROUND = 1;
		} else {
			TRUE_FIRM_GROUND = 0;
		}
		
		lastposy = player.pos.y;
		float anything;
		Cylinder testcyl = player.physics.cyl;
		testcyl.origin.y += 3.f;
		ON_PLATFORM = 0;
		anything = CheckAnythingInCylinder(testcyl, entities.player(), 0);
		LAST_ON_PLATFORM = ON_PLATFORM;
	
		if(player.jumpphase != JumpAscending) {
			if(anything >= 0.f) {
				TRUE_FIRM_GROUND = 0;
			} else {
				TRUE_FIRM_GROUND = 1;
				testcyl.radius -= 30.f;
				testcyl.origin.y -= 10.f;
				anything = CheckAnythingInCylinder(testcyl, entities.player(), 0);
			}
		} else {
			TRUE_FIRM_GROUND = 0;
			LAST_ON_PLATFORM = 0;
		}
		
		Cylinder cyl = Cylinder(player.basePosition() + Vec3f(0.f, 1.f, 0.f), player.physics.cyl.radius, player.physics.cyl.height);
		
		float anything2 = CheckAnythingInCylinder(cyl, entities.player(), CFLAG_JUST_TEST | CFLAG_PLAYER);
		
		if(anything2 > -5 && player.physics.velocity.y > (15.f / TARGET_DT) && !LAST_ON_PLATFORM
		   && !TRUE_FIRM_GROUND && player.jumpphase == NotJumping && !player.levitate && anything > 80.f) {
			player.jumpphase = JumpDescending;
			if(!player.falling) {
				player.falling = true;
				ARX_PLAYER_StartFall();
			}
		} else if(!player.falling) {
			FALLING_TIME = 0;
		}
		
		if(player.jumpphase != NotJumping && player.levitate) {
			player.jumpphase = NotJumping;
			player.falling = false;
			Falling_Height = player.pos.y;
			FALLING_TIME = 0;
		}
		
		if(!LAST_FIRM_GROUND && TRUE_FIRM_GROUND) {
			player.jumpphase = NotJumping;
			if(FALLING_TIME > 0 && player.falling) {
				player.physics.velocity.x = 0.f;
				player.physics.velocity.z = 0.f;
				player.physics.forces.x = 0.f;
				player.physics.forces.z = 0.f;
				player.falling = false;
				float fh = player.pos.y - Falling_Height;
				if(fh > 400.f) {
					float dmg = (fh - 400.f) * (1.0f / 15);
					if(dmg > 0.f) {
						Falling_Height = player.pos.y;
						FALLING_TIME = 0;
						damagePlayer(dmg, 0);
						ARX_DAMAGES_DamagePlayerEquipment(dmg);
					}
				}
			}
		}
		
		LAST_FIRM_GROUND = TRUE_FIRM_GROUND;
		player.onfirmground = (TRUE_FIRM_GROUND != 0);
		if(player.onfirmground && !player.falling) {
			FALLING_TIME = 0;
		}
		
		// Apply player impulse force
		
		float jump_mul = 1.f;
		PlatformDuration diff = g_platformTime.frameStart() - LAST_JUMP_ENDTIME;
		if(diff < 600ms) {
			jump_mul = 0.5f;
			if(diff >= 300ms) {
				jump_mul += (toMsf(LAST_JUMP_ENDTIME - g_platformTime.frameStart()) + 300.f) * (1.f / 300);
				if(jump_mul > 1.f) {
					jump_mul = 1.f;
				}
			}
		}
		
		Vec3f impulse = g_moveto - player.pos;
		if(impulse != Vec3f(0.f)) {
			
			const AnimLayer & layer0 = entities.player()->animlayer[0];
			float scale = 1.25f / 1000;
			if(layer0.cur_anim) {
				if(player.jumpphase != NotJumping) {
					if(player.m_currentMovement & PLAYER_MOVE_WALK_BACKWARD) {
						scale = 0.8f / 1000;
					} else if(player.m_currentMovement & PLAYER_MOVE_WALK_FORWARD) {
						scale = 7.9f / 1000;
					} else if(player.m_currentMovement & PLAYER_MOVE_STRAFE_LEFT) {
						scale = 2.6f / 1000;
					} else if(player.m_currentMovement & PLAYER_MOVE_STRAFE_RIGHT) {
						scale = 2.6f / 1000;
					} else {
						scale = 0.2f / 1000;
					}
				} else if(levitate && !player.climbing) {
					scale = 0.875f / 1000;
				} else {
					Vec3f mv = GetAnimTotalTranslate(layer0.cur_anim, layer0.altidx_cur);
					AnimationDuration time = layer0.cur_anim->anims[layer0.altidx_cur]->anim_time;
					scale = glm::length(mv) / toMsf(time) * 0.0125f;
				}
			}
			
			impulse *= scale / glm::length(impulse) * jump_mul;
		}
		
		if(player.jumpphase != NotJumping) {
			// No Vertical Interpolation
			entities.player()->_npcdata->vvpos = -99999.f;
			if(player.jumpphase == JumpAscending) {
				g_moveto.y = player.pos.y;
				player.physics.velocity.y = 0;
			}
		}
		
		if(player.climbing) {
			player.physics.velocity.x = 0.f;
			player.physics.velocity.y *= 0.5f;
			player.physics.velocity.z = 0.f;
			if(player.m_currentMovement & PLAYER_MOVE_WALK_FORWARD) {
				g_moveto.x = player.pos.x;
				g_moveto.z = player.pos.z;
			}
			if(player.m_currentMovement & PLAYER_MOVE_WALK_BACKWARD) {
				impulse.x = 0;
				impulse.z = 0;
				g_moveto.x = player.pos.x;
				g_moveto.z = player.pos.z;
			}
		}
		
		player.physics.forces += impulse;
		
		// Apply Gravity force if not LEVITATING or JUMPING
		if(!levitate && player.jumpphase != JumpAscending && !LAST_ON_PLATFORM) {
			player.physics.forces.y += (player.falling ? JUMP_GRAVITY : WORLD_GRAVITY) / TARGET_DT;
			// Check for LAVA Damage !!!
			float epcentery;
			EERIEPOLY * ep = CheckInPoly(player.pos + Vec3f(0.f, 150.f, 0.f), &epcentery);
			if(ep) {
				if((ep->type & POLY_LAVA) && glm::abs(epcentery - (player.pos.y - player.baseHeight())) < 30) {
					float mul = 1.f - (glm::abs(epcentery - (player.pos.y - player.baseHeight())) * (1.0f / 30));
					const float LAVA_DAMAGE = 10.f;
					float damages = LAVA_DAMAGE * g_framedelay * 0.01f * mul;
					damages = ARX_SPELLS_ApplyFireProtection(entities.player(), damages);
					damagePlayer(damages, DAMAGE_TYPE_FIRE, entities.player());
					ARX_DAMAGES_DamagePlayerEquipment(damages);
					Vec3f pos = player.basePosition();
					ARX_PARTICLES_Spawn_Lava_Burn(pos, entities.player());
				}
			}
		}
		
		// Apply velocity damping (natural velocity attenuation, stands for friction)
		float dampen = 1.f - (0.009f * DeltaTime);
		if(dampen < 0.001f) {
			dampen = 0.f;
		}
		player.physics.velocity.x *= dampen;
		player.physics.velocity.z *= dampen;
		if(glm::abs(player.physics.velocity.x) < 0.001f) {
			player.physics.velocity.x = 0;
		}
		if(glm::abs(player.physics.velocity.z) < 0.001f) {
			player.physics.velocity.z = 0;
		}
		
		// Apply attraction
		player.physics.forces += ARX_SPECIAL_ATTRACTORS_ComputeForIO(*entities.player()) / TARGET_DT;
		
		// Apply push player force
		player.physics.forces += PUSH_PLAYER_FORCE / TARGET_DT;
		PUSH_PLAYER_FORCE = Vec3f(0.f);
		
		// Apply forces to velocity
		player.physics.velocity += player.physics.forces * DeltaTime;
		
		if(player.levitate) {
			player.physics.velocity.y = 0.0f;
		}

		// Apply climbing velocity
		if(player.climbing) {
			if(player.m_currentMovement & PLAYER_MOVE_WALK_FORWARD) {
				player.physics.velocity.y = -0.2f;
			}
			if(player.m_currentMovement & PLAYER_MOVE_WALK_BACKWARD) {
				player.physics.velocity.y = 0.2f;
			}
		}
		
		// Removes y velocity if on firm ground...
		if(player.onfirmground && !player.climbing) {
			player.physics.velocity.y = 0.f;
		}
		
		float posy;
		EERIEPOLY * ep = CheckInPoly(player.pos, &posy);
		if(ep == nullptr) {
			player.physics.velocity.y = 0;
		} else if(!player.climbing && player.pos.y >= posy) {
			player.physics.velocity.y = 0;
		}

		// Reset forces
		player.physics.forces = Vec3f(0.f);
		
		// Check if player is already on firm ground AND not moving
		if(glm::abs(player.physics.velocity.x) < 0.001f
		   && glm::abs(player.physics.velocity.z) < 0.001f
		   && player.onfirmground
		   && player.jumpphase == NotJumping
		) {
			g_moveto = player.pos;
			setPlayerPositionColor();
			return;
		}
		
		// Need to apply some physics/collision tests
		player.physics.cyl.origin = player.basePosition();
		player.physics.startpos = player.physics.cyl.origin;
		player.physics.targetpos = player.physics.startpos + player.physics.velocity * DeltaTime;
		
		// Jump impulse
		if(player.jumpphase == JumpAscending) {
			
			if(player.jumplastposition == -1.f) {
				player.jumplastposition = 0;
				player.jumpstarttime = g_platformTime.frameStart();
			}
			
			const float jump_up_time = 200.f;
			const float jump_up_height = 130.f;
			const PlatformInstant now = g_platformTime.frameStart();
			const float elapsed = toMsf(now - player.jumpstarttime);
			float position = glm::clamp(elapsed / jump_up_time, 0.f, 1.f);
			
			float p = (position - player.jumplastposition) * jump_up_height;
			player.physics.targetpos.y -= p;
			player.jumplastposition = position;
			levitate = 0;
		}
		
		bool test;
		float PLAYER_CYLINDER_STEP = 40.f;
		if(player.climbing) {
			test = ARX_COLLISION_Move_Cylinder(&player.physics, entities.player(), PLAYER_CYLINDER_STEP,
			                                   CFLAG_EASY_SLIDING | CFLAG_CLIMBING | CFLAG_PLAYER);
			if(!COLLIDED_CLIMB_POLY) {
				player.climbing = false;
			}
		} else {
			test = ARX_COLLISION_Move_Cylinder(&player.physics, entities.player(), PLAYER_CYLINDER_STEP,
			                                   levitate | CFLAG_EASY_SLIDING | CFLAG_PLAYER);
			
			if(!test && !LAST_FIRM_GROUND && !TRUE_FIRM_GROUND) {
				player.physics.velocity.x = 0.f;
				player.physics.velocity.z = 0.f;
				if(FALLING_TIME > 0 && player.falling) {
					float fh = player.pos.y - Falling_Height;
					if(fh > 400.f) {
						float dmg = (fh - 400.f) * (1.f / 15);
						if(dmg > 0.f) {
							Falling_Height = (player.pos.y + Falling_Height * 2) * (1.f / 3);
							damagePlayer(dmg, 0);
							ARX_DAMAGES_DamagePlayerEquipment(dmg);
						}
					}
				}
			}
			
			if(!test && player.jumpphase != NotJumping) {
				player.physics.startpos.x = player.physics.cyl.origin.x = player.pos.x;
				player.physics.startpos.z = player.physics.cyl.origin.z = player.pos.z;
				player.physics.targetpos.x = player.physics.startpos.x;
				player.physics.targetpos.z = player.physics.startpos.z;
				if(player.physics.targetpos.y != player.physics.startpos.y) {
					test = ARX_COLLISION_Move_Cylinder(&player.physics, entities.player(), PLAYER_CYLINDER_STEP,
					                                   levitate | CFLAG_EASY_SLIDING | CFLAG_PLAYER);
					entities.player()->_npcdata->vvpos = -99999.f;
				}
			}
		}
		
		if(COLLIDED_CLIMB_POLY) {
			player.climbing = true;
		}
		
		if(player.climbing) {
			if(player.m_currentMovement
			   && player.m_currentMovement != PLAYER_ROTATE
			   && !(player.m_currentMovement & PLAYER_MOVE_WALK_FORWARD)
			   && !(player.m_currentMovement & PLAYER_MOVE_WALK_BACKWARD)
			) {
				player.climbing = false;
			}
			
			if((player.m_currentMovement & PLAYER_MOVE_WALK_BACKWARD) && !test) {
				player.climbing = false;
			}
			
			if(player.climbing) {
				player.jumpphase = NotJumping;
				player.falling = false;
				FALLING_TIME = 0;
				Falling_Height = player.pos.y;
			}
		}
		
		if(player.jumpphase == JumpAscending) {
			player.climbing = false;
		}
		
		g_moveto = player.physics.cyl.origin + player.baseOffset();
		d = glm::distance(player.pos, g_moveto);
	} else {
		Vec3f vect = g_moveto - player.pos;
		float divv = glm::length(vect);
		if(divv > 0.f) {
			float mul = toMsf(g_platformTime.lastFrameDuration()) * 0.001f * 200.f;
			divv = mul / divv;
			vect *= divv;
			g_moveto = player.pos + vect;
		}
		
		player.onfirmground = false;
	}
	
	if(player.pos == g_moveto) {
		d = 0.f;
	}
	
	// Emit Stepsound
	if(USE_PLAYERCOLLISIONS) {
		if(player.m_currentMovement & PLAYER_CROUCH) {
			d *= 2.f;
		}
		currentdistance += d;
		if(player.jumpphase == NotJumping && !player.falling
		   && currentdistance >= STEP_DISTANCE) {
			ARX_PLAYER_MakeStepNoise();
		}
	}
	
	// Finally update player pos !
	player.pos = g_moveto;
	
	setPlayerPositionColor();
}

void ARX_PLAYER_Manage_Movement() {
	
	ARX_PROFILE_FUNC();
	
	// Is our player able to move ?
	if(cinematicBorder.isActive() || BLOCK_PLAYER_CONTROLS || !entities.player())
		return;

	// Compute current player speedfactor
	float speedfactor = entities.player()->basespeed + entities.player()->speed_modif;

	if(speedfactor < 0)
		speedfactor = 0;

	// Compute time things
	const float FIXED_TIMESTEP = 25.f;
	const float MAX_FRAME_TIME = 200.f;

	static float StoredTime = 0;

	float DeltaTime = std::min(toMsf(g_platformTime.lastFrameDuration()), MAX_FRAME_TIME);
	DeltaTime = StoredTime + DeltaTime * speedfactor;
	
	if(player.jumpphase != NotJumping) {
		while(DeltaTime > FIXED_TIMESTEP) {
			/*
			 * TODO: should be PlayerMovementIterate(FIXED_TIMESTEP);
			 * However, jump forward movement is only applied the the first
			 * iteration, so we need this to not completely break the jump
			 * at lower framerates.
			 * Should only cause minor differences at higher framerates.
			 * Fix this once PlayerMovementIterate has been cleaned up!
			 */
			PlayerMovementIterate(DeltaTime);
			DeltaTime -= FIXED_TIMESTEP;
		}
	} else {
		PlayerMovementIterate(DeltaTime);
		DeltaTime = 0;
	}
	
	StoredTime = DeltaTime;
}

/*!
 * \brief Manage Player Death Visual
 */
void ARX_PLAYER_Manage_Death() {
	if(player.DeadTime <= 2s)
		return;

	player.m_paralysed = false;
	float ratio = (player.DeadTime - 2s) / 5s;

	if(ratio >= 1.f) {
		ARX_MENU_Launch(false);
		player.DeadTime = 0;
	}
	
	UseRenderState state(render2D().blend(BlendZero, BlendInvSrcColor));
	EERIEDrawBitmap(Rectf(g_size), 0.000091f, nullptr, Color::gray(ratio));
}

/*!
 * \brief Specific for color checks
 */
float GetPlayerStealth() {
	return 15 + player.m_skillFull.stealth * ( 1.0f / 10 );
}

/*!
 * \brief Force Player to standard stance
 */
void ARX_PLAYER_PutPlayerInNormalStance() {
	
	if(player.m_currentMovement & PLAYER_CROUCH) {
		player.m_currentMovement &= ~PLAYER_CROUCH;
	}
	
	player.m_currentMovement = 0;
	ARX_PLAYER_RectifyPosition();
	
	if(player.jumpphase != NotJumping || player.falling) {
		player.physics.cyl.origin = player.basePosition();
		IO_PHYSICS phys = player.physics;
		AttemptValidCylinderPos(phys.cyl, entities.player(), CFLAG_RETURN_HEIGHT);
		player.pos.y = phys.cyl.origin.y + player.baseHeight();
		player.jumpphase = NotJumping;
		player.falling = false;
	}
	
	if(player.Interface & INTER_COMBATMODE) {
		player.Interface &= ~INTER_COMBATMODE;
		ARX_EQUIPMENT_LaunchPlayerUnReadyWeapon();
	}
	
	ARX_SOUND_Stop(player.magic_draw);
	player.magic_draw = audio::SourcedSample();
}

/*!
 * \brief Add gold to player purse
 */
void ARX_PLAYER_AddGold(long _lValue) {
	player.gold += _lValue;
	g_hudRoot.purseIconGui.requestHalo();
}

void ARX_PLAYER_AddGold(Entity * gold) {
	
	arx_assert(gold->ioflags & IO_GOLD);
	
	ARX_PLAYER_AddGold(gold->_itemdata->buyPrice * std::max(short(1), gold->_itemdata->count));
	
	ARX_SOUND_PlayInterface(g_snd.GOLD);
	
	gold->gameFlags &= ~GFLAG_ISINTREATZONE;
	
	gold->destroy();
}

void ARX_PLAYER_Start_New_Quest() {
	
	LogInfo << "Starting a new playthrough";
	
	DanaeClearLevel();
	SetEditMode();
	
	g_characterCreation.resetCheat();
	arx_assert(!player.torch);
	svar.clear();
	
	ARX_CHANGELEVEL_StartNew();
	
	entities.player()->halo.flags = 0;
}

void ARX_PLAYER_AddBag() {
	entities.player()->inventory->setBags(std::min(entities.player()->inventory->bags() + 1, size_t(10)));
}

bool ARX_PLAYER_CanStealItem(Entity * item) {
	return (item->_itemdata->stealvalue > 0 && player.m_skillFull.stealth >= item->_itemdata->stealvalue
	        && item->_itemdata->stealvalue < 100.f);
}

void ARX_PLAYER_Rune_Add_All() {

	ARX_Player_Rune_Add(FLAG_AAM);
	ARX_Player_Rune_Add(FLAG_CETRIUS);
	ARX_Player_Rune_Add(FLAG_COMUNICATUM);
	ARX_Player_Rune_Add(FLAG_COSUM);
	ARX_Player_Rune_Add(FLAG_FOLGORA);
	ARX_Player_Rune_Add(FLAG_FRIDD);
	ARX_Player_Rune_Add(FLAG_KAOM);
	ARX_Player_Rune_Add(FLAG_MEGA);
	ARX_Player_Rune_Add(FLAG_MORTE);
	ARX_Player_Rune_Add(FLAG_MOVIS);
	ARX_Player_Rune_Add(FLAG_NHI);
	ARX_Player_Rune_Add(FLAG_RHAA);
	ARX_Player_Rune_Add(FLAG_SPACIUM);
	ARX_Player_Rune_Add(FLAG_STREGUM);
	ARX_Player_Rune_Add(FLAG_TAAR);
	ARX_Player_Rune_Add(FLAG_TEMPUS);
	ARX_Player_Rune_Add(FLAG_TERA);
	ARX_Player_Rune_Add(FLAG_VISTA);
	ARX_Player_Rune_Add(FLAG_VITAE);
	ARX_Player_Rune_Add(FLAG_YOK);
}

void ARX_PLAYER_Invulnerability(long flag) {

	if(flag)
		player.playerflags |= PLAYERFLAGS_INVULNERABILITY;
	else
		player.playerflags &= ~PLAYERFLAGS_INVULNERABILITY;
}

void ARX_GAME_Reset() {
	arx_assert(entities.player());
	
	player.DeadTime = 0;
	
	LastValidPlayerPos = Vec3f(0.f);
	
	entities.player()->speed_modif = 0;
	
	LAST_JUMP_ENDTIME = 0;
	FlyingOverIO = nullptr;
	g_miniMap.mapMarkerInit();
	ClearDynLights();

	if(!DONT_ERASE_PLAYER) {
		entities.player()->halo.flags = 0;
	}

	entities.player()->gameFlags &= ~GFLAG_INVISIBILITY;
	
	ARX_PLAYER_Invulnerability(0);
	player.m_paralysed = false;

	ARX_PLAYER_Reset_Fall();

	player.levitate = false;
	player.m_telekinesis = false;
	player.onfirmground = false;
	TRUE_FIRM_GROUND = 0;

	lastposy = -99999999999.f;

	ioSteal = nullptr;

	g_gameTime.setSpeed(1.f);

	CheatReset();
	
	entities.player()->spellcast_data.castingspell = SPELL_NONE;
	
	ARX_INTERFACE_NoteClear();
	player.Interface = INTER_LIFE_MANA | INTER_MINIBACK | INTER_MINIBOOK;

	// Interactive DynData
	ARX_INTERACTIVE_ClearAllDynData();

	// PolyBooms
	PolyBoomClear();

	// Magical Flares
	ARX_MAGICAL_FLARES_KillAll();

	// Thrown Objects
	ARX_THROWN_OBJECT_KillAll();

	// Pathfinder
	EERIE_PATHFINDER_Clear();

	// Sound
	ARX_SOUND_MixerStop(ARX_SOUND_MixerGame);
	ARX_SOUND_MixerPause(ARX_SOUND_MixerGame);
	ARX_SOUND_MixerResume(ARX_SOUND_MixerGame);

	// Damages
	ARX_DAMAGE_Reset_Blood_Info();
	ARX_DAMAGES_Reset();

	// Scripts
	ARX_SCRIPT_Timer_ClearAll();
	ARX_SCRIPT_EventStackClear();
	ARX_SCRIPT_ResetAll(false);
	
	// Speech Things
	REQUEST_SPEECH_SKIP = false;
	notification_ClearAll();
	ARX_SPEECH_Reset();

	// Spells
	ARX_SPELLS_Precast_Reset();
	ARX_SPELLS_CancelSpellTarget();

	spells.clearAll();
	ARX_SPELLS_ClearAllSymbolDraw();
	ARX_SPELLS_ResetRecognition();

	// Particles
	ARX_PARTICLES_ClearAll();
	ParticleSparkClear();
	g_particleManager.Clear();

	// Fogs
	ARX_FOGS_Render();

	// Anchors
	ANCHOR_BLOCK_Clear();

	// Attractors
	ARX_SPECIAL_ATTRACTORS_Reset();

	// Cinematics
	cinematicKill();

	// Paths
	ARX_PATH_ClearAllControled();
	ARX_PATH_ClearAllUsePath();
	
	// Player Quests
	ARX_PLAYER_Quest_Init();

	// Player Keyring
	ARX_KEYRING_Init();

	// Player Init
	if(!DONT_ERASE_PLAYER) {
		g_miniMap.mapMarkerInit();
		GLOBAL_MAGIC_MODE = true;

		// Linked Objects
		UnlinkAllLinkedObjects();
		ARX_EQUIPMENT_UnEquipAllPlayer();

		ARX_EQUIPMENT_ReleaseAll(entities.player());

		CleanInventory();
		ARX_PLAYER_InitPlayer();
		ARX_INTERACTIVE_RemoveGoreOnIO(entities.player());
		
		// default to mouselook on, inventory closed
		TRUE_PLAYER_MOUSELOOK_ON = true;

		// Player Inventory
		CleanInventory();
		g_playerInventoryHud.setCurrentBag(0);
	}

	// Misc Player Vars.
	ROTATE_START = 0;
	BLOCK_PLAYER_CONTROLS = false;
	HERO_SHOW_1ST = -1;
	PUSH_PLAYER_FORCE = Vec3f(0.f);
	player.jumplastposition = 0;
	player.jumpstarttime = 0;
	player.jumpphase = NotJumping;
	entities.player()->inzone = nullptr;

	RemoveQuakeFX();
	player.m_improve = false;

	eyeball.reset();
	
	entities.player()->ouch_time = 0;
	entities.player()->invisibility = 0.f;
	
	fadeReset();
	
	// GLOBALMods
	ARX_GLOBALMODS_Reset();

	// Missiles
	ARX_MISSILES_ClearAll();
	
	culledStaticLightsReset();
	
	// Interface
	ARX_INTERFACE_Reset();
	ARX_INTERFACE_NoteClear();
	setDraggedEntity(nullptr);
	g_cameraEntity = nullptr;
	CHANGE_LEVEL_ICON = NoChangeLevel;
	
	ClearTileLights();
}

void ARX_PLAYER_Reset_Fall()
{
	FALLING_TIME = 0;
	Falling_Height = 50.f;
	player.falling = false;
}

