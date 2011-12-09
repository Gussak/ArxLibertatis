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

#ifndef ARX_IO_FILESYSTEM_H
#define ARX_IO_FILESYSTEM_H

#include <stddef.h>
#include <ctime>
#include <string>

#include "platform/Platform.h"

namespace res { class path; }

namespace fs {

/**
 * Check if a file (directory or regular file) exists.
 * @return true if the file exists, false if it doesn't exist or there was an error
 */
bool exists(const res::path & p);

/**
 * Check if a path points to a directory.
 * @return true if the p exists and is a directory, false otherwise
 */
bool is_directory(const res::path & p);

/**
 * Check if a path points to a regular file.
 * @return true if the p exists and is a regular file, false otherwise.
 */
bool is_regular_file(const res::path & p);

/**
 * Get the last write time of a file.
 * @return the last write time or 0 if there was an error (file doesn't exist, ...).
 */
std::time_t last_write_time(const res::path & p);

/**
 * Get the size of a file.
 * @return the filesize or (u64)-1 if there was an error (file doesn't exist, ...).
 */
u64 file_size(const res::path & p);

/**
 * Remove a file or empty directory.
 * @return true if the file was removed or didn't exist.
 */
bool remove(const res::path & p);

/**
 * Recursively remove a file or directory.
 * @return true if the file was removed or didn't exist.
 */
bool remove_all(const res::path & p);

/**
 * Create a directory.
 * p.parent() must exist and be a directory.
 * @return true if the directory was created or false if there was an error.
 */
bool create_directory(const res::path & p);

/**
 * Create a directory.
 * All ancestors of p must either be a directory or not exist.
 * @return true if the directory was created or false if there was an error.
 */
bool create_directories(const res::path & p);

/**
 * Copy a regular file.
 * from_p must exist and be a regular file.
 * to_p.parent() must exist and be a directory.
 * new_p must not be a directory, even if overwrite is true
 * @return true if the file was copied or false if there was an error.
 */
bool copy_file(const res::path & from_p, const res::path & to_p, bool overwrite = false);

/**
 * Move a regular file or directory.
 * old_p must exist.
 * new_p.parent() must exist and be a directory.
 * new_p must not be a directory, even if overwrite is true
 * @return true if the file was copied or false if there was an error.
 */
bool rename(const res::path & old_p, const res::path & new_p, bool overwrite = false);

/**
 * Read a file into memory.
 * @param p The file to load.
 * @param size Will receive the size of the loaded file.
 * @return a new[]-allocated buffer containing the file data or NULL on error.
 */
char * read_file(const res::path & p, size_t & size);

class directory_iterator {
	
	directory_iterator operator++(int dummy); //!< prevent postfix ++
	
	//! prevent assignment
	directory_iterator & operator=(const directory_iterator &);
	directory_iterator(const directory_iterator &);
	
	void * handle;
	void * buf;
	
public:
	
	explicit directory_iterator(const res::path & p);
	
	~directory_iterator();
	
	directory_iterator & operator++();
	
	bool end();
	
	std::string name();
	
	bool is_directory();
	
	bool is_regular_file();
	
};

}

#endif // ARX_IO_FILESYSTEM_H
