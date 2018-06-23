/*
 * Copyright 2015-2016 Arx Libertatis Team (see the AUTHORS file)
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

#include "gui/widget/TextInputWidget.h"

#include "core/Application.h"
#include "core/Core.h"
#include "core/GameTime.h"
#include "graphics/Draw.h"
#include "graphics/Renderer.h"
#include "graphics/font/Font.h"
#include "gui/Text.h"
#include "scene/GameSound.h"
#include "window/RenderWindow.h"

TextInputWidget::TextInputWidget(Font * font, const std::string & text, Vec2f pos)
	: m_font(font)
{
	m_rect = Rectf(RATIO_2(pos), 0.f, 0.f);
	setText(text);
	eState = EDIT;
}

void TextInputWidget::setText(const std::string & text) {
	m_text = text;
	Vec2i textSize = m_font->getTextSize(m_text);
	m_rect = Rectf(m_rect.topLeft(), textSize.x + 1, textSize.y + 1);
}

bool TextInputWidget::click() {
	
	bool result = Widget::click();
	
	if(!m_enabled) {
		return result;
	}
	
	ARX_SOUND_PlayMenu(SND_MENU_CLICK);
	
	if(eState == EDIT) {
		eState = EDIT_TIME;
		return true;
	}
	
	return result;
}

void TextInputWidget::render(bool mouseOver) {
	
	Color color = Color(232, 204, 142);
	if(!m_enabled) {
		color = Color::grayb(127);
	} else if(mouseOver) {
		color = Color::white;
	}
	
	ARX_UNICODE_DrawTextInRect(m_font, m_rect.topLeft(), m_rect.right, m_text, color, NULL);
	
	if(eState == EDIT_TIME) {
		bool blink = true;
		if(mainApp->getWindow()->hasFocus()) {
			blink = timeWaveSquare(g_platformTime.frameStart(), PlatformDurationMs(1200));
		}
		if(blink) {
			// Draw cursor
			TexturedVertex v[4];
			GRenderer->ResetTexture(0);
			v[0].color = v[1].color = v[2].color = v[3].color = Color::white.toRGB();
			v[0].p = Vec3f(m_rect.right, m_rect.top, 0.f);
			v[1].p = v[0].p + Vec3f(2.f, 0.f, 0.f);
			v[2].p = Vec3f(m_rect.right, m_rect.bottom, 0.f);
			v[3].p = v[2].p + Vec3f(2.f, 0.f, 0.f);
			v[0].w = v[1].w = v[2].w = v[3].w = 1.f;
			EERIEDRAWPRIM(Renderer::TriangleStrip, v, 4);
		}
	}
	
}