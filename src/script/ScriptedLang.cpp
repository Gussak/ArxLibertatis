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

#include "script/ScriptedLang.h"

#include <memory>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

#include "ai/Paths.h"
#include "core/GameTime.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "graphics/Math.h"
#include "scene/Interactive.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"
#include "util/Number.h"


namespace script {

namespace {

class NopCommand : public Command {
	
public:
	
	NopCommand() : Command("nop") { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		return Success;
	}
	
	Result peek(Context & context) override { return execute(context); }
	
};

class GotoCommand : public Command {
	
	bool sub;
	
public:
	
	GotoCommand(std::string_view command, bool _sub = false) : Command(command), sub(_sub) { }
	
	Result execute(Context & context) override {
		
		std::string label = context.getWord();
		
		DebugScript(' ' << label);
		
		if(!sub) {
			size_t pos = context.skipCommand();
			if(pos != size_t(-1)) {
				ScriptWarning << "unexpected text at " << pos;
			}
		}
		
		if(!context.jumpToLabel(label, sub)) {
			ScriptError << "unknown label \"" << label << '"';
			return AbortError;
		}
		
		return Jumped;
	}
	
	Result peek(Context & context) override { return execute(context); }
	
};

class AbortCommand : public Command {
	
	Result result;
	
public:
	
	AbortCommand(std::string_view command, Result _result) : Command(command), result(_result) { }
	
	Result execute(Context & context) override {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		return result;
	}
	
	Result peek(Context & context) override { return execute(context); }
	
};

class RandomCommand : public Command {
	
public:
	
	RandomCommand() : Command("random") { }
	
	Result execute(Context & context) override {
		
		float chance = glm::clamp(context.getFloat(), 0.f, 100.f);
		
		DebugScript(' ' << chance);
		
		float t = Random::getf(0.f, 100.f);
		if(chance < t) {
			context.skipBlock();
		}
		
		return Success;
	}
	
};

class ReturnCommand : public Command {
	
public:
	
	ReturnCommand() : Command("return") { }
	
	Result execute(Context & context) override {
		
		DebugScript("");
		
		if(!context.returnToCaller()) {
			ScriptError << "return failed";
			return AbortError;
		}
		
		return Success;
	}
	
	Result peek(Context & context) override { return execute(context); }
	
};

class SetMainEventCommand : public Command {
	
public:
	
	explicit SetMainEventCommand(std::string_view command) : Command(command, AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string event = context.getWord();
		
		DebugScript(' ' << event);
		
		if(context.getEntity()) {
			context.getEntity()->mainevent = ScriptEventName::parse(event);
		}
		
		return Success;
	}
	
};

class StartStopTimerCommand : public Command {
	
	const bool start;
	
public:
	
	StartStopTimerCommand(std::string_view command, bool _start) : Command(command), start(_start) { }
	
	Result execute(Context & context) override {
		
		std::string timer = context.getWord();
		
		DebugScript(' ' << timer);
		
		size_t t;
		if(timer == "timer1") {
			t = 0;
		} else if(timer == "timer2") {
			t = 1;
		} else if(timer == "timer3") {
			t = 2;
		} else if(timer == "timer4") {
			t = 3;
		} else {
			ScriptWarning << "invalid timer: " << timer;
			return Failed;
		}
		
		Entity * entity = context.getEntity();
		if(start) {
			entity->m_scriptTimers[t] = g_gameTime.now();
			if(entity->m_scriptTimers[t] == 0) {
				entity->m_scriptTimers[t] += 1ms;
			}
		} else {
			entity->m_scriptTimers[t] = 0;
		}
		
		return Success;
	}
	
};

class SendEventCommand : public Command {
	
public:
	
	SendEventCommand() : Command("sendevent") { }
	
