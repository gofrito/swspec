#ifndef _INIPARSER_H
#define _INIPARSER_H
/************************************************************************
 * IBM Cell / Intel Software Spectrometer
 * Copyright (C) 2008 Jan Wagner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
**************************************************************************/

#include <iostream>
#include <fstream>

#include <vector>
#include <string>

typedef struct keyvalpair_tt {
    std::string key;
    std::string value;
} keyvalpair_t;

typedef struct inisection_tt {
    std::string sectionname;
    std::vector<keyvalpair_t> entries;
} inisection_t;

class IniParser {

  public:

    /***
     * IniParser(filename)
     *
     * Opens, loads and parses the specified INI file.
     * @param filename   File and path to the INI file.
     */
    IniParser(std::string filename);

    ~IniParser() { return; }

  private:
    std::vector<inisection_t> iniSections;
    int section_index;

  public:

    /***
     * bool selectSection(sectioname)
     *
     * Finds the INI file section ([sectionname]) and
     * sets it as the active section. The hasKey() and
     * getKeyValue() functions will use the active
     * section to look for key-value pairs.
     * @param sectionname  name of the INI section
     * @return             true if the section is found
     */
    bool selectSection(std::string sectionname);

    /***
     * bool hasKey(key)
     *
     * Looks in the active section for the specified key.
     * @param key    name of the key to find
     * @return       true if the key was found
     */
    bool hasKey(std::string const& key) const;

    /***
     * bool getKeyValue(key, string value)
     *
     * Gets the value assigned to the given key in the
     * currently active section. If the key is not found,
     * the function returns false and the contents of 'value'
     * remains unchanged.
     * @param key   name of the key to find and read
     * @param value returns the string value of the key if it is found
     * @return      true if the key was found
     */
    bool getKeyValue(const char*, std::string& value) const;
    bool getKeyValue(const char*, int& value) const;
    bool getKeyValue(const char*, size_t& value) const;
    bool getKeyValue(const char*, float& value) const;
    bool getKeyValue(const char*, bool&) const;

};

#endif // _INIPARSER_H
