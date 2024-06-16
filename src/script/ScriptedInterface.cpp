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

#include "script/ScriptedInterface.h"

#include <boost/algorithm/string/predicate.hpp>
#include <regex>
#include <utility>

#include "core/GameTime.h"
#include "game/Inventory.h"
#include "game/Entity.h"
#include "game/Player.h"
#include "gui/Console.h"
#include "gui/Hud.h"
#include "gui/Interface.h"
#include "gui/Menu.h"
#include "gui/MiniMap.h"
#include "gui/hud/SecondaryInventory.h"
#include "gui/widget/TextInputWidget.h"
#include "scene/GameSound.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"
#include "util/Cast.h"
#include "util/String.h"


namespace script {

namespace {

class BookCommand : public Command {
	
public:
	
	BookCommand() : Command("book") { }
	
	Result execute(Context & context) override {
		
		HandleFlags("aem") {
			if(flg & flag('a')) {
				g_playerBook.forcePage(BOOKMODE_MINIMAP);
			}
			if(flg & flag('e')) {
				g_playerBook.forcePage(BOOKMODE_SPELLS);
			}
			if(flg & flag('m')) {
				g_playerBook.forcePage(BOOKMODE_QUESTS);
			}
		}
		
		std::string command = context.getWord();
		
		if(command == "open") {
			g_playerBook.open();
		} else if(command == "close") {
			g_playerBook.close();
		} else if(command == "change") {
			// Nothing to do, mode already changed by flags.
		} else {
			ScriptWarning << "unexpected command: " << options << " \"" << command << "\"";
		}
		
		DebugScript(' ' << options << ' ' << command);
		
		return Success;
	}
	
};

class ConsoleHistoryCommand : public Command { // TODOA not working yet
	
	public:
	
		ConsoleHistoryCommand() : Command("hist") { }
		
		Result execute(Context & context) override {
			
			g_console.list(context.getWord(), true);
			
			return Success;
		}
};

class TextInputCommand : public Command { // TODOA not working yet
	
	public:
	
		TextInputCommand() : Command("ask") { }
		