	Result execute(Context & context) override {
		
		EntityFlags sendto = 0;
		bool radius = false;
		bool zone = false;
		bool group = false;
		HandleFlags("gfinrz") {
			group = test_flag(flg, 'g');
			sendto |= (flg & flag('f')) ? IO_FIX : EntityFlags(0);
			sendto |= (flg & flag('i')) ? IO_ITEM : EntityFlags(0);
			sendto |= (flg & flag('n')) ? IO_NPC : EntityFlags(0);
			radius = test_flag(flg, 'r');
			zone = test_flag(flg, 'z');
		}
		if(!sendto) {
			sendto = IO_NPC;
		}
		
		std::string groupname;
		if(group) {
			groupname = context.getStringVar(context.getWord());
		}
		
		std::string eventname = context.getWord();
		
		std::string zonename;
		if(zone) {
			zonename = context.getStringVar(context.getWord());
		}
		
		float rad = 0.f;
		if(radius) {
			rad = context.getFloat();
		}
		
		std::string target;
		if(!group && !zone && !radius) {
			target = context.getStringVar(context.getWord());
			// TODO(broken-scripts) work around broken scripts
			for(size_t i = 0; i < SM_MAXCMD; i++) {
				std::string_view event = ScriptEvent::name(ScriptMessage(i));
				arx_assert(boost::starts_with(event, "on "));
				if(target == event.substr(3)) {
					std::swap(target, eventname);
					break;
				}
			}
		}
		
		ScriptParameters parameters = ScriptParameters::parse(context.getWord());
		
		if(radius) {
			DebugScript(' ' << eventname << ' ' << parameters << " to " << (group ? "group " + groupname : "everyone") << " in radius " << rad);
		} else if(zone) {
			DebugScript(' ' << eventname << ' ' << parameters << " to " << (group ? "group " + groupname : "everyone") << " in zone " << zonename);
		} else {
			DebugScript(' ' << eventname << ' ' << parameters << " to " << target);
		}
		
		ScriptEventName event = ScriptEventName::parse(eventname);
		
		Entity * sender = context.getEntity();
		
		if(radius) {
			
			for(Entity & entity : entities(sendto)) {
				
				if(entity == *sender || (entity.ioflags & (IO_CAMERA | IO_MARKER))) {
					continue;
				}
				
				if(group && entity.groups.find(groupname) == entity.groups.end()) {
					continue;
				}
				
				Vec3f _pos  = GetItemWorldPosition(&entity);
				Vec3f _pos2 = GetItemWorldPosition(sender);
				if(!fartherThan(_pos, _pos2, rad)) {
					sender->stat_sent++;
					Stack_SendIOScriptEvent(sender, &entity, event, parameters);
				}
				
			}
			
		} else if(zone) {
			
			Zone * ap = getZoneByName(zonename);
			if(!ap) {
				ScriptWarning << "unknown zone: " << zonename;
				return Failed;
			}
			
			for(Entity & entity : entities(sendto)) {
				
				if(entity.ioflags & (IO_CAMERA | IO_MARKER)) {
					continue;
				}
				
				if(group && entity.groups.find(groupname) == entity.groups.end()) {
					continue;
				}
				
				Vec3f _pos = GetItemWorldPosition(&entity);
				if(ARX_PATH_IsPosInZone(ap, _pos)) {
					sender->stat_sent++;
					Stack_SendIOScriptEvent(sender, &entity, event, parameters);
				}
				
			}
			
		} else if(group) {
			
			for(Entity & entity : entities) {
				
				if(entity == *sender) {
					continue;
				}
				
				if(entity.groups.find(groupname) == entity.groups.end()) {
					continue;
				}
				
				sender->stat_sent++;
				Stack_SendIOScriptEvent(sender, &entity, event, parameters);
				
			}
			
		} else {
			
			Entity * entity = entities.getById(target, sender);
			if(!entity) {
				DebugScript(": target does not exist");
				return Failed;
			}
			
			sender->stat_sent++;
			Stack_SendIOScriptEvent(sender, entity, event, parameters);
			
		}
		
		return Success;
	}
	
};

class SetEventCommand : public Command {
	
public:
	
	SetEventCommand() : Command("setevent", AnyEntity) { }
	
	Result execute(Context & context) override {
		
		std::string name = context.getWord();
		bool enable = context.getBool();
		
		DebugScript(' ' << name << ' ' << enable);
		
		DisabledEvents mask = ScriptEventName::parse(name).toDisabledEventsMask();
		if(!mask) {
			ScriptWarning << "cannot disable event: " << name;
			return Failed;
		}
		
		if(enable) {
			context.getEntity()->m_disabledEvents &= ~mask;
		} else {
			context.getEntity()->m_disabledEvents |= mask;
		}
		
		return Success;
	}
	
};

class IfCommand : public Command {
	
