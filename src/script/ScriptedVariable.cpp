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

#include "script/ScriptedVariable.h"

#include <boost/algorithm/string/predicate.hpp>

#include <cstring>
#include <regex>
#include <string>
#include <string_view>

#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Item.h"
#include "game/Inventory.h"
#include "graphics/data/Mesh.h"
#include "platform/Environment.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"
#include "util/Number.h"

namespace script {

namespace {

class SetCommand : public Command {
	
public:
	
	SetCommand() : Command("set") { }
	
	std::string getItemListAtInventory(Entity * ent, std::string prefix, bool getCountToo=false) {
		std::string list;
		if(ent && ent->inventory) {
			for(auto slot : ent->inventory->slots()) {
				if(slot.entity && (boost::starts_with(slot.entity->idString(), prefix) || prefix == "*")) {
					if(list != "") {
						list += " ";
					}
					
					list += slot.entity->idString();
					
					if(getCountToo) {
						list += " " + slot.entity->_itemdata->count;
					}
				}
			}
		}
		return list;
	}
	
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
			
			if(posWordEnd == posWordStart) { // fail
				word = "";
				break;
			}
			
			word = array.substr(posWordStart, posWordEnd - posWordStart);
			if(indexCurrent == indexAsked) { // success
				break;
			}
			if(posWordEnd == array.length()) { // fail
				break;
			}
			
			posWordStart=posWordEnd;
			posWordStart++;
			
			indexCurrent++;
		}

		if(indexCurrent < indexAsked) { // array ended before reaching the requested index
			word = "";
		}
		
		return word;
	}
	
