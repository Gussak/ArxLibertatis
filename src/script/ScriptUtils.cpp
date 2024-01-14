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

#include "script/ScriptUtils.h"

#include <set>
#include <utility>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "game/Entity.h"
#include "graphics/data/Mesh.h"
#include "platform/Dialog.h"
#include "platform/Process.h"
#include "util/Number.h"
#include "util/String.h"


namespace script {

static bool isWhitespace(char c) {
	return (static_cast<unsigned char>(c) <= 32 || c == '(' || c == ')');
}

std::string_view toLocalizationKey(std::string_view string) {
	
	// if the section name has the qualifying brackets "[]", cut them off
	if(!string.empty() && string.front() == '[' && string.back() == ']') {
		string.remove_prefix(1);
		string.remove_suffix(1);
	}
	
	return string;
}

Context::Context(const EERIE_SCRIPT * script, size_t pos, Entity * sender, Entity * entity,
                 ScriptMessage msg, ScriptParameters parameters)
	: m_script(script)
	, m_pos(pos)
	, m_sender(sender)
	, m_entity(entity)
	, m_message(msg)
	, m_parameters(std::move(parameters))
{
	updateNewLinesList();
}

void Context::updateNewLinesList() {
	size_t posNL = 0;
	while(true) {
		posNL = m_script->data.find('\n', posNL);
		if(posNL == std::string::npos) {
			break;
		}else{
			m_vNewLineAt.push_back(posNL);
			posNL++;
		}
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
std::string Context::formatString(std::string format, float var) const {
	std::string strTmp(256, '\0');
	auto written = std::snprintf(&strTmp[0], strTmp.size(), format.c_str(), static_cast<double>(var) );
	strTmp.resize(size_t(written));
	return strTmp;
}
std::string Context::formatString(std::string format, long var) const {
	std::string strTmp(256, '\0');
	auto written = std::snprintf(&strTmp[0], strTmp.size(), format.c_str(), var);
	strTmp.resize(size_t(written));
	return strTmp;
}
std::string Context::formatString(std::string format, std::string var) const {
	std::string strTmp(256, '\0');
	auto written = std::snprintf(&strTmp[0], strTmp.size(), format.c_str(), var.c_str());
	strTmp.resize(size_t(written));
	return strTmp;
}
#pragma GCC diagnostic pop

std::string Context::autoVarNameForScope(bool privateScopeOnly, std::string_view name, std::string labelOverride) const {
	if(!isLocalVariable(name)) {
		return name;
	}
	
	if(privateScopeOnly) { // only if private scope is requested on the var name thru the special char
		if(name[1] != '\xB7') { // bullet char
			return name;
		}
	}
	
	std::string label;
	if(labelOverride.size() > 0){
		label = labelOverride;
	} else {
		if(m_stackIdCalledFromPos.size() == 0) { //TODO use event name instead
			return name;
		}
		label = m_stackIdCalledFromPos[m_stackIdCalledFromPos.size()-1].second;
	}
	if(label.size() == 0) {
		return name;
	}
	
	char cSeparator = '_'; // local scope
	size_t posID = 1;
	if(name[1] == '\xB7') { // bullet char
		cSeparator = '\xB7'; // private scope
		posID = 2;
	}
	
	if(!boost::starts_with(name.substr(1), label)) { // only prefix with label if not already
		name = std::string() + name[0] + label + cSeparator + name.substr(posID);
	}
	
	return name;
}

std::string Context::getStringVar(std::string_view name, Entity * entOverride) const {
	
	if(name.empty()) {
		return std::string();
	}
	
	std::string format;
	if(name[0] == '%') { // printf format syntax. ex.: this happens when using var expansion like ~%.2f,@var~
		format = name.substr( 0, name.find(',', 0) );
		name   = name.substr( format.size() + 1 ); // +1 skips the ','
	}
	
	name = autoVarNameForScope(true, name);
	
	if(name[0] == '^') {
		long lv;
		float fv;
		std::string tv;
		switch(getSystemVar(*this, name, tv, &fv, &lv)) {
			case TYPE_TEXT: return format.size() > 0 ? formatString(format, tv) : tv;
			case TYPE_LONG: return format.size() > 0 ? formatString(format, lv) : std::to_string(lv);
			default: return format.size() > 0 ? formatString(format, fv) : std::to_string(fv);
		}
	} else if(name[0] == '#') {
		long lv = GETVarValueLong(svar, name);
		return format.size() > 0 ? formatString(format, lv) : std::to_string(lv);
	} else if(name[0] == '\xA7') {
		long lv = GETVarValueLong((entOverride ? entOverride : getEntity())->m_variables, name);
		return format.size() > 0 ? formatString(format, lv) : std::to_string(lv);
	} else if(name[0] == '&') {
		float fv = GETVarValueFloat(svar, name);
		return format.size() > 0 ? formatString(format, fv) : boost::lexical_cast<std::string>(fv);
	} else if(name[0] == '@') {
		float fv = GETVarValueFloat((entOverride ? entOverride : getEntity())->m_variables, name);
		return format.size() > 0 ? formatString(format, fv) : boost::lexical_cast<std::string>(fv);
	} else if(name[0] == '$') {
		const SCRIPT_VAR * var = GetVarAddress(svar, name);
		return var ? (format.size() > 0 ? formatString(format, var->text) : var->text) : "void";
	} else if(name[0] == '\xA3') {
		const SCRIPT_VAR * var = GetVarAddress((entOverride ? entOverride : getEntity())->m_variables, name);
		return var ? (format.size() > 0 ? formatString(format, var->text) : var->text) : "void";
	}
	
	return std::string(name);
}

#define ScriptParserWarning ARX_LOG(isSuppressed(*this, "?") ? Logger::Debug : Logger::Warning) << ScriptContextPrefix(*this) << ": "

bool detectAndSkipComment(const std::string_view & esdat, size_t & pos, bool skipNewlines) {
	if(esdat[pos] == '/' && pos + 1 != esdat.size() && esdat[pos + 1] == '/') {
		pos = esdat.find('\n', pos + 2);
		if(pos == std::string::npos) {
			pos = esdat.size();
		} else {
			if(skipNewlines) {
				pos++; //after \n
			}
		}
		return true;
	}
	return false;
}

void Context::skipWhitespaceAndComment() { // TODO refactor to skipWhitespacesCommentsAndNewLines
	skipWhitespace(true);
	script::detectAndSkipComment(m_script->data, m_pos, true);
	skipWhitespace(true);
}

std::string Context::getCommand(bool skipNewlines) {
	
	std::string_view esdat = m_script->data;
	
	skipWhitespace(skipNewlines);
	
	std::string word;
	
	// now take chars until it finds a space or unused char
	for(; m_pos != esdat.size() && !isWhitespace(esdat[m_pos]); m_pos++) {
		
		char c = esdat[m_pos];
		if(c == '"') {
			ScriptParserWarning << "unexpected '\"' in command name";
		} else if(c == '~') {
			ScriptParserWarning << "unexpected '~' in command name";
		} else if(c == '\n') {
			break;
		} else if(script::detectAndSkipComment(esdat, m_pos, false)) {
			if(!word.empty()) {
				break;
			}
			skipWhitespace(skipNewlines), m_pos--;
		} else {
			word.push_back(c);
		}
	}
	
	return word;
}

std::string Context::getPositionAndLineNumber(bool compact, size_t pos) const {
	std::stringstream s;
	
	if(pos == static_cast<size_t>(-1)) {
		pos = m_pos;
	}
	
	s << "(" << (compact ? "p=" : "Position ") << pos;
	
	size_t iLine, iColumn;
	getLineColumn(iLine, iColumn, pos);
	
	s << (compact ? ",l=" : ", Line ") << iLine << (compact ? ",c=" : ", Column ") << iColumn << ")";
	return s.str();
}

void Context::getLineColumn(size_t & iLine, size_t & iColumn, size_t pos) const {
	if(pos == static_cast<size_t>(-1)) {
		pos = m_pos;
	}
	
	iLine = 0;
	iColumn = 1;
	for(size_t i = 0; i < m_vNewLineAt.size(); i++) {
		if(pos > m_vNewLineAt[i]) {
			iLine = i + 1;
			iColumn = pos - m_vNewLineAt[i];
			iLine++;
			iColumn--;
		} else {
			break;
		}
	}
}

size_t Context::getGoSubCallFromPos(size_t indexFromLast) const {
	if(m_stackIdCalledFromPos.size() == 0) {
		return static_cast<size_t>(-1); // means invalid
	}
	
	if(indexFromLast >= m_stackIdCalledFromPos.size()) {
		indexFromLast = m_stackIdCalledFromPos.size() - 1;
	}
	
	return m_stackIdCalledFromPos[m_stackIdCalledFromPos.size() - indexFromLast - 1].first;
}

std::string Context::getGoSubCallStack(std::string_view prepend, std::string_view append, std::string_view between, size_t indexFromLast) const {
	std::stringstream ss;
	
	if(m_stackIdCalledFromPos.size() > 0) {
		ss << prepend;
		
		size_t indexHighlight = size_t(-1);
		if(indexFromLast != size_t(-1)) {
			if(indexFromLast >= m_stackIdCalledFromPos.size()) {
				indexHighlight = 0;
			} else {
				indexHighlight = m_stackIdCalledFromPos.size() - indexFromLast - 1;
			}
		}
		
		size_t index = 0;
		for(auto pair : m_stackIdCalledFromPos) {
			if(index >= 1) ss << between;
			if(indexHighlight == index) ss << "!!!";
			ss << pair.second;
			if(indexHighlight == index) ss << "!!!";
			ss << getPositionAndLineNumber(true, m_stackIdCalledFromPos[index].first);
			index++;
		}
		
		ss << append;
	}
	
	return ss.str();
}

void Context::seekToPosition(size_t pos) { 
	m_pos=pos; 
}

/**
 * //TODO sketch studing script pre-compilation
 * it is like a pre-compiled script
 * TODO clean all comments
 * TODO convert all \n to ' '
 */
bool Context::writePreCompiledData(std::string & esdat, size_t pos, unsigned char cCmd, unsigned char cSkipCharsCount) { //std::string word, PreCompileReference & ref) { //std::string reference) {
	static int allowPreCompilation = [](){const char * pc = std::getenv("ARX_AllowScriptPreCompilation"); if(pc) { LogWarning << "[ARX_AllowScriptPreCompilation] = \"" << pc << "\""; return util::parseInt(pc);} return 0;}();  // warns only once. set ARX_AllowScriptPreCompilation=1
	if(!allowPreCompilation) return false;
	
	//arx_assert_msg(!(word.size() < reference.size()),"pre-compiled reference=%s needs to be at most word=%s size", reference.c_str(), word.c_str());
	//m_script->data[pos] = PreCompiled::REFERENCE;
	//m_script->data[pos+1] = c;
	//m_script->data[pos+1] = ref;
	//m_script->data[pos+2] = word.size();
	esdat[pos] = script::PreCompiled::REFERENCE;
	//pos++;arx_assert_msg(pos<esdat.size(),"beyond esdat size for cCmd=%d",cCmd);
	pos++;arx_assert_msg(pos<esdat.size(),"%lu should be < esdat size=%lu for cCmd=%d",pos,esdat.size(),cCmd);
	esdat[pos] = cCmd;
	//pos++;arx_assert_msg(pos<esdat.size(),"beyond esdat size for cSkipCharsCount=%d",cSkipCharsCount);
	pos++;arx_assert_msg(pos<esdat.size(),"%lu should be < esdat size=%lu for cSkipCharsCount=%d",pos,esdat.size(),cSkipCharsCount);
	esdat[pos] = cSkipCharsCount;
	
	return true;
}

std::string Context::getWord(bool evaluateVars) {
	
	std::string_view esdat = m_script->data;
	
	skipWhitespace(false, true);
	
	if(m_pos >= esdat.size()) {
		return std::string();
	}
	
	bool tilde = false; // number of tildes
	
	std::string word;
	std::string var;
	
	if(esdat[m_pos] == '"') {
		
		for(m_pos++; m_pos != esdat.size() && esdat[m_pos] != '"'; m_pos++) {
			if(esdat[m_pos] == '\n') {
				if(tilde) {
					ScriptParserWarning << "unmatched '\"' before end of line";
				}
				return word;
			} else if(esdat[m_pos] == '~') {
				if(tilde) {
					if(evaluateVars) {
						word += getStringVar(var);
						var.clear();
					}
				}
				tilde = !tilde;
			} else if(tilde) {
				if(evaluateVars) var.push_back(esdat[m_pos]);
			} else {
				word.push_back(esdat[m_pos]);
			}
		}
		
		if(m_pos != esdat.size()) {
			m_pos++;
		} else {
			ScriptParserWarning << "unmatched '\"'";
		}
		
	} else {
		
		// now take chars until it finds a space or unused char
		for(; m_pos != esdat.size() && !isWhitespace(esdat[m_pos]); m_pos++) {
			
			if(esdat[m_pos] == '"') {
				int i;
				ScriptParserWarning << "unexpected '\"' inside token";
				i++;
				i++;
			} else if(esdat[m_pos] == '~') {
				if(tilde) {
					if(evaluateVars) {
						word += getStringVar(var);
						var.clear();
					}
				}
				tilde = !tilde;
			} else if(tilde) {
				if(evaluateVars) var.push_back(esdat[m_pos]);
			} else if(script::detectAndSkipComment(esdat, m_pos, false)) {
				break;
			} else {
				word.push_back(esdat[m_pos]);
			}
		}
		
	}
	
	if(tilde) {
		ScriptParserWarning << "unmatched '~'";
	}
	
	return word;
}

void Context::skipWord() {
	
	std::string_view esdat = m_script->data;
	
	skipWhitespace(false, true);
	
	if(m_pos != esdat.size() && esdat[m_pos] == '"') {
		
		for(m_pos++; m_pos != esdat.size() && esdat[m_pos] != '"'; m_pos++) {
			if(esdat[m_pos] == '\n') {
				ScriptParserWarning << "missing '\"' before end of line";
				return;
			}
		}
		
		if(m_pos != esdat.size()) {
			m_pos++;
		} else {
			ScriptParserWarning << "unmatched '\"'";
		}
		
	} else {
		
		// now take chars until it finds a space or unused char
		for(; m_pos != esdat.size() && !isWhitespace(esdat[m_pos]); m_pos++) {
			if(esdat[m_pos] == '"') {
				ScriptParserWarning << "unexpected '\"' inside token";
			} else if(script::detectAndSkipComment(esdat, m_pos, false)) {
				break;
			}
		}
		
	}
}

void Context::skipWhitespace(bool skipNewlines, bool warnNewlines) {
	
	std::string_view esdat = m_script->data;
	
	// First ignores spaces & unused chars
	for(; m_pos != esdat.size() && isWhitespace(esdat[m_pos]); m_pos++) {
		if(esdat[m_pos] == '\n') {
			if(warnNewlines) {
				ScriptParserWarning << "unexpected newline";
				if(isBlockEndSuprressed(*this, "?")) {
					// Ignore the newline
					continue;
				}
			}
			if(!skipNewlines) {
				return;
			}
		}
	}
}

std::string Context::getFlags() {
	
	skipWhitespace(false, true);
	
	if(m_pos < m_script->data.size() && m_script->data[m_pos] == '-') {
		return getWord();
	}
	
	return std::string();
}

float Context::getFloat() {
	return getFloatVar(getWord());
}

bool Context::getBool() {
	
	std::string word = getWord();
	
	return (word == "on" || word == "yes" || word == "enable" || word == "true");
}

float Context::getFloatVar(std::string_view name, Entity * entOverride) const {
	
	if(name.empty()) {
		return 0.f;
	} else if(name[0] == '^') {
		long lv;
		float fv;
		std::string tv;
		switch(getSystemVar(*this, name, tv, &fv, &lv)) {
			case TYPE_TEXT:
				return util::parseFloat(tv);
			case TYPE_LONG:
				return float(lv);
			default:
				return fv;
		}
	} else if(name[0] == '#') {
		return float(GETVarValueLong(svar, name));
	} else if(name[0] == '\xA7') {
		return float(GETVarValueLong((entOverride ? entOverride : getEntity())->m_variables, autoVarNameForScope(true, name)));
	} else if(name[0] == '&') {
		return GETVarValueFloat(svar, name);
	} else if(name[0] == '@') {
		return GETVarValueFloat((entOverride ? entOverride : getEntity())->m_variables, autoVarNameForScope(true, name));
	}
	
	return util::parseFloat(name);
}

size_t Context::skipCommand() {
	
	skipWhitespace();
	
	std::string_view esdat = m_script->data;
	
	if(m_pos == esdat.size() || esdat[m_pos] == '\n') {
		return size_t(-1);
	}
	
	size_t oldpos = m_pos;
	
	if(script::detectAndSkipComment(esdat, m_pos, false)) {
		oldpos = size_t(-1);
	} else { // skips to the end of the line even if it is not commented
		m_pos = esdat.find('\n', m_pos);
		if(m_pos == std::string::npos) {
			m_pos = esdat.size();
		}
	}
	
	return oldpos;
}

size_t seekBackwardsForCommentToken(const std::string_view & esdat, const size_t posToBackTrackFrom) {
	for(size_t p = posToBackTrackFrom;; p--) {
		if(esdat[p] == '/' && (p + 1 != esdat.size()) && esdat[p + 1] == '/') {
			return p;
		}
		//if(esdat[p] == '/' && esdat[p + 1] == '*') { // multiline comments are dynamically pre-applied to become single line comments now, therefore they do not exist
			//return p;
		//}
		if(esdat[p] == '\n' || p == 0) {
			break;
		}
	}
	return size_t(-1); // is not commented, therefore is a valid code line
}

bool askOkCancelCustomUserSystemPopupCommand(const std::string strTitle, const std::string strCustomMessage, const std::string strDetails, const std::string strFileToEdit, const std::string strScriptStringVariableID, const Context * context, size_t callStackIndexFromLast) {
	std::stringstream ssPopupMsg; ssPopupMsg << strCustomMessage << "\n";
	
	std::stringstream ssWarn;
	std::stringstream ssError;
	if(boost::starts_with(util::toLowercase(strCustomMessage), "warn:" )) ssWarn  << strCustomMessage.substr(5);
	if(boost::starts_with(util::toLowercase(strCustomMessage), "error:")) ssError << strCustomMessage.substr(6);
	
	size_t lineAtFileToEdit = 0;
	if(context) {
		std::string strScriptMsg = context->getStringVar(std::string() + '\xA3' + util::toLowercase(strScriptStringVariableID)); // must become lowercase or wont match
		
		if(boost::starts_with(util::toLowercase(strScriptMsg), "warn:" )) ssWarn  << " " << strScriptMsg.substr(5);
		if(boost::starts_with(util::toLowercase(strScriptMsg), "error:")) ssError << " " << strScriptMsg.substr(6);
		
		size_t columnDummy;
		context->getLineColumn(lineAtFileToEdit, columnDummy, context->getGoSubCallFromPos(callStackIndexFromLast));
		
		ssPopupMsg << ScriptContextPrefix(*context) << " [CallStackIndexFromLast=" << callStackIndexFromLast << "]\n"
			 << " [!!!ScriptDebugMessage!!!] " << strScriptMsg << "\n";
	}
	
	std::stringstream ssFlInfo; ssFlInfo << " at \"" << strFileToEdit << "\"";
	if(ssWarn.str().size()  > 0) LogWarning << ssWarn.str()  << ssFlInfo.str();
	if(ssError.str().size() > 0) LogError   << ssError.str() << ssFlInfo.str();
	
	return platform::askOkCancelCustomUserSystemPopupCommand(strTitle, ssPopupMsg.str(), strDetails, strFileToEdit, lineAtFileToEdit);
}

#pragma GCC push_options
#pragma GCC optimize ("O0") //required to let the breakpoint work
/* implementation suggestion:
 >>FUNCCustomCmdsB4DbgBreakpoint { showvars GoSub FUNCDebugBreakpoint RETURN } >>FUNCDebugBreakpoint { RETURN }
 * Call this inside the .asl script like: GoSub FUNCCustomCmdsB4DbgBreakpoint
 * If using nemiver to debug, just Shift+Ctrl+B and paste DebugBreakpoint at function name field.
 * This will be available also on release compilations as mod developers can use the system popup instead of a debugger! The system popup requires environment variables to be set, or it wont show up, what makes it ok also to players.
*/
static void DebugBreakpoint(std::string_view target, Context & context) {
	if(boost::contains(target, "debugbreakpoint")) { // this must be on the script call target name
		static int iDbgBrkPCount = 0;
		iDbgBrkPCount++; // put breakpoint here if using a debugger
		size_t callStackIndexFromLast = 1; // callStackIndexFromLast: n=0 (size-1(-n)=last) would return where FUNCDebugBreakpoint was called from. n=1 (size-1(-n)=last-1) would return where FUNCCustomCmdsB4DbgBreakpoint was called from.
		askOkCancelCustomUserSystemPopupCommand(
			"Debug",
			"Script Debug BreakPoint",
			context.getGoSubCallStack(
				"Script GoSub CallStack (targed ID was called from that line,column):\n ",
				"\n",
				" -> \n ",
				callStackIndexFromLast + 1), // +1 as getGoSubCallStack doesnt know about debugbreakpoint target
			(context).getScript()->file,
			"DebugMessage",
			&context,
			callStackIndexFromLast
		);
	}
}
#pragma GCC pop_options

bool Context::jumpToLabel(std::string_view target, bool substack) {
	
	size_t targetpos = FindScriptPos(m_script, std::string(">>") += target);
	if(targetpos == size_t(-1)) {
		return false;
	}
	
	m_pos = targetpos;
	
	if(substack) {
		m_stackIdCalledFromPos.push_back(std::make_pair(m_pos, std::string() += target));
	}
	
	DebugBreakpoint(target, *this);
	
	return true;
}

bool Context::returnToCaller() {
	
	if(m_stackIdCalledFromPos.empty()) {
		return false;
	}
	
	m_pos = m_stackIdCalledFromPos.back().first;
	m_stackIdCalledFromPos.pop_back();
	
	return true;
}

void Context::skipBlock() {
	
	std::string word = getCommand();
	if(m_pos == m_script->data.size()) {
		ScriptParserWarning << "missing statement before end of script";
		return;
	}
	
	if(word == "{") {
		long brackets = 1;
		while(brackets > 0) {
			
			skipWhitespace(true);
			word = getWord(false);
			if(m_pos == m_script->data.size()) {
				ScriptParserWarning << "missing '}' before end of script";
				return;
			}
			
			if(word == "{") {
				brackets++;
			} else if(word == "}") {
				brackets--;
			}
		}
	} else {
		skipCommand();
	}
	
	skipWhitespace(true);
	size_t oldpos = m_pos;
	word = getCommand();
	if(word != "else") {
		m_pos = oldpos;
	}
}

namespace {

typedef std::set<std::string_view> SuppressedCommands;
typedef std::map<std::string_view, SuppressedCommands> SuppressionsForFile;
typedef std::map<size_t, SuppressionsForFile> SuppressionsForPos;

size_t suppressionCount = 0;
SuppressionsForPos suppressions;
SuppressionsForPos blockSuppressions;

void suppress(std::string_view script, size_t pos, std::string_view command) {
	suppressionCount++;
	suppressions[pos][script].insert(command);
}

void suppressBlockEnd(std::string_view script, size_t pos, std::string_view command) {
	suppressionCount++;
	blockSuppressions[pos][script].insert(command);
}

bool contains(const SuppressionsForPos & list, const Context & context, std::string_view command) {
	
	SuppressionsForPos::const_iterator i0 = list.find(context.getPosition());
	if(i0 == list.end()) {
		return false;
	}
	
	SuppressionsForFile::const_iterator i1 = i0->second.find(context.getEntity() ? ((context.getScript() == &context.getEntity()->script) ? context.getEntity()->className() : context.getEntity()->idString()) : "unknown");
	if(i1 == i0->second.end()) {
		return false;
	}
	
	return (i1->second.find(command) != i1->second.end());
}

} // anonymous namespace

size_t initSuppressions() {
	
	suppressBlockEnd("akbaa_tentacle", 2428, "?"); // unexpected newline (newline inside command)
	suppressBlockEnd("akbaa_tentacle", 3420, "?"); // unexpected newline (newline inside command)
	
	suppressBlockEnd("camera_0027", 1140, "}"); // '}' should be commented out!
	
	suppressBlockEnd("black_thing_0002", 1075, "on"); // missing '}' and accept/refuse
	
	suppressBlockEnd("chest_metal_0103", 626, "on"); // missing '}' and accept/refuse
	
	suppressBlockEnd("chest_metal_0104", 667, "on"); // missing '}' and accept/refuse
	
	suppressBlockEnd("goblin_base_0021", 617, "on"); // missing '}'
	
	suppressBlockEnd("goblin_base_0031", 974, "on"); // missing '}'
	
	suppressBlockEnd("human_base_0082", 24110, "?"); // unexpected newline (newline inside command)
	suppressBlockEnd("human_base_0082", 24135, "?"); // unexpected newline (newline inside command)
	
	suppressBlockEnd("lever_0028", 402, "}"); // extranous '}'
	
	// TODO(broken-scripts)
	// TODO move to external file
	
	suppress("akbaa_phase2", 13884, "play"); // missing sound files 'akbaa_strike1' to 'akbaa_strike3', should be 'akbaa_strike'
	suppress("akbaa_phase2", 19998, "play"); // sound number is sonetimes too high; missing 'akbaa_hail1', should be 'akbaa_hail'
	suppress("akbaa_phase2", 18549, "playanim"); // animation 'grunt' not loaded
	
	suppress("akbaa_tentacle", 2428, "?"); // unexpected newline (newline inside command)
	suppress("akbaa_tentacle", 3420, "?"); // unexpected newline (newline inside command)
	suppress("akbaa_tentacle", 3747, "?"); // unexpected newline (missing parameter)
	suppress("akbaa_tentacle", 3747, "dodamage"); // missing target parameter
	
	suppress("axe_2handed", 26, "settwohanded"); // obsolete command
	
	suppress("black_thing", 3703, "play"); // variable is never set
	
	suppress("camera_0072", 269, "goto"); // missing label 'dialogue7_2'
	
	suppress("camera_0076", 2139, ""); // missing accept/refuse/return/goto/gosub before end of script
	
	suppress("black_thing_0003", 4360, "setevent"); // unsupported event: "eat"
	suppress("black_thing_0003", 4388, "setevent"); // unsupported event: "no_more_eat"
	suppress("black_thing_0003", 4411, "setevent"); // unsupported event: "no_eat"
	suppress("black_thing_0003", 4709, "behvaior"); // unknown command 'behvaior', should be 'behavior'
	
	suppress("chest_metal_0011", 78, "inventory add"); // missing object: "graph/obj3d/interactive/items/magic/dragon_bone_powder/dragon_bone_powder.teo" (should be 'powder_dragon_bone/dragon_bone_powder'?)
	
	suppress("chest_metal_0012", 389, "inventory add"); // missing object: "graph/obj3d/interactive/items/magic/dragon_bone_powder/dragon_bone_powder.teo" (should be 'powder_dragon_bone/dragon_bone_powder'?)
	
	suppress("chest_metal_0020", 54, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0020", 99, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0020", 149, "inventory add"); // missing object: "graph/obj3d/interactive/items/magic/ring_darkaa/ring_darkaa.teo" (should be 'ring_daarka/ring_daarka'?)
	
	suppress("chest_metal_0023", 495, "unsetcontrolledzone"); // unknown zone 'zone_7_bis'
	
	suppress("chest_metal_0029", 224, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0029", 317, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0029", 461, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0029", 557, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	suppress("chest_metal_0029", 650, "inventory add"); // missing object: "graph/obj3d/interactive/items/provisions/candle/candle.teo" (should be 'candle/candel'?)
	
	suppress("chest_metal_0045", 242, "inventory addfromscene"); // bad target ident: "magic\\potion_life\\potion_life"
	
	suppress("chest_metal_0095", 143, "inventory add"); // missing object: "graph/obj3d/interactive/items/armor/legging_leatherac/legging_leatherac.teo" (should be 'legging_leather_ac'?)
	
	suppress("chest_metal_0100", 629, "inventory add"); // missing object: "graph/obj3d/interactive/items/magic/dragon_bone_powder/dragon_bone_powder.teo" (should be 'powder_dragon_bone/dragon_bone_powder'?)
	suppress("chest_metal_0100", 693, "inventory add"); // missing object: "graph/obj3d/interactive/items/magic/dragon_bone_powder/dragon_bone_powder.teo" (should be 'powder_dragon_bone/dragon_bone_powder'?)
	
	suppress("chicken_base", 2037, "gosub"); // missing label 'save_behavior'
	suppress("chicken_base", 2410, "}"); // missing accept/refuse before end of event block
	
	suppress("corpse_0003", 399, "inventory addfromscene"); // bad target ident: "magic\\potion_life\\potion_life" (should be 'inventory add'?)
	
	suppress("corpse_0006", 172, "inventory add"); // missing object: "graph/obj3d/interactive/items/armor/helmet_leather/helmet_leather.teo"
	
	suppress("corpse_0084", 274, "inventory add"); // missing object: "graph/obj3d/interactive/items/weapons/chest_leather_ac/chest_leather_ac.teo"
	
	suppress("demon", 3571, "loadanim"); // missing animation 'demon_fight_left_start'
	suppress("demon", 3634, "loadanim"); // missing animation 'demon_fight_left_cycle'
	suppress("demon", 3698, "loadanim"); // missing animation 'demon_fight_left_strike'
	suppress("demon", 3762, "loadanim"); // missing animation 'demon_fight_right_start'
	suppress("demon", 3826, "loadanim"); // missing animation 'demon_fight_right_cycle'
	suppress("demon", 3891, "loadanim"); // missing animation 'demon_fight_right_strike'
	suppress("demon", 18479, "play"); // sound number is sometimes too high
	
	suppress("diamond", 139, "play"); // unknown flag -e (ignored) note: fix_inter/diamond_inwall/diamond.asl!
	
	suppress("dog", 19669, "play"); // sound number is sometimes too high
	
	suppress("dog_0011", 31, "playanim"); // animation 'action2' not loaded
	
	suppress("door_orbiplanax_chest", 371, "if"); // unknown operator '==1' (should be '== 1'), interpreted as constant true
	
	suppress("dragon_ice", 9029, "setevent"); // unsupported event 'agression', should be 'aggression'
	
	suppress("dragon_ice_0001", 93, "loadanim"); // missing animation: "dragon_talk_head"
	suppress("dragon_ice_0001", 3687, "playanim"); // animation 'action9' not loaded
	
	suppress("dragon's_lair_ice_wall", 41, "satangular"); // unknown command 'satangular', should be setangular
	
	suppress("dwarf_little_crusher_0001", 204, "?"); // 'playanim' only takes one parameter
	suppress("dwarf_little_crusher_0001", 228, "?"); // 'playanim' only takes one parameter
	
	suppress("dwarf_little_crusher_0002", 201, "?"); // 'playanim' only takes one parameter
	suppress("dwarf_little_crusher_0002", 225, "?"); // 'playanim' only takes one parameter
	
	suppress("dwarf_little_crusher_0003", 113, "?"); // 'playanim' only takes one parameter
	suppress("dwarf_little_crusher_0003", 137, "?"); // 'playanim' only takes one parameter
	
	suppress("emerald_inwall", 136, "play"); // unknown flag -e (ignored)
	
	suppress("fake_golden_snake", 185, "setinternalname"); // obsolete command (ignored)
	
	suppress("flour_bag", 41, "collison"); // unknown command 'collison', should be collision
	
	suppress("gem_inwall", 114, "play"); // unknown flag -e (ignored)
	
	suppress("goblin_base", 30010, "goto"); // missing label "main_alert"
	
	suppress("goblin_base_0009", 1455, "setevent"); // unsupported event: combine
	suppress("goblin_base_0009", 3864, "playanim"); // used -e flag without command
	
	suppress("goblin_base_0016", 2320, "playanim"); // used -e flag without command
	
	suppress("goblin_base_0027", 8463, "wrong]"); // space instead of _ in localisation key
	
	suppress("goblin_base_0034", 771, "detach"); // object mug_full_0003 already destroyed
	
	suppress("goblin_base_0041", 3063, "if"); // unknown operator '==1' (should be '== 1'), interpreted as constant true
	
	suppress("goblin_base_0048", 632, "setevent"); // unsupported event: combine
	
	suppress("goblin_base_0046", 2924, "if"); // unknown operator '=>' (should be '>='?), interpreted as constant true
	
	suppress("gold_chunk_inwall", 144, "play"); // unknown flag -e (ignored)
	
	suppress("golden_snake", 156, "setinternalname"); // obsolete command
	
	suppress("hammer_club", 66, "settwohanded"); // obsolete command
	
	suppress("hanged_gob", 526, "playanim"); // animation 'action3' not loaded

	suppress("human_base", 5872, "loadanim"); // bad animation id: 'bae_ready', should be 'bare_ready'
	suppress("human_base", 13711, "loadanim"); // missing animation "child_get_hit", should be "child_hit"?
	suppress("human_base", 13751, "loadanim"); // missing animation "child_get_hit", should be "child_hit"?
	suppress("human_base", 39089, "teleport"); // variable dying_marker not set
	suppress("human_base", 45586, "goto"); // missing label "main_alert"
	
	suppress("human_base_0006", 83, "playanim"); // animation 'wait' not loaded yet
	
	suppress("human_base_0012", 1519, "goto"); // missing label 'stop'
	
	suppress("human_base_0016", 7142, "setcontrolledzone"); // unknown zone 'calpale_beer', should be 'calpal_beer'?
	suppress("human_base_0016", 1270, "inventory addfromscene"); // unknown target 'key_calpale_0003' already taken by player?
	
	suppress("human_base_0022", 10108, "behaviormoveto"); // unknown command 'behaviormoveto', should be 'behavior move_to'
	
	suppress("human_base_0025", 732, "detach"); // object mug_full_0002 already destroyed
	
	suppress("human_base_0041", 4279, "if"); // missing space between oprateor '==' and argument
	
	suppress("human_base_0051", 5396, "/"); // unknown command, should be '//' instead of '/ /'
	
	suppress("human_base_0051", 6083, "set"); // bad variable name: "waiting"
	
	suppress("human_base_0046", 679, "goto"); // missing label 'next_step02', should be 'next_step01'?
	
	suppress("human_base_0079", 239, "inventory add"); // missing object: "graph/obj3d/interactive/items/armor/chest_leatherac/chest_leatherac.teo" (should be 'chest_leather_ac'?)
	suppress("human_base_0079", 303, "inventory add"); // missing object: "graph/obj3d/interactive/items/armor/leggings_leatherac/leggings_leatherac.teo" (should be 'legging_leather_ac'?)
	
	suppress("human_base_0082", 24110, "?"); // unexpected newline (newline inside command)
	suppress("human_base_0082", 24135, "?"); // unexpected newline (newline inside command)
	
	suppress("human_base_0085", 426, "loadanim"); // missing animation 'human_noraml_sit_out', should be 'human_normal_sit_out'?
	
	suppress("human_base_0086", 189, "if"); // unknown operator '==1' (should be '== 1')
	suppress("human_base_0086", 787, "loadanim"); // missing animation 'human_noraml_sit_out', should be 'human_normal_sit_out'?
	
	suppress("human_base_0095", 722, "setcontrolledzone"); // unknown zone 'maria_shop'
	
	suppress("human_base_0097", 9830, "speak"); // unexpected flags: -0 (should be -O?)
	
	suppress("human_base_0099", 997, "errata"); // unknown command 'errata', should be 'goto errata'
	
	suppress("human_base_0114", 6541, "teleport"); // unknown target 'marker_0327'
	
	suppress("human_base_0118", 101, "collisions"); // unknown command 'collisions', should be 'collision'
	
	suppress("human_base_0119", 179, "collisions"); // unknown command 'collisions', should be 'collision'
	
	suppress("human_base_0120", 101, "collisions"); // unknown command 'collisions', should be 'collision'
	
	suppress("human_base_0121", 135, "collisions"); // unknown command 'collisions', should be 'collision'
	
	suppress("human_base_0122", 350, "collisions"); // unknown command 'collisions', should be 'collision'
	
	suppress("human_base_0135", 939, "detroy"); // unknown command 'detroy', should be 'destroy'
	
	suppress("human_base_0136", 995, "detroy"); // unknown command 'detroy', should be 'destroy'
	
	suppress("human_base_0137", 992, "detroy"); // unknown command 'detroy', should be 'destroy'
	
	suppress("human_base_0138", 2439, "setcontrolledzone"); // unknown zone 'shani_flee'
	
	suppress("human_base_0174", 136, "play"); // missing sound file 'loop_crypt1l', should be 'ambiance/loop_crypt1l'
	
	suppress("jail_wood_grid", 152, "set"); // bad variable name: "material"
	
	suppress("lamp_goblin2_0003", 737, "no"); // unknown command 'no' should be 'nop'?
	
	suppress("lava_event01_0004", 277, "action1"); // unknown command 'action1' (missing space in '200 playanim')
	
	suppress("light_door", 422, "set"); // bad variable name: "durability"
	
	suppress("light_door_0019", 105, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0020", 230, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0021", 234, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0029", 88, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0030", 162, "setevent"); // unsupported event: "npc_open"
	suppress("light_door_0030", 488, "setevent"); // unsupported event: "npc_open"
	suppress("light_door_0030", 717, "setevent"); // unsupported event: "npc_open"
	
	suppress("light_door_0100", 69, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0102", 88, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("light_door_0106", 110, "setcontrolledzone"); // unknown zone 'city_entrance'
	
	suppress("light_door_0121", 88, "setspeakpitch"); // setspeakpitch only applies to NPCs
	
	suppress("lockpicks", 462, "play"); // missing sound file 'brokenweapon.wav', should be 'broken_weapon'
	
	suppress("long_sword_recovery", 591, "setequip"); // unknown flag '-s' (ignored)
	
	suppress("marker_0025", 288, "sendevent"); // unknown zone 'cooking' (should be 'cook_gary'?)
	
	suppress("marker_0247", 44, "setcontrolledzone"); // unknown zone 'level3_zone4'
	
	suppress("marker_0811", 536, "worldface"); // unknown command 'worldface', should be 'worldfade'
	
	suppress("metal_chunk_inwall", 143, "play"); // unknown flag -e (ignored)
	
	suppress("metal_grid_0008", 338, "}"); // missing accept/refuse before end of event block
	
	suppress("mithril_chunk_inwall", 144, "play"); // unknown flag -e (ignored)
	
	suppress("morning_glory", 971, "playanim"); // animation 'action1' not loaded
	
	suppress("orb_crypt", 76, "setsteal"); // setsteal only applies to items
	
	suppress("pig", 2409, "}"); // missing accept/refuse before end of event block
	
	suppress("player", 7725, "loadanim"); // bad animation id: "cast_hold"
	suppress("player", 8463, "loadanim"); // bad animation id: "lean_left_cycle"
	suppress("player", 8531, "loadanim"); // bad animation id: "lean_left_out"
	suppress("player", 8666, "loadanim"); // bad animation id: "lean_right_cycle"
	suppress("player", 8733, "loadanim"); // bad animation id: "lean_right_out"
	suppress("player", 9284, "loadanim"); // missing animation "human_death_cool"
	suppress("player", 9558, "loadanim"); // missing animation "human_talk_happyneutral_headonly"
	suppress("player", 18044, "play"); // missing sound file 'bell', should be 'arx_bell'
	
	suppress("porticullis_0039", 806, "setevent"); // unsupported event: "custom"
	
	suppress("porticullis_0049", 231, "?"); // missing '}' before end of script
	suppress("porticullis_0049", 231, ""); // missing accept / refuse / return before script end
	
	suppress("pressurepad_gob_0029", 74, "goto"); // missing label 'stress'
	
	suppress("public_notice_0011", 965, "magicoff"); // unknown command 'magicoff', should be 'magic off'
	
	suppress("rat_base", 17145, "play"); // sound number is sometimes too high
	
	suppress("rat_base_0059", 62, "behavior"); // unknown behavior 'firendly', should be 'friendly'
	suppress("rat_base_0059", 160, "behavior"); // unknown behavior 'firendly', should be 'friendly'
	
	suppress("rat_base_0077", 38, "?"); // unexpected newline (missing parameter)
	
	suppress("ratman_base", 22834, "goto"); // missing label "main_alert"
	
	suppress("ratman_base_0024", 608, "goto"); // missing label 'test'
	
	suppress("ratman_base_0026", 712, "setevent"); // unsupported event: "detect_player"
	
	suppress("rock_akbaa", 135, "setinternalname"); // obsolete command 'setinternalname'
	
	suppress("ruby_inwall", 135, "play"); // unknown flag -e (ignored)
	
	suppress("sausagev", 12376, "inventory playeraddfromscene"); // unknown target 'note_0015'
	
	suppress("secret_door_council_2b", 609, "}"); // extraneous '}'
	
	suppress("shiny_orb", 103, "setinternalname"); // obsolete command
	
	suppress("snake_woman_base", 26358, "goto"); // missing label 'main_alert'
	
	suppress("snake_woman_base_0004", 1660, "goto"); // unreachable code after goto
	
	suppress("snake_woman_base_0007", 1138, "goto"); // missing label 'short'
	
	suppress("snake_woman_base_0008", 16149, "goto"); // missing label 'dialogue5_2'
	
	suppress("snake_woman_base_0010", 122, "collions"); // unknown command 'collions', should be 'collision'
	
	suppress("snake_woman_base_0015", 113, "setevent"); // unsupported event: "misc_reflection"
	
	suppress("snake_woman_base_0016", 138, "setevent"); // unsupported event: "misc_reflection"
	
	suppress("spider_base_0024", 660, "play"); // missing sound file 'spider_stress'
	suppress("spider_base_0024", 858, "play"); // missing sound file 'spider_stress'
	
	suppress("sword_2handed_meteor_enchant_0001", 48, "}"); // missing accept/refuse before end of event block
	
	suppress("sword_mx", 458, "halo"); // unknown flag -a (ignored)
	
	suppress("sylib", 832, "timer"); // unknown flag -t (ignored)
	
	suppress("timed_lever_0033", 1027, "-smf"); // command wrongly interpreted as event (script parser limitation)
	
	suppress("timed_lever_0052", 648, "-smf"); // command wrongly interpreted as event (script parser limitation)
	
	suppress("torch_rotating_0004", 68, "?"); // 'playanim' only takes one parameter
	suppress("torch_rotating_0004", 88, "?"); // 'playanim' only takes one parameter
	suppress("torch_rotating_0004", 89, "rotatingtorchdown"); // 'playanim' only takes one parameter
	
	suppress("torch_rotating_0005", 68, "?"); // 'playanim' only takes one parameter
	suppress("torch_rotating_0005", 88, "?"); // 'playanim' only takes one parameter
	suppress("torch_rotating_0005", 89, "rotatingtorchdown"); // 'playanim' only takes one parameter
	
	suppress("training_dummy", 174, "play"); // missing sound file "wooddoorhit", closest match is "door_wood_hit"
	
	suppress("troll_base", 5107, "loadanim"); // missing animation: "troll_fight_ready_toponly"
	suppress("troll_base", 5175, "loadanim"); // missing animation: "troll_fight_unready_toponly"
	suppress("troll_base", 19054, "goto"); // missing label "main_alert"
	
	suppress("undead_base_0039", 102, "}"); // missing accept/refuse before end of event block
	
	suppress("undead_base_0046", 110, "playanim"); // animation 'wait' not loaded yet
	
	suppress("wall_breakable", 523, "}"); // missing accept/refuse before end of event block
	
	suppress("wrat_base", 17152, "play"); // sound number is sometimes too high
	
	suppress("y_mx", 3106, "loadanim"); // bad animation id: 'bae_ready', should be 'bare_ready'
	
	class FakeCommand : public Command {
		
	public:
		
		explicit FakeCommand(std::string_view name) : Command(name) { }
		
		Result execute(Context & context) override {
			ARX_UNUSED(context);
			return Success;
		}
		
	};
	
	/* 'playanim' only takes one parameter
	 * dwarf_little_crusher_0001:229
	 * dwarf_little_crusher_0002:226
	 * dwarf_little_crusher_0003:138
	 * need to use fake command so other commands on same line get executed!
	*/
	ScriptEvent::registerCommand(std::make_unique<FakeCommand>("dwarflittlecrusherup"));
	
	return suppressionCount;
}

bool isSuppressed(const Context & context, std::string_view command) {
	return contains(suppressions, context, command);
}

bool isBlockEndSuprressed(const Context & context, std::string_view command) {
	return contains(blockSuppressions, context, command);
}

} // namespace script