		/**
		 * TODO: LogHelp()
		 * ask "<question>" <stringVar>
		 */
		Result execute(Context & context) override {
			
			std::string strQuestion = context.getWord();
			std::string strVar = context.autoVarNameForScope(true, context.getWord());
			
			std::string strVal = context.getStringVar(strVar);
			
			if(strVal == "void") {
				if(!SETVarValueText(context.getEntity()->m_variables, strVar, "")) {
					ScriptWarning << "Unable to create variable " << strVar;
					return Failed;
				}
			}
			
			g_gameTime.pause(GameTime::PauseUser);
			
			//TODOA see how ScriptConsole::draw() works ! it pauses the game and receives the input focus, try to not pause the game and make inscribing (hand writing) speed depend on objectknowledge 1s-5s skill 100-20  =  (1+((100-skill)/20))s. Inscribing will then fail if player moves or enter combat mode. Sound hand writing when finished. Sound dammit if fails. Store a history to not require re-typing everything.
/*
			if(!m_enabled) {
				return;
			}
			
			UseRenderState state(render2D());
			
			Color background = Color::black;
			background.a = 150;
			Color line = Color::gray(0.8f);
			Color selection = Color::yellow;
			selection.a = 40;
			
			{
				Rectf box = Rectf(g_size);
				box.bottom = box.top + float((m_buffer.lines() + 1) * hFontDebug->getLineHeight()) + 4.f;
				EERIEDrawBitmap(box, 0.f, nullptr, background);
			}
			
			Vec2i pos(0);
			for(size_t i = 0; i < m_buffer.lines(); i++) {
				hFontDebug->draw(pos, m_buffer.line(i), Color::white);
				pos.y += hFontDebug->getLineHeight();
			}
			
			pos.y += 1;
			
			drawLine(Vec2f(0, pos.y), Vec2f(g_size.width(), pos.y), 0.f, line);
			
			pos.y += 2;
			
			pos.x += hFontDebug->draw(pos, "> ", Color::green).advance();
			
			std::string_view displayText = text();
			std::string buffer;
			if(!strVal.empty()) {
				buffer = displayText;
				buffer.insert(cursorPos(), strVal);
				displayText = buffer;
			}
			size_t displayCursorPos = cursorPos() + editCursorPos();
			
			// Highlight edit area
			if(!strVal.empty()) {
				int left = hFontDebug->getTextSize(displayText.substr(0, cursorPos())).advance();
				int right = hFontDebug->getTextSize(displayText.substr(0, cursorPos() + strVal.size())).advance();
				int height = hFontDebug->getLineHeight();
				Rectf box = Rectf(Rect(pos + Vec2i(left, 0), right - left, height));
				EERIEDrawBitmap(box, 0.f, nullptr, selection);
			}
			
			// Draw text
			s32 x = hFontDebug->draw(pos.x, pos.y, displayText, Color::white).next();
			
			// Preview autocomplete
			std::string_view completion = m_completion.second;
			if(cursorPos() == text().size() && cursorPos() < completion.size()) {
				hFontDebug->draw(x, pos.y, completion.substr(cursorPos()), Color::gray(0.5f));
			}
			
			// Draw cursor
			bool blink = true;
			if(mainApp->getWindow()->hasFocus()) {
				blink = timeWaveSquare(g_platformTime.frameStart(), 1200ms);
			}
			if(blink) {
				int cursor = x;
				if(cursorPos() != displayText.size()) {
					cursor = pos.x + hFontDebug->getTextSize(displayText.substr(0, displayCursorPos)).next();
				}
				drawLine(Vec2f(cursor, pos.y), Vec2f(cursor, pos.y + hFontDebug->getLineHeight()), 0.f, line);
				if(editCursorLength() > 0) {
					int endX = pos.x + hFontDebug->getTextSize(displayText.substr(0, displayCursorPos + editCursorLength())).next();
					drawLine(Vec2f(endX, pos.y), Vec2f(endX, pos.y + hFontDebug->getLineHeight()), 0.f, line);
				}
			}
			
			pos.y += hFontDebug->getLineHeight();
			
			pos.y += 1;
			
			drawLine(Vec2f(0, pos.y), Vec2f(g_size.width(), pos.y), 0.f, line);
			
			pos.y += 2;
			
			// Draw error message and suggestions
			if(!m_error.second.empty()) {
				Vec2i errorPos = pos;
				errorPos.x += hFontDebug->getTextSize(std::string_view(text()).substr(0, m_error.first)).advance();
				hFontDebug->draw(errorPos + Vec2i(1), m_error.second, Color::black);
				hFontDebug->draw(errorPos, m_error.second, Color::red);
			} else if(!m_suggestions.empty()) {
				Vec2i suggestionPos = pos;
				size_t position = 0;
				size_t start = 0;
				if(m_selection >= int(MaxVisibleSuggestions)) {
					start = m_selection - MaxVisibleSuggestions + 1;
				}
				for(size_t i = start; i < m_suggestions.size(); i++) {
					if(i == start + MaxVisibleSuggestions - (start != 0)) {
						std::ostringstream oss;
						oss << "... " << (start + 1) << " - " << (start + MaxVisibleSuggestions - (start != 0))
								<< " / " << m_suggestions.size();
						hFontDebug->draw(suggestionPos + Vec2i(1), oss.str(), Color::black);
						hFontDebug->draw(suggestionPos, oss.str(), Color::red);
						break;
					}
					if(m_suggestions[i].first != position) {
						position = m_suggestions[i].first;
						suggestionPos.x = pos.x + hFontDebug->getTextSize(std::string_view(text()).substr(0, position)).advance();
					}
					if(start != 0 && i == start) {
						hFontDebug->draw(suggestionPos + Vec2i(1), "...", Color::black);
						hFontDebug->draw(suggestionPos, "...", Color::red);
						suggestionPos.y += hFontDebug->getLineHeight();
					}
					if(int(i) + 1 == m_selection) {
						int width = hFontDebug->getTextSize(m_suggestions[i].second).width();
						int height = hFontDebug->getLineHeight();
						Rectf highlight = Rectf(Rect(suggestionPos - Vec2i(2, 1), width + 3, height + 2));
						EERIEDrawBitmap(highlight, 0.f, nullptr, selection);
						drawLineRectangle(highlight, 0.f, background);
					}
					hFontDebug->draw(suggestionPos + Vec2i(1), m_suggestions[i].second, Color::black);
					hFontDebug->draw(suggestionPos, m_suggestions[i].second, Color::white);
					suggestionPos.y += hFontDebug->getLineHeight();
				}
			}
//*/			
			
			ARX_UNICODE_DrawTextInRect(hFontMenu, Vec2f(200,200), 999999, strQuestion, Color(232, 204, 142));
			
			TextInputWidget textbox(hFontMenu, strVal, Rectf(200,220,300,20));
			//textbox.setText(strVal);
			if(!textbox.click()) {
				ScriptWarning << "Unable create text input " << strQuestion << ", " << strVar << ", " << strVal;
				return Failed;
			}
			
			g_gameTime.resume(GameTime::PauseUser); // TODOA wait enter or esc key ? or is it automatic?
			
			if(!textbox.text().empty()) {
				if(!SETVarValueText(context.getEntity()->m_variables, strVar, context.getStringVar(textbox.text(), context.getEntity()))) {
					ScriptWarning << "Unable to set variable " << strVar;
					return Failed;
				}
			}
			
			textbox.unfocus();
			
			return Success;
		}
};

class CloseStealBagCommand : public Command {
	
public:
	