	// TODO(script) move to context?
	static ValueType getVar(const Context & context, std::string_view var, std::string & s, float & f, ValueType def) {
		
		char c = (var.empty() ? '\0' : var[0]);
		
		switch(c) {
			
			case '^': {
				
				long l;
				switch(getSystemVar(context, var, s, &f, &l)) {
					
					case TYPE_TEXT: return TYPE_TEXT;
					
					case TYPE_FLOAT: return TYPE_FLOAT;
					
					case TYPE_LONG: {
						f = static_cast<float>(l);
						return TYPE_FLOAT;
					}
					
				}
				
				arx_unreachable();
			}
			
			case '#': {
				f = GETVarValueLong(svar, var);
				return TYPE_FLOAT;
			}
			
			case '\xA7': {
				f = GETVarValueLong(context.getEntity()->m_variables, var);
				return TYPE_FLOAT;
			}
			
			case '&': {
				f = GETVarValueFloat(svar, var);
				return TYPE_FLOAT;
			}
			
			case '@': {
				f = GETVarValueFloat(context.getEntity()->m_variables, var);
				return TYPE_FLOAT;
			}
			
			case '$': {
				s = GETVarValueText(svar, var);
				return TYPE_TEXT;
			}
			
			case '\xA3': {
				s = GETVarValueText(context.getEntity()->m_variables, var);
				return TYPE_TEXT;
			}
			
			default: {
				if(def == TYPE_TEXT) {
					s = var;
					return TYPE_TEXT;
				} else {
					f = util::parseFloat(var);
					return TYPE_FLOAT;
				}
			}
			
		}
		
	}
	
	class Operator {
		
		std::string m_name;
		ValueType m_type;
		
	public:
		
		Operator(std::string_view name, ValueType type) : m_name(name), m_type(type) { }
		
		virtual ~Operator() = default;
		
		virtual bool number(const Context & context, float left, float right) {
			ARX_UNUSED(left), ARX_UNUSED(right);
			ScriptWarning << "operator " << m_name << " is not aplicable to numbers";
			return true;
		}
		
		virtual bool text(const Context & context, std::string_view left, std::string_view right) {
			ARX_UNUSED(left), ARX_UNUSED(right);
			ScriptWarning << "operator " << m_name << " is not aplicable to text";
			return false;
		}
		
		std::string_view getName() { return "if"; }
		const std::string & getOperator() { return m_name; }
		ValueType getType() { return m_type; }
		
	};
	
	std::map<std::string_view, std::unique_ptr<Operator>> m_operators;
	
	void addOperator(Operator * op) {
		[[maybe_unused]] auto res = m_operators.emplace(op->getOperator(), op);
		arx_assert_msg(res.second, "Duplicate script 'if' operator name: %s", op->getOperator().c_str());
	}
	
	class IsElementOperator : public Operator {
		
	public:
		
		IsElementOperator() : Operator("iselement", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view seek, std::string_view text) override {
			ARX_UNUSED(context);
			
			for(size_t pos = 0, next; next = text.find(' ', pos), true ; pos = next + 1) {
				
				if(next == std::string_view::npos) {
					return (text.substr(pos, text.length() - pos) == seek);
				}
				
				if(text.substr(pos, next - pos) == seek) {
					return true;
				}
				
			}
			
			return false; // for stupid compilers
		}
		
	};
	
	class IsClassOperator : public Operator {
		
	public:
		
		IsClassOperator() : Operator("isclass", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view left, std::string_view right) override {
			ARX_UNUSED(context);
			return (left.find(right) != std::string_view::npos || right.find(left) != std::string_view::npos);
		}
		
	};
	
	class IsGroupOperator : public Operator {
		
	public:
		
		IsGroupOperator() : Operator("isgroup", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view obj, std::string_view group) override {
			
			Entity * t = entities.getById(obj, context.getEntity());
			
			return (t != nullptr && t->groups.find(group) != t->groups.end());
		}
		
	};
	
	class NotIsGroupOperator : public Operator {
		
	public:
		
		NotIsGroupOperator() : Operator("!isgroup", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view obj, std::string_view group) override {
			
			Entity * t = entities.getById(obj, context.getEntity());
			
			return (t != nullptr && t->groups.find(group) == t->groups.end());
		}
		
	};
	
	class IsTypeOperator : public Operator {
		
	public:
		
		IsTypeOperator() : Operator("istype", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view obj, std::string_view type) override {
			
			Entity * t = entities.getById(obj, context.getEntity());
			
			ItemType flag = ARX_EQUIPMENT_GetObjectTypeFlag(type);
			if(!flag) {
				ScriptWarning << "unknown type: " << type;
				return false;
			}
			
			return (t != nullptr && (t->type_flags & flag));
		}
		
	};
	
	class IsInOperator : public Operator {
		
	public:
		
		IsInOperator() : Operator("isin", TYPE_TEXT) { }
		