	/**
	 * Set [-rw] <w?entWriteTo> <r?entReadFrom> <var> <val>
	 * 		<entReadFrom> entity to read 'val' from
	 * 		<entWriteTo> entity to write 'var' at
	 * 
	 * The Modes below are exclusive. Use only one.
	 * 
	 ** <-v> Mode: Array of words: assigns to var the array entry at index
	 * Set -v[rw] <w?entWriteTo> <r?entReadFrom> <var> <a?index> <a?array...> ;
	 * 		<index> array index that begins in 0.
	 * 		<array...> are words terminated with ';' word
	 * 		; is required to know the list ended
	 * 		returns "void" meaning index out of bounds
	 * 
	 ** <-a> Mode: Array concatenated in a string: assigns to var the array entry at index
	 * Set -a[rw] <w?entWriteTo> <r?entReadFrom> <var> <a?index> <a?array>
	 * 		<index> array index that begins in 0
	 * 		<array> is a string that contains words separated by spaces ' '
	 * 
	 ** <-x> Mode: Replaces a string in a string var matching a regex
	 * Set -x[rw] <w?entWriteTo> <r?entReadFrom> <var> <x?regex> <x?replaceWith>
	 * 
	 * Usage examples:
	 * Set <var> <val>
	 * Set -r <entReadFrom> <var> <val>
	 * Set -w <entWriteTo> <var> <val>
	 * Set -rw <entWriteTo> <entReadFrom> <var> <val> //with both rw, first w then r, matching var val order
	 * Set -a <var> <index> <arrayString> 
	 * Set -v <var> <index> <array...> ;
	 * Set -rwv <entWriteTo> <entReadFrom> <var> <index> <array...> ;
	 */
	Result execute(Context & context) override {
		
		Entity * entReadFrom = context.getEntity();
		Entity * entWriteTo  = context.getEntity();
		std::string strEntityCheck;
		char mode = '.';
		bool bReadFrom=false;
		bool bWriteTo=false;
		
		HandleFlags("rwavx") {
			if(flg & flag('r')) {
				bReadFrom=true;
			}
			if(flg & flag('w')) {
				bWriteTo=true;
			}
			if(flg & flag('a')) {
				mode = 'a';
			}
			if(flg & flag('v')) {
				mode = 'v';
			}
			if(flg & flag('x')) {
				mode = 'x';
			}
		}
		
		//keep this order: WriteTo ReadFrom, to match this order: var val
		bool bFail=false;
		if(bWriteTo) {
			strEntityCheck = context.getStringVar(context.getWord());
			entWriteTo = entities.getById(strEntityCheck);
			if(!entWriteTo) {
				ScriptWarning << "Invalid entity to write variable to " << strEntityCheck;
				bFail=true;
			}
		}
		if(bReadFrom) {
			strEntityCheck = context.getStringVar(context.getWord());
			entReadFrom = entities.getById(strEntityCheck);
			if(!entReadFrom) {
				ScriptWarning << "Invalid entity to read variable from " << strEntityCheck;
				bFail=true;
			}
		}
		
		if(bFail) { // discards following words coherently
			context.skipWord(); // var
			if(mode != '.') {
				switch(mode) {
					// array mode
					case 'a': 
						context.skipWord(); // index
						context.skipWord(); // array
						break;
					case 'v': 
						context.skipWord(); // index
						while(context.getWord() != ";"); // array... and terminator ;
						break;
					case 'x': 
						context.skipWord(); // regex
						context.skipWord(); // replaceWith
						break;
					default: arx_assert_msg(false, "Invalid mode used in SetCommand: %c", mode); break;
				}
			} else {
				context.skipWord(); // val
			}
			return Failed;
		}
		
		std::string var = context.autoVarNameForScope(true, context.getWord());
		
		std::string val;
		switch(mode) {
			case '.': { // simple value mode
				val = context.getWord();
			}; break;
			
			case 'a': { // array in a string mode
				long index = long(context.getFloatVar(context.getWord(), entReadFrom));
				std::string array = context.getStringVar(context.getWord(), entReadFrom);
				val = getWordAtIndex(array, index); 
			}; break;
			
			case 'v': { // array of words mode
				long index = long(context.getFloatVar(context.getWord(), entReadFrom));
				std::string word = "void"; // means index out of bounds, index not set, index doesnt exist, like a var that doesnt exist at getStringVar()
				long count = 0;
				while(true) {
					word = context.getStringVar(context.getWord(), entReadFrom);
					if(word == ";") break; // must continue til the end of the list
					if(count == index) {
						val = word;
					}
					count++;
				}
			}; break;
			
			case 'x': { // replace mode
				std::string strRegexMatch = context.getStringVar(context.getWord(), entReadFrom);
				std::string strReplace = context.getStringVar(context.getWord(), entReadFrom);
				val = context.getStringVar(var, entReadFrom);
				
				DebugScript(' ' << strRegexMatch << ' ' << strReplace << ' ' << val);
				
				std::regex reRegexMatch(strRegexMatch.c_str(), std::regex_constants::ECMAScript | std::regex_constants::icase);
				val = std::regex_replace(val, reRegexMatch, strReplace.c_str());
			}; break;
			
			default: arx_assert_msg(false, "Invalid mode used in SetCommand: %c", mode); break;
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
				sv = SETVarValueText(variablesWriteTo, var, context.getStringVar(val, entReadFrom));
				break;
			}
			
			case '#':      // global long
			case '\xA7': { // local long
				sv = SETVarValueLong(variablesWriteTo, var, long(context.getFloatVar(val, entReadFrom)));
				break;
			}
			
			case '&':      // global float
			case '@': {    // local float
				sv = SETVarValueFloat(variablesWriteTo, var, context.getFloatVar(val, entReadFrom));
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

class EnvironmentCommand : public Command {
	
public:
	
	EnvironmentCommand() : Command("env") { }
	
	/**
	 * This is intended to tweak env vars in memory to avoid having to restart the game.
	 * This is not intended to set permanent env vars on the system nor to prepare the environment for sub proccesses (but could be).
	 * This is UNSAFE! this means that the checks performed during normal env var reading will not be performed again, so be careful.
	 * This is intended for careful mod developers and source code developers.
	 * env -l //list all in console log
	 * env -s <envVarId> <value> //set EnvVar to <value>
	 * env -g <envVarId> <scriptVariable> //get EnvVar value into <scriptVariable>
	 */
	Result execute(Context & context) override {
		bool bSet = false;
		bool bGet = false;
		
		HandleFlags("lsg") {
			if(flg & flag('l')) {
				platform::getEnvVarList();
				return Success;
			}
			if(flg & flag('s')) {
				bSet = true;
			}
			if(flg & flag('g')) {
				bGet = true;
			}
		}
		
		std::string envVar = context.getStringVar(context.getWord());
		
		if(bSet) {
			std::string val = context.getStringVar(context.getWord());
			//platform::EnvVar ev = platform::getEnvVar(envVar);
			platform::getEnvVar(envVar)->setValAuto(val, true);
			//if(ev.isString()) {
				//platform::getEnvVar(envVar)->setVal(val, true);
			//} else
			//if(boost::contains(val, ".")) {
				//platform::getEnvVar(envVar)->setVal(util::parseFloat(val), true);
			//} else {
				//platform::getEnvVar(envVar)->setVal(util::parseInt(val), true);
			//}
			
			return Success;
		}
		
		if(bGet) {
			std::string val = platform::getEnvVar(envVar)->getString();
			
			Entity * entWriteTo = context.getEntity();
			Entity * entReadFrom = context.getEntity();
			
			std::string var = context.autoVarNameForScope(true, context.getWord());
			
			SCRIPT_VARIABLES & variablesWriteTo = isLocalVariable(var) ? entWriteTo->m_variables : svar;
			
			SCRIPT_VAR * sv = nullptr;
			switch(var[0]) {
				case '$':      // global text
				case '\xA3': { // local text
					sv = SETVarValueText(variablesWriteTo, var, context.getStringVar(val, entReadFrom));
					break;
				}
				
				case '#':      // global long
				case '\xA7': { // local long
					sv = SETVarValueLong(variablesWriteTo, var, long(context.getFloatVar(val, entReadFrom)));
					break;
				}
				
				case '&':      // global float
				case '@': {    // local float
					sv = SETVarValueFloat(variablesWriteTo, var, context.getFloatVar(val, entReadFrom));
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
		
		return Failed;
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
		Calc,
	};
	
private:
	
	/**
	 * if a var is expanded like ~@test1~ (at getWord()), it will NOT be read from entReadFrom, but from current/self entity
	 */
	float calc(Context & context, Entity * entReadFrom) {
		float fCalc = 0.f;
		
		std::string strCalcMsg;
		std::string strWord = context.getWord();
		char cOperator = '.'; // init to invalid
		float fWorkWithValue = 0.f;
		size_t positionBeforeWord;
		
		if(strWord != "[") {
			ScriptWarning << "Malformed calculation: calc must start with '[' " << strCalcMsg;
			return 99999999999.f;
		}
		
		int iWordCount = 0;
		char cMode = 'v';
		while(true) {
			context.skipWhitespaceAndComment();
			positionBeforeWord = context.getPosition(); //Put after skip new lines.
			strWord = context.getWord();
			strCalcMsg += strWord + " ";
			
			switch(cMode) {
				case 'v': // value
					if(strWord == "[") { // calc value from nested
						context.seekToPosition(positionBeforeWord);
						fWorkWithValue = calc(context, entReadFrom);
					} else {
						fWorkWithValue = context.getFloatVar(strWord,entReadFrom);
					}
					
					if(iWordCount == 0) {
						fCalc = fWorkWithValue;
					} else {
						switch(cOperator) { // the previous word was operator
							case '+': fCalc = calculate(fCalc, fWorkWithValue, ArithmeticCommand::Add,       context, entReadFrom); break;
							case '-': fCalc = calculate(fCalc, fWorkWithValue, ArithmeticCommand::Subtract,  context, entReadFrom); break;
							case '*': fCalc = calculate(fCalc, fWorkWithValue, ArithmeticCommand::Multiply,  context, entReadFrom); break;
							case '/': fCalc = calculate(fCalc, fWorkWithValue, ArithmeticCommand::Divide,    context, entReadFrom); break;
							case '%': fCalc = calculate(fCalc, fWorkWithValue, ArithmeticCommand::Remainder, context, entReadFrom); break;
							case '^':
								fCalc = calculate(
									fCalc,
									fWorkWithValue >= 1.0f ? fWorkWithValue : 1.0f/fWorkWithValue,
									fWorkWithValue >= 1.0f ? ArithmeticCommand::Power : ArithmeticCommand::NthRoot,
									context,
									entReadFrom);
								break;
							default:
								ScriptWarning << "Unexpected calculation operator '" << cOperator << "' at " << strCalcMsg; // TODO use arx_assert_msg as below `case 'o'` should grant this?
								return 99999999999.f;
						}
						
						cOperator = '.'; // reset to invalid
					}
					
					cMode = 'o';
					break;
				
				case 'o': // operation
					if(strWord == "]") {
						return fCalc;
					}
					
					switch(strWord[0]) {
						case '+': case '-': case '*': case '/': case '%': case '^':
							cOperator = strWord[0];
							break;
							
						default:
							ScriptWarning << "Invalid calculation operator '" << strWord << "' at " << strCalcMsg;
							return 99999999999.f;
					}
					
					cMode = 'v';
					break;
			}
			
			iWordCount++;
		}
		
		return fCalc;
	}
	
	float calculate(float left, float right, Operator opOverride, Context & context, Entity * entReadFrom) {
		switch(opOverride) {
			case Add:        return left + right;
			case Subtract:   return left - right;
			case Multiply:   return left * right;
			case Divide:     return (right == 0.f) ? 0.f : left / right;
			case Remainder:  return (right == 0.f) ? 0.f : std::fmod(left, right);
			case Power:      return static_cast<float> ( std::pow(left,right) );
			case NthRoot:
				// pow only works with positive left, this avoids being limited by sqtr/cbrt nesting
				if(left < 0.f) return -( static_cast<float> (std::pow(-left, 1.0f/right)) );
				return static_cast<float> ( std::pow(left, 1.0f/right) );
			case Calc:       return calc(context, entReadFrom);
		}
		arx_assert_msg(false, "Invalid op used in ArithmeticCommand: %d", int(op));
		return 0.f;
	}
	
	Operator op;
	
public:
	
	ArithmeticCommand(std::string_view name, Operator _op) : Command(name), op(_op) { }
	
	Result execute(Context & context) override {
		
		std::string var = context.autoVarNameForScope(true, context.getWord());
		float val = op == ArithmeticCommand::Calc ? 0.f : context.getFloatVar(context.getWord(), context.getEntity());
		
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
				sv = SETVarValueLong(variables, var, long(calculate(float(old), val, op, context, context.getEntity())));
				break;
			}
			
			case '&':   // global float
			case '@': { // local float
				float old = GETVarValueFloat(variables, var);
				sv = SETVarValueFloat(variables, var, calculate(old, val, op, context, context.getEntity()));
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
		
		std::string var = context.autoVarNameForScope(true, context.getWord());
		
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
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("calc", ArithmeticCommand::Calc));
	ScriptEvent::registerCommand(std::make_unique<UnsetCommand>());
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("++", 1));
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("--", -1));
	ScriptEvent::registerCommand(std::make_unique<EnvironmentCommand>());
	
}

} // namespace script
