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
#include <iostream> //del
#define MYDBG(x) std::cout << "___MySimpleDbg___: " << x << "\n" //del

#include "script/ScriptedVariable.h"

#include <boost/algorithm/string/predicate.hpp>

#include <cstring>
#include <string>
#include <string_view>

#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Item.h"
#include "game/Inventory.h"
#include "graphics/data/Mesh.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"
#include "util/Number.h"

namespace script {

namespace {

class SetCommand : public Command {
	
public:
	
	SetCommand() : Command("set") { }
	
	int getItemCountAtInventory(Entity * ent, std::string prefix) {
		int count = 0;
		if(ent && ent->inventory) {
			for(auto slot : ent->inventory->slots()) {
				if(slot.entity && boost::starts_with(slot.entity->idString(), prefix)) {
					count += slot.entity->_itemdata->count;
				}
			}
		}
		return count;
	}
	
	/**
	 * the array must contain words separated by a single space
	 */
	std::string getWordAtIndex(std::string array, long indexAsked) {
		long indexCurrent=0;
		size_t posWordStart = 0;
		size_t posWordEnd = 0;
		std::string word;
		while(true) {
			posWordEnd = array.find(' ', posWordStart);
			if(posWordEnd == std::string_view::npos) {
				posWordEnd = array.length();
			}
			
			if(posWordEnd == posWordStart) { //fail
				word = "";
				break;
			}
			
			word = array.substr(posWordStart, posWordEnd - posWordStart);
			if(indexCurrent == indexAsked) { //success
				break;
			}
			if(posWordEnd == array.length()) { //fail
				break;
			}
			
			posWordStart=posWordEnd;
			posWordStart++;
			
			indexCurrent++;
		}

		if(indexCurrent < indexAsked) { //array ended before reaching the requested index
			word = "";
		}
		
		return word;
	}
	
	/**
	 * usage examples:
	 * 
	 * Set <var> <val>
	 * Set -r <entReadFrom> <var> <val>
	 * Set -w <entWriteTo> <var> <val>
	 * Set -rw <entReadFrom> <entWriteTo> <var> <val>
	 * Set -wr <entWriteTo> <entReadFrom> <var> <val> //attention, w r order matters!
	 * <entReadFrom> entity to read 'val' from
	 * <entWriteTo> entity to write 'var' at
	 * 
	 ** Mode: Array:
	 * Set -a <var> <array> <index>
	 * <array> is a string that contains words separated by spaces ' '
	 * <index> array index that begins in 0
	 * Set -rwa <entReadFrom> <entWriteTo> <var> <array> <index>
	 * 
	 ** Mode: Item count at inventory:
	 * Set -i <var> <entityIdPrefix>
	 * Set -rwi <entReadFrom> <entWriteTo> <var> <entityIdPrefix>
	 */
	Result execute(Context & context) override {
		
		Entity * entReadFrom = context.getEntity();
		Entity * entWriteTo  = context.getEntity();
		std::string strEntityCheck;
		char mode = '.';
		
		HandleFlags("rwai") {
			if(flg & flag('r')) {
				strEntityCheck = context.getWord();
				entReadFrom = entities.getById(strEntityCheck);
			}
			if(flg & flag('w')) {
				strEntityCheck = context.getWord();
				entWriteTo = entities.getById(strEntityCheck);
			}
			if(flg & flag('a')) {
				mode = 'a';
			}
			if(flg & flag('i')) {
				mode = 'i';
			}
		}
		
		bool bFail=false;
		if(!entReadFrom) {
			ScriptWarning << "Invalid entity to read variable from " << strEntityCheck;
			bFail=true;
		}
		if(!entWriteTo) {
			ScriptWarning << "Invalid entity to write variable to " << strEntityCheck;
			bFail=true;
		}
		if(bFail) {
			context.skipWord(); //var
			if(mode != '.') {
				switch(mode) {
					// array mode
					case 'a': 
						context.skipWord(); //array
						context.skipWord(); //index
						break;
					// item count at inventory mode
					case 'i': context.skipWord(); break; //item prefix
					default: arx_assert_msg(false, "Invalid mode used in SetCommand: %c", mode); break;
				}
			} else {
				context.skipWord(); //val
			}
			return Failed;
		}
		
		std::string var = context.getWord();
		
		std::string val;
		if(mode != '.') {
			switch(mode) {
				case 'a': {// array mode
					std::string array = context.getWord();
					std::string indexVarName = context.getWord();
					long index = long(context.getFloatVar(indexVarName,entReadFrom));
					val = getWordAtIndex(array, index); 
				}; break;
				case 'i': { // item count at inventory mode
					std::string itemPrefix = context.getWord();
					val = getItemCountAtInventory(entReadFrom, itemPrefix); 
				}; break;
				default: arx_assert_msg(false, "Invalid mode used in SetCommand: %c", mode); break;
			}
		} else {
			val = context.getWord();
		}
		
		DebugScript(' ' << var << " \"" << val << '"');
		
		if(var.empty()) {
			ScriptWarning << "Missing variable name";
			return Failed;
		}
		
		SCRIPT_VARIABLES & variablesWriteTo = isLocalVariable(var) ? entWriteTo->m_variables : svar;
		
		SCRIPT_VAR * sv = nullptr;
		switch(var[0]) {
			
			case '$':      // global text
			case '\xA3': { // local text
				sv = SETVarValueText(variablesWriteTo, var, context.getStringVar(val,entReadFrom));
				break;
			}
			
			case '#':      // global long
			case '\xA7': { // local long
				sv = SETVarValueLong(variablesWriteTo, var, long(context.getFloatVar(val,entReadFrom)));
				break;
			}
			
			case '&':      // global float
			case '@': {    // local float
				sv = SETVarValueFloat(variablesWriteTo, var, context.getFloatVar(val,entReadFrom));
				break;
			}
			
			default: {
				ScriptWarning << "Unknown variable type: " << var;
				return Failed;
			}
			
		}
		
		if(!sv) {
			ScriptWarning << "Unable to set variable " << var;
			return Failed;
		}
		
		return Success;
	}
	
};

class ArithmeticCommand : public Command {
	
public:
	