		bool text(const Context & context, std::string_view needle, std::string_view haystack) override {
			ARX_UNUSED(context);
			return haystack.find(needle) != std::string_view::npos;
		}
		
	};
	
	class EqualOperator : public Operator {
		
	public:
		
		EqualOperator() : Operator("==", TYPE_FLOAT) { }
		
		bool text(const Context & context, std::string_view left, std::string_view right) override {
			ARX_UNUSED(context);
			return left == right;
		}
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left == right;
		}
		
	};
	
	class NotEqualOperator : public Operator {
		
	public:
		
		NotEqualOperator() : Operator("!=", TYPE_FLOAT) { }
		
		bool text(const Context & context, std::string_view left, std::string_view right) override {
			ARX_UNUSED(context);
			return left != right;
		}
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left != right;
		}
		
	};
	
	class LessEqualOperator : public Operator {
		
	public:
		
		LessEqualOperator() : Operator("<=", TYPE_FLOAT) { }
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left <= right;
		}
		
	};
	
	class LessOperator : public Operator {
		
	public:
		
		LessOperator() : Operator("<", TYPE_FLOAT) { }
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left < right;
		}
		
	};
	
	class GreaterEqualOperator : public Operator {
		
	public:
		
		GreaterEqualOperator() : Operator(">=", TYPE_FLOAT) { }
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left >= right;
		}
		
	};
	
	class GreaterOperator : public Operator {
		
	public:
		
		GreaterOperator() : Operator(">", TYPE_FLOAT) { }
		
		bool number(const Context & context, float left, float right) override {
			ARX_UNUSED(context);
			return left > right;
		}
		
	};
	
public:
	
	IfCommand() : Command("if") {
		addOperator(new IsElementOperator);
		addOperator(new IsClassOperator);
		addOperator(new IsGroupOperator);
		addOperator(new NotIsGroupOperator);
		addOperator(new IsTypeOperator);
		addOperator(new IsInOperator);
		addOperator(new EqualOperator);
		addOperator(new NotEqualOperator);
		addOperator(new LessEqualOperator);
		addOperator(new LessOperator);
		addOperator(new GreaterEqualOperator);
		addOperator(new GreaterOperator);
	}
	
