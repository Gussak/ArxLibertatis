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

#include "util/String.h"

#include <utility>

#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace util {

std::string_view loadString(const char * data, size_t maxLength) {
	return std::string_view(data, std::find(data, data + maxLength, '\0') - data);
}

void storeString(char * dst, size_t maxLength, std::string_view src) {
	std::memcpy(dst, src.data(), std::min(maxLength, src.length()));
	if(maxLength > src.length()) {
		std::memset(dst + src.length(), 0, maxLength - src.length());
	}
}

void makeLowercase(std::string & string) {
	for(char & character : string) {
		character = toLowercase(character);
	}
}

struct character_escaper {
	template <typename FinderT>
	std::string operator()(const FinderT & match) const {
		std::string s;
		for(typename FinderT::const_iterator i = match.begin(); i != match.end(); ++i) {
			s += std::string("\\") + *i;
		}
		return s;
	}
};

std::string escapeString(std::string text, std::string_view escapeChars) {
	std::string escapedStr = std::move(text);
	boost::find_format_all(escapedStr, boost::token_finder(boost::is_any_of(escapeChars)), character_escaper());
	return escapedStr;
}

std::string getDateTimeString(std::string strFormat) {
	
	boost::posix_time::ptime localTime = boost::posix_time::second_clock::local_time();
	boost::gregorian::date::ymd_type ymd = localTime.date().year_month_day();
	boost::posix_time::time_duration hms = localTime.time_of_day();
	
	std::stringstream localTimeString;
	localTimeString << std::setfill('0');
	for(size_t i = 0; i < strFormat.size(); i++) {
		switch(strFormat[i]) {
			case 'Y': localTimeString << ymd.year; break;
			case 'M': localTimeString << ymd.month.as_number(); break;
			case 'D': localTimeString << ymd.day.as_number(); break;
			case 'h': localTimeString << hms.hours(); break;
			case 'm': localTimeString << hms.minutes(); break;
			case 's': localTimeString << hms.seconds(); break;
			default:  localTimeString << strFormat[i] << std::setw(2); break;
		}
	}
	
	return localTimeString.str();
}

void applyTokenAt(std::string & strAt, const std::string strToken, const std::string strText) {
	strAt.replace(strAt.find(strToken), strToken.size(), strText);
}

} // namespace util
