/*
 * Copyright 2015-2022 Arx Libertatis Team (see the AUTHORS file)
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

#ifndef ARX_GUI_HUD_SECONDARYINVENTORY_H
#define ARX_GUI_HUD_SECONDARYINVENTORY_H

#include "game/Inventory.h"
#include "gui/hud/HudCommon.h"
#include "math/Vector.h"

class Entity;
class TextureContainer;


class SecondaryInventoryPickAllHudIcon : public HudIconBase {
	
	Vec2f m_size;
	
public:
	
	SecondaryInventoryPickAllHudIcon() : m_size(0.f) { }
	
	void init();
	void update(const Rectf & parent);
	void updateInput();
	
};

class SecondaryInventoryCloseHudIcon : public HudIconBase {
	
	Vec2f m_size;
	
public:
	
	SecondaryInventoryCloseHudIcon() : m_size(0.f) { }
	
	void init();
	void update(const Rectf & parent);
	void updateInput();
	
};


class SecondaryInventoryHud : public HudItem {
	
	Vec2f m_size;
	TextureContainer * ingame_inventory;
	TextureContainer * m_canNotSteal;
	TextureContainer * m_defaultBackground;
	
	SecondaryInventoryPickAllHudIcon m_pickAllButton;
	SecondaryInventoryCloseHudIcon m_closeButton;
	
	Entity * m_container;
	bool m_open;
	
	Entity * getSecondaryOrStealInvEntity();
	
public:
	
	SecondaryInventoryHud()
		: m_size(0.f)
		, ingame_inventory(nullptr)
		, m_canNotSteal(nullptr)
		, m_defaultBackground(nullptr)
		, m_container(nullptr)
		, m_open(false)
		, m_fadeDirection(Fade_stable)
		, m_fadePosition(0.f)
	{ }
	
	void init();
	void update();
	void updateRect();
	void updateCombineFlags(Entity * source);
	void draw();
	void drawItemPrice(float scale);
	
	void updateInputButtons();
	
	/*!
	 * \brief Returns true if position is in secondary inventory
	 */
	[[nodiscard]] bool containsPos(Vec2s pos) const noexcept;
	[[nodiscard]] Entity * getObj(Vec2s pos) const noexcept;
	
	void dropEntity();
	void dragEntity(Entity * io);
	
	void open(Entity * container);
	void close();
	
	[[nodiscard]] bool isVisible() const noexcept;
	[[nodiscard]] bool isOpen() const noexcept;
	[[nodiscard]] bool isOpen(const Entity * container) const noexcept;
	
	void clear(const Entity * container);
	
	void updateFader();
	
	void takeAllItems();
	
	bool isSlotVisible(InventoryPos pos);
	
	enum Fade {
		Fade_left = -1,
		Fade_stable = 0,
		Fade_right = 1
	};
	
	Fade m_fadeDirection;
	float m_fadePosition;
	
	Entity * getEntity() { return m_container; }
};

extern SecondaryInventoryHud g_secondaryInventoryHud;

#endif // ARX_GUI_HUD_SECONDARYINVENTORY_H