#ifdef ARX_DEBUG
#pragma GCC push_options
#pragma GCC optimize ("O0") //required to let the breakpoints work properly
#endif
	Result compare(Context & context, bool & condition, bool & comparisonDetected, bool & bJustConsumeTheWords) {
		std::string left  = context.getWord();
		std::string op    = context.getWord();
		std::string right = context.getWord();
		auto it = m_operators.find(op);
		if(it == m_operators.end()) {
			ScriptWarning << "unknown operator: " << op;
			comparisonDetected = false;
			return Failed;
		}else{
			comparisonDetected = true;
		}
		if(bJustConsumeTheWords){
			return Success;
		}

		float f1, f2;
		std::string s1, s2;
		ValueType t1 = getVar(context, left, s1, f1, it->second->getType());
		ValueType t2 = getVar(context, right, s2, f2, t1);
		
		if(t1 != t2) {
			ScriptWarning << "incompatible types: \"" << left << "\" (" << (t1 == TYPE_TEXT ? "text" : "number") << ") and \"" << right << "\" (" << (t2 == TYPE_TEXT ? "text" : "number") << ')';
			return Failed;
		}
		
		if(t1 == TYPE_TEXT) {
			condition = it->second->text(context, s1, s2);
			DebugScript(" \"" << left << "\" " << op << " \"" << right << "\"  ->  \"" << s1 << "\" " << op << " \"" << s2 << "\"  ->  " << (condition ? "true" : "false")); // TODO fix formatting in Logger and use std::boolalpha
		} else {
			condition = it->second->number(context, f1, f2);
			DebugScript(" \"" << left << "\" " << op << " \"" << right << "\"  ->  " << f1 << " " << op << " " << f2 << "  ->  " << (condition ? "true" : "false"));
		}
		
		return Success;
	}
	
	bool recursiveLogicOperationByWord(Context & context, std::string & wordCheck, bool & condition, Result & res, bool & bJustConsumeTheWords) {
		if(boost::equals(wordCheck, "and")) {
			res = recursiveLogicOperation(context, condition, 'a', bJustConsumeTheWords);
		} else
		if(boost::equals(wordCheck, "or")) {
			res = recursiveLogicOperation(context, condition, 'o', bJustConsumeTheWords);
		} else
		if(boost::equals(wordCheck, "not")) {
			res = recursiveLogicOperation(context, condition, 'n', bJustConsumeTheWords);
		}else{
			return false;
		}
		return true;
	}
	/**
	 * it is always 'if(and(...))' or 'if(or(...))' or the default 'if(SomeComparison)'
	 * so all nested 'and()' or 'or()' or 'comparison' must be inside a initial abrangent/top 'and()' or 'or()' 
	 */
	Result recursiveLogicOperation(Context & context, bool & condition, char logicOp, bool & bJustConsumeTheWords) {
		Result res = Success;
		size_t positionBeforeWord;
		std::string wordCheck;
		int iCount=0;
		bool bJustConsumeTheWordsThisLoopAndNestedOnly = bJustConsumeTheWords;
		while(true){
			if(res != Success) {return res;}; // in case of errors found in the script.
			iCount++;
			context.skipWhitespaceAndComment();
			positionBeforeWord = context.getPosition(); //Put after skip new lines.
			wordCheck = context.getWord();
			
			//logic operation loop end detected. this is not required in the script if the next thing to be found is a '{'
			if( boost::equals(wordCheck, ";") ) { 
				break;
			}
			
			//block begin detected, end recursive logic
			if( boost::equals(wordCheck, "{") ) { 
				context.seekToPosition(positionBeforeWord); //to let the block be proccessed or skipped properly later
				break;
			}
			
			//         ex.: if(comparison && comparison && comparison) {
			//                      1     2       3     4      5       6
			// iCount%2             1     0       1     0      1       0
			if(iCount%2 == 1){ //comparison or recursive logic nesting
				if(recursiveLogicOperationByWord(context, wordCheck, condition, res, bJustConsumeTheWordsThisLoopAndNestedOnly)){
					if(logicOp == 'n') { //not
						condition = !condition;
						break; //this break breaks the loop. logic NOT accepts only one comparison or nested logical operator
					}
					continue;
				} else { //check for normal comparison
					//recursive logic operators or blanks were not detected. undo the wordCheck position
					context.seekToPosition(positionBeforeWord);
					// it needs to determine if the next thing is a comparison as the block begin char was not detected
					bool comparisonDetected = false;
					res = compare(context, condition, comparisonDetected, bJustConsumeTheWordsThisLoopAndNestedOnly);
					if(comparisonDetected){
						if(!bJustConsumeTheWordsThisLoopAndNestedOnly) {
							if( logicOp == 'n') { //not
								condition = !condition; 
								break; //this break breaks the loop. logic NOT accepts only one comparison or nested logical operator
							}
							switch(logicOp) { //this is just an optimization
								//if logic AND fails or if logic OR succeeds, the remaining comparisons in this loop (and everything else nested) only will be skipped
								case 'a': if(!condition) bJustConsumeTheWordsThisLoopAndNestedOnly=true; break;
								case 'o': if( condition) bJustConsumeTheWordsThisLoopAndNestedOnly=true; break;
								default: arx_assert_msg(false, "Invalid logical operator mode: %c", logicOp); break;
							}
						}
						continue;
					} else {
						// it is not a comparison, this is a command outside of a block, so restore position and end the loop
						context.seekToPosition(positionBeforeWord);
						break;
					}
				}
			}
			
			// logic connector say the next thing is a logical result to process. They also make reading the script more clear. they are always in-between, 2nd 4th 6th... word on.
			if(iCount%2 == 0){
				if(logicOp == 'n') { //not
					ScriptError << "the not() logical operator only accepts one comparison or nested logic operator";
					return Failed;
				}
				
				switch(logicOp) {
					case 'a': //and
						if( wordCheck[0] == ',' || boost::equals(wordCheck, "&&") ) continue; 
						break;
					case 'o': //or
						if( wordCheck[0] == ',' || boost::equals(wordCheck, "||") ) continue;
						break;
					default: arx_assert_msg(false, "Invalid logical operator mode: %c", logicOp); break;
				}
				
				// the absense of a coherent logic connector is expected and means the end of a nesting ex.: if( AND( ... && or(...) && not(and(...)) ) ){...} //at '){' or some command like 'Set' also ends the logic
				context.seekToPosition(positionBeforeWord);
				break;
			}
			
		} //end while
		
		return Success;
	}
	Result execute(Context & context) override {
		Result res; //this overrides processing condition result, in case of Failed
		bool condition = false; //this will be set by the function calls
		bool bJustConsumeTheWords = false; //when a multi nested condition can quickly end, this will be set to true so all words (related to logic operations and comparisons) are consumed and the block/command can be safely reached and processed or skipped.
		context.skipWhitespaceAndComment();
		size_t positionBeforeWord = context.getPosition(); //this is used to undo the wordCkeck position. Put after skip new lines.
		std::string wordCheck = context.getWord();
		if(recursiveLogicOperationByWord(context, wordCheck, condition, res, bJustConsumeTheWords)){
			// all work already done at recursiveLogicOperationByWord(...)
		} else {
			context.seekToPosition(positionBeforeWord); //is a simple check, so restore the position
			bool comparisonDetected = false; //dummy required param
			res = compare(context, condition, comparisonDetected, bJustConsumeTheWords);
		}
		
		if(res == Failed) { 
			//not mixed with !condition to isolate what shall be done on script interpreting failure
			context.skipBlock();
			return Failed;
		}
		
		if(!condition) {
			context.skipBlock();
		}
		
		return Success;
	}