	enum Operator {
		Add,
		Subtract,
		Multiply,
		Divide,
		Remainder,
		Power,
		NthRoot,
	};
	
private:
	
	float calculate(float left, float right) {
		switch(op) {
			case Add:        return left + right;
			case Subtract:   return left - right;
			case Multiply:   return left * right;
			case Divide:     return (right == 0.f) ? 0.f : left / right;
			case Remainder:  return (right == 0.f) ? 0.f : static_cast<int> (left) % static_cast<int> (right);
			case Power:      return static_cast<float> ( std::pow(left,right) );
			case NthRoot:
				if(left < 0.f) return -(static_cast<float> ( std::pow(-left,1.0f/right) )); // pow only works with positive left, this avoids being limited by sqtr/cbrt nesting
				return static_cast<float> ( std::pow(left,1.0f/right) );
		}
		arx_assert_msg(false, "Invalid op used in ArithmeticCommand: %d", int(op));
		return 0.f;
	}
	
	Operator op;
	
public:
	
	ArithmeticCommand(std::string_view name, Operator _op) : Command(name), op(_op) { }
	
	Result execute(Context & context) override {
		
		std::string var = context.getWord();
		float val = context.getFloat();
		
		DebugScript(' ' << var << ' ' << val);
		
		if(var.empty()) {
			ScriptWarning << "Missing variable name";
			return Failed;
		}
		
		SCRIPT_VARIABLES & variables = isLocalVariable(var) ? context.getEntity()->m_variables : svar;
		
		SCRIPT_VAR * sv = nullptr;
		switch(var[0]) {
			
			case '$':      // global text
			case '\xA3': { // local text
				ScriptWarning << "Cannot calculate with text variables";
				return Failed;
			}
			
			case '#':      // global long
			case '\xA7': { // local long
				long old = GETVarValueLong(variables, var);
				sv = SETVarValueLong(variables, var, long(calculate(float(old), val)));
				break;
			}
			
			case '&':   // global float
			case '@': { // local float
				float old = GETVarValueFloat(variables, var);
				sv = SETVarValueFloat(variables, var, calculate(old, val));
				break;
			}
			
			default: {
				ScriptWarning << "Unknown variable type: " << var;
				return Failed;
			}
			
		}
		
		if(!sv) {
			ScriptWarning << "Unable to set variable " << var;
			return Failed;
		}
		
		return Success;
	}
	
};

class UnsetCommand : public Command {
	
	// TODO move to variable context
	static void UNSETVar(SCRIPT_VARIABLES & svf, std::string_view name) {
		
		SCRIPT_VARIABLES::iterator it;
		for(it = svf.begin(); it != svf.end(); ++it) {
			if(it->name == name) {
				svf.erase(it);
				break;
			}
		}
		
	}
	
public:
	
	UnsetCommand() : Command("unset") { }
	
	Result execute(Context & context) override {
		
		std::string var = context.getWord();
		
		DebugScript(' ' << var);
		
		if(var.empty()) {
			ScriptWarning << "missing variable name";
			return Failed;
		}
		
		SCRIPT_VARIABLES & variables = isLocalVariable(var) ? context.getEntity()->m_variables : svar;
		
		UNSETVar(variables, var);
		
		return Success;
	}
	
};

class IncrementCommand : public Command {
	
	long m_diff;
	
public:
	
	IncrementCommand(std::string_view name, long diff) : Command(name), m_diff(diff) { }
	
	Result execute(Context & context) override {
		
		std::string var = context.getWord();
		
		DebugScript(' ' << var);
		
		if(var.empty()) {
			ScriptWarning << "missing variable name";
			return Failed;
		}
		
		SCRIPT_VARIABLES & variables = isLocalVariable(var) ? context.getEntity()->m_variables : svar;
		
		SCRIPT_VAR * sv = nullptr;
		switch(var[0]) {
			
			case '$':
			case '\xA3': {
				ScriptWarning << "Cannot increment text variables";
				return Failed;
			}
			
			case '#':
			case '\xA7': {
				sv = SETVarValueLong(variables, var, GETVarValueLong(variables, var) + m_diff);
				break;
			}
			
			case '&':
			case '@': {
				sv = SETVarValueFloat(variables, var, GETVarValueFloat(variables, var) + float(m_diff));
				break;
			}
			
			default: {
				ScriptWarning << "Unknown variable type: " << var;
				return Failed;
			}
			
		}
		
		if(!sv) {
			ScriptWarning << "Unable to set variable " << var;
			return Failed;
		}
		
		return Success;
	}
	
};

} // anonymous namespace

void setupScriptedVariable() {
	
	ScriptEvent::registerCommand(std::make_unique<SetCommand>());
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("inc", ArithmeticCommand::Add));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("add", ArithmeticCommand::Add));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("dec", ArithmeticCommand::Subtract));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("sub", ArithmeticCommand::Subtract));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("mul", ArithmeticCommand::Multiply));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("div", ArithmeticCommand::Divide));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("mod", ArithmeticCommand::Remainder));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("pow", ArithmeticCommand::Power));
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("nthroot", ArithmeticCommand::NthRoot));
	ScriptEvent::registerCommand(std::make_unique<UnsetCommand>());
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("++", 1));
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("--", -1));
	
}

} // namespace script
