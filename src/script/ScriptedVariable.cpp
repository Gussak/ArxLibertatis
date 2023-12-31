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
	
	Result execute(Context & context) override {
		
		std::string var = context.getWord();
		std::string val = context.getWord();
		
		DebugScript(' ' << var << " \"" << val << '"');
		
		if(var.empty()) {
			ScriptWarning << "Missing variable name";
			return Failed;
		}
		
		SCRIPT_VARIABLES & variables = isLocalVariable(var) ? context.getEntity()->m_variables : svar;
		
		SCRIPT_VAR * sv = nullptr;
		switch(var[0]) {
			
			case '$':      // global text
			case '\xA3': { // local text
				sv = SETVarValueText(variables, var, context.getStringVar(val));
				break;
			}
			
			case '#':      // global long
			case '\xA7': { // local long
				sv = SETVarValueLong(variables, var, long(context.getFloatVar(val)));
				break;
			}
			
			case '&':      // global float
			case '@': {    // local float
				sv = SETVarValueFloat(variables, var, context.getFloatVar(val));
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
		
		std::string var = context.getWord();
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
	ScriptEvent::registerCommand(std::make_unique<ArithmeticCommand>("calc", ArithmeticCommand::Calc));
	ScriptEvent::registerCommand(std::make_unique<UnsetCommand>());
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("++", 1));
	ScriptEvent::registerCommand(std::make_unique<IncrementCommand>("--", -1));
	
}

} // namespace script