#ifdef ARX_DEBUG
#pragma GCC pop_options
#endif
	
	Result peek(Context & context) override { return execute(context); }
	
};

class ElseCommand : public Command {
	
public:
	
	ElseCommand() : Command("else") { }
	
	Result execute(Context & context) override {
		
		DebugScript("");
		
		context.skipBlock();
		
		return Success;
	}
	
	Result peek(Context & context) override { return execute(context); }
	
};

} // anonymous namespace

static std::string_view getName() {
	return "timer";
}

void timerCommand(std::string_view name, Context & context) {
	
	bool mili = false, idle = false;
	HandleFlags("mi") {
		mili = test_flag(flg, 'm');
		idle = test_flag(flg, 'i');
	}
	
	std::string command = context.getWord();
	
	if(command == "kill_local") {
		DebugScript(" kill_local");
		ARX_SCRIPT_Timer_Clear_All_Locals_For_IO(context.getEntity());
		return;
	}
	
	if(!name.empty()) {
		ARX_SCRIPT_Timer_Clear_By_Name_And_IO(name, context.getEntity());
	}
	if(command == "off") {
		if(name.empty()) {
			ScriptWarning << "Cannot turn off unamed timers";
		}
		DebugScript(name << " off");
		return;
	}
	
	long count = long(context.getFloatVar(command));
	float interval = context.getFloat();
	
	if(count < 0 || interval < 0.f) {
		ScriptError << "Timer count and interval must not be negative";
		return;
	}
	
	std::string timername = name.empty() ? getDefaultScriptTimerName(context.getEntity()) : std::string(name);
	DebugScript(timername << ' ' << options << ' ' << count << ' ' << interval);
	
	size_t pos = context.skipCommand();
	
	SCR_TIMER & timer = createScriptTimer(context.getEntity(), std::move(timername));
	timer.es = context.getScript();
	if(mili) {
		timer.interval = std::chrono::duration<float, std::milli>(interval);
	} else {
		timer.interval = std::chrono::duration<float>(interval);
	}
	timer.pos = pos;
	timer.start = g_gameTime.now();
	timer.count = count;
	timer.idle = idle;
	
}

void setupScriptedLang() {
	
	ScriptEvent::registerCommand(std::make_unique<NopCommand>()); // TODO(script-parser) remove
	ScriptEvent::registerCommand(std::make_unique<GotoCommand>("goto")); // TODO(script-parser) remove when possible
	ScriptEvent::registerCommand(std::make_unique<GotoCommand>("gosub", true));
	ScriptEvent::registerCommand(std::make_unique<AbortCommand>("accept", Command::AbortAccept));
	ScriptEvent::registerCommand(std::make_unique<AbortCommand>("refuse", Command::AbortRefuse));
	ScriptEvent::registerCommand(std::make_unique<RandomCommand>());
	ScriptEvent::registerCommand(std::make_unique<ReturnCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetMainEventCommand>("setstatus"));
	ScriptEvent::registerCommand(std::make_unique<SetMainEventCommand>("setmainevent"));
	ScriptEvent::registerCommand(std::make_unique<StartStopTimerCommand>("starttimer", true));
	ScriptEvent::registerCommand(std::make_unique<StartStopTimerCommand>("stoptimer", false));
	ScriptEvent::registerCommand(std::make_unique<SendEventCommand>());
	ScriptEvent::registerCommand(std::make_unique<SetEventCommand>());
	ScriptEvent::registerCommand(std::make_unique<IfCommand>());
	ScriptEvent::registerCommand(std::make_unique<ElseCommand>()); // TODO(script-parser) remove
	
}

} // namespace script