	CloseStealBagCommand() : Command("closestealbag") { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		if(!(player.Interface & INTER_STEAL)) {
			return Success;
		}
		
		g_secondaryInventoryHud.close();
		
		return Success;
	}
	
};

class NoteCommand : public Command {
	
public:
	
	NoteCommand() : Command("note") { }
	
	Result execute(Context & context) override {
		
		Note::Type type;
		std::string tpname = context.getWord();
		if(tpname == "note") {
			type = Note::SmallNote;
		} else if(tpname == "notice") {
			type = Note::Notice;
		} else if(tpname == "book") {
			type = Note::Book;
		} else {
			ScriptWarning << "unexpected note type: " << tpname;
			type = Note::SmallNote;
		}
		
		std::string text = context.getWord();
		
		DebugScript(' ' << tpname << ' ' << text);
		
		ARX_INTERFACE_NoteOpen(type, toLocalizationKey(text));
		
		return Success;
	}
	
};

struct PrintGlobalVariables {
	
	std::string m_filter;
	
	explicit PrintGlobalVariables(std::string_view filter = "") : m_filter(filter) { }
	
};

std::ostream & operator<<(std::ostream & os, const PrintGlobalVariables & data) {
	
	if(data.m_filter.size() > 0) {
		static std::regex * re = nullptr;
		re = util::prepareRegex(re, data.m_filter);
		if(!re) {
			LogError << "invalid regex: " << data.m_filter;
			return os;
		}
		
		for(const SCRIPT_VAR & var : svar) {
			if (std::regex_search(var.name, *re)) {
				os << var << '\n';
			}
		}
	} else {
		for(const SCRIPT_VAR & var : svar) {
			os << var << '\n';
		}
	}
	
	return os;
}

class ShowGlobalsCommand : public Command {
	
public:
	
	ShowGlobalsCommand() : Command("showglobals") { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		LogInfo << "Global variables:\n" << PrintGlobalVariables();
		
		return Success;
	}
	
};

struct PrintLocalVariables {
	
	Entity * m_entity;
	std::string m_filter;
	
	explicit PrintLocalVariables(Entity * entity, std::string_view filter = "") : m_entity(entity), m_filter(filter) { }
	
};

std::ostream & operator<<(std::ostream & os, const PrintLocalVariables & data) {
	
	if(data.m_filter.size() > 0) {
		static std::regex * re = nullptr;
		re = util::prepareRegex(re, data.m_filter);
		if(!re) {
			LogError << "invalid regex: " << data.m_filter;
			return os;
		}
		
		for(const SCRIPT_VAR & var : data.m_entity->m_variables) {
			if (std::regex_search(var.name, *re)) {
				os << var << '\n';
			}
		}
	} else {
		for(const SCRIPT_VAR & var : data.m_entity->m_variables) {
			os << var << '\n';
		}
	}
	
	return os;
}

static std::string getEventAndStackInfo(Context & context) {
	std::stringstream s;
	
	if(context.getMessage() < SM_MAXCMD) {
		s << " at Event " << ScriptEvent::name(context.getMessage());
	}
	
	if(context.getSender()) {
		s << " sent from " << context.getSender()->idString();
	}
	
	if(context.getParameters().size() > 0) {
		s << " with parameters (";
		for(std::string s2 : context.getParameters()) {
			s << s2 << " ";
		}
		s << ")";
	}
	
	s << context.getGoSubCallStack(" at GoSub {CallStackId(FromPosition): ", " }, " + context.getPosLineColumnInfo());
	
	return s.str();
}

class ShowLocalsCommand : public Command {
	
public:
	
	ShowLocalsCommand() : Command("showlocals", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string filter;
		HandleFlags("f") {
			if(flg & flag('f')) {
				filter = context.getStringVar(context.getWord());
			}
		}
		
		DebugScript("" << filter);
		
		LogInfo << "Local variables for " << context.getEntity()->idString() << getEventAndStackInfo(context) << ":\n"
			<< PrintLocalVariables(context.getEntity(), filter);
		
		return Success;
	}
	
};

class ShowVarsCommand : public Command {
	
public:
	
