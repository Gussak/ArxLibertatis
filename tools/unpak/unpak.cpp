/*
 * Copyright 2011 Arx Libertatis Team (see the AUTHORS file)
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

#include <string>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <algorithm>

#include "io/Filesystem.h"
#include "io/resource/PakReader.h"
#include "io/resource/PakEntry.h"
#include "io/resource/ResourcePath.h"
#include "io/FileStream.h"
#include "io/log/Logger.h"

using std::transform;
using std::ostringstream;
using std::string;

void dump(PakDirectory & dir, const res::path & dirname = res::path()) {
	
	fs::create_directories(dirname);
	
	for(PakDirectory::files_iterator i = dir.files_begin(); i != dir.files_end(); ++i) {
		
		res::path filename = dirname / i->first;
		
		PakFile * file = i->second;
		
		printf("%s\n", filename.string().c_str());
		
		fs::ofstream ofs(filename, fs::fstream::out | fs::fstream::binary | fs::fstream::trunc);
		if(!ofs.is_open()) {
			printf("error opening file for writing: %s\n", filename.string().c_str());
			exit(1);
		}
		
		if(file->size() > 0) {
			
			char * data = (char*)file->readAlloc();
			arx_assert(data != NULL);
			
			if(ofs.write(data, file->size()).fail()) {
				printf("error writing to file: %s\n", filename.string().c_str());
				exit(1);
			}
			
			free(data);
			
		}
		
	}
	
	for(PakDirectory::dirs_iterator i = dir.dirs_begin(); i != dir.dirs_end(); ++i) {
		dump(i->second, dirname / i->first);
	}
	
}

int main(int argc, char ** argv) {
	
	ARX_UNUSED(resources);
	
	Logger::init();
	
	if(argc < 2) {
		printf("usage: unpak <pakfile> [<pakfile>...]\n");
		return 1;
	}
	
	for(int i = 1; i < argc; i++) {
		
		PakReader pak;
		if(!pak.addArchive(argv[i])) {
			printf("error opening PAK file\n");
			return 1;
		}
		
		dump(pak);
		
	}
	
}