	ShowVarsCommand() : Command("showvars", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string filter;
		HandleFlags("f") {
			if(flg & flag('f')) {
				filter = context.getStringVar(context.getWord());
			}
		}
		
		DebugScript("" << filter);
		
		LogInfo << "Local variables for " << context.getEntity()->idString() << getEventAndStackInfo(context) << ":\n"
			<< PrintLocalVariables(context.getEntity(), filter);
		LogInfo << "Global variables:\n" << PrintGlobalVariables(filter);
		
		return Success;
	}
	
};

class PlayerInterfaceCommand : public Command {
	
public:
	
	PlayerInterfaceCommand() : Command("playerinterface") { }
	
	Result execute(Context & context) override {
		
		bool smooth = false;
		HandleFlags("s") {
			smooth = test_flag(flg, 's');
		}
		
		std::string command = context.getWord();
		
		DebugScript(' ' << options << ' ' << command);
		
		if(command == "hide") {
			g_hudRoot.playerInterfaceFader.requestFade(FadeDirection_Out, smooth);
		} else if(command == "show") {
			g_hudRoot.playerInterfaceFader.requestFade(FadeDirection_In, smooth);
		} else {
			ScriptWarning << "unknown command: " << command;
			return Failed;
		}
		
		return Success;
	}
	
};

class PopupCommand : public Command {
	
public:
	
	PopupCommand() : Command("popup") { }
	
	Result execute(Context & context) override {
		
		std::string message = context.getWord();
		
		DebugScript(' ' << message);
		
		return Success;
	}
	
};

class EndIntroCommand : public Command {
	
public:
	
	EndIntroCommand() : Command("endintro") { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		ARX_SOUND_MixerStop(ARX_SOUND_MixerGame);
		ARX_MENU_Launch(false);
		
		return Success;
	}
	
};

class EndGameCommand : public Command {
	
public:
	
	EndGameCommand() : Command("endgame") { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		ARX_SOUND_MixerStop(ARX_SOUND_MixerGame);
		ARX_MENU_Launch(false);
		ARX_MENU_Clicked_CREDITS();
		
		return Success;
	}
	
};

class MapMarkerCommand : public Command {
	
public:
	
	MapMarkerCommand() : Command("mapmarker") { }
	
	Result execute(Context & context) override {
		
		bool remove = false;
		HandleFlags("r") {
			remove = test_flag(flg, 'r');
		}
		
		if(remove) {
			
			std::string marker = context.getWord();
			
			DebugScript(' ' << options << ' ' << marker);
			
			g_miniMap.mapMarkerRemove(toLocalizationKey(marker));
			
		} else {
			
			float x = context.getFloat();
			float y = context.getFloat();
			float level = context.getFloat();
			
			std::string marker = context.getWord();
			
			DebugScript(' ' << options << ' ' << x << ' ' << y << ' ' << level << ' ' << marker);
			
			if(level < 1.f || !util::is_in_range<s16>(level)) {
				ScriptError << "Invalid map level: " << level;
				return Failed;
			}
			
			g_miniMap.mapMarkerAdd(Vec2f(x, y), MapLevel(u32(level - 1)), std::string(toLocalizationKey(marker)));
			
		}
		
		return Success;
	}
	
};

class DrawSymbolCommand : public Command {
	
public:
	
	DrawSymbolCommand() : Command("drawsymbol", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string symbol = context.getWord();
		
		GameDuration duration = std::chrono::duration<float, std::milli>(context.getFloat());
		
		DebugScript(' ' << symbol << ' ' << toMsf(duration));
		
		ARX_SPELLS_RequestSymbolDraw(context.getEntity(), symbol, duration);
		
		return Success;
	}
	
};

} // anonymous namespace

void setupScriptedInterface() {
	
	ScriptEvent::registerCommand(std::make_unique<BookCommand>());
	ScriptEvent::registerCommand(std::make_unique<CloseStealBagCommand>());
	ScriptEvent::registerCommand(std::make_unique<NoteCommand>());
	ScriptEvent::registerCommand(std::make_unique<ShowGlobalsCommand>());
	ScriptEvent::registerCommand(std::make_unique<ShowLocalsCommand>());
	ScriptEvent::registerCommand(std::make_unique<ShowVarsCommand>());
	ScriptEvent::registerCommand(std::make_unique<PlayerInterfaceCommand>());
	ScriptEvent::registerCommand(std::make_unique<PopupCommand>());
	ScriptEvent::registerCommand(std::make_unique<EndIntroCommand>());
	ScriptEvent::registerCommand(std::make_unique<EndGameCommand>());
	ScriptEvent::registerCommand(std::make_unique<MapMarkerCommand>());
	ScriptEvent::registerCommand(std::make_unique<DrawSymbolCommand>());
	ScriptEvent::registerCommand(std::make_unique<TextInputCommand>());
	ScriptEvent::registerCommand(std::make_unique<ConsoleHistoryCommand>());
}

} // namespace script
