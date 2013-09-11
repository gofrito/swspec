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

#include "IniParser.h"
#include "Helpers.h"
#include <sstream>
#include <string>
#include <stdlib.h> // atoi()
#include <vector>
#include <iterator>

using std::cerr;
using std::endl;

// #define DEBUG_INI // define to increase verbosity

// TODO: 
//  - make the key comparisons case insensitive 
//  - protection against typos and mistakes: add a bag 
//    for "allowed keys" and print a warning if a key
//    in the INI is not one of the allowed keys
//  - overload getKeyValue to (key, int result) to
//    have string->int conversion directly

/***
 * IniParser(filename)
 *
 * Opens, loads and parses the specified INI file.
 * @param filename   File and path to the INI file.
 */
IniParser::IniParser(std::string filename)
{
    std::ifstream file(filename.c_str());
    std::string   line;
    int      linenr = -1;

    iniSections.clear();
    section_index = -1;

    if (!file.is_open()) { 
        cerr << "Error: Could not open INI file " << filename << endl;
        return; 
    }

    /* parse all lines */
    while (!std::getline(file, line).fail()) {

        std::string  mline;
        size_t  cpos;

        linenr++;

        // remove all ' ' spaces throughout the string
        std::string element;
        std::istringstream istr(line);
        while(istr >> element) {
           mline = mline + element;
        }

        // remove comments
        cpos = mline.find_first_of("/'#", 0);
        if (cpos != std::string::npos) {
            mline.resize(cpos);
        }

        // skip blank lines
        if (mline.size() < 1) {
            continue;
        }

        // add sections
        if (mline.at(0) == '[') {
            inisection_t nsec;
            cpos = mline.find_first_of("]", 0);
            if ((cpos == std::string::npos) || (cpos <= 1)) {
                cerr << "INI section line " << linenr << " '" << line << "' incorrectly formed'" << endl;
                continue;
            }
            nsec.sectionname = mline.substr(1, cpos-1);
            section_index++;
            iniSections.push_back(nsec);
            #ifdef DEBUG_INI
            cerr << "Added section '" << nsec.sectionname << "'" << endl;
            #endif
            continue;
        }

        // add key/value pairs
        cpos = mline.find_first_of("=", 0);
        if ((cpos != std::string::npos) && (cpos > 1)) {
            if (section_index < 0) {
                #ifdef DEBUG_INI
                cerr << "INI key/value pair '" << mline << "' is outside of a [section], skipping it" << endl;
                #endif
                continue;
            }
            keyvalpair_t kv;
            kv.key   = mline.substr(0, cpos);
            kv.value = mline.substr(cpos+1, std::string::npos);
            if (this->hasKey(kv.key)) {
                cerr << "Warning: INI section contains duplicate key '" << kv.key << "'! "
                     << "Not assigning new value '" << kv.value << "'." << endl;
                continue;
            }
            iniSections.at(section_index).entries.push_back(kv);
            #ifdef DEBUG_INI
            cerr << "Added key '" << kv.key << "' with value '" << kv.value << "'" << endl;
            #endif
            continue;
        }

        cerr << "INI has malformed line " << linenr << " '" << line << "' " 
             << "(parsed as '" << mline << "') " << endl;
    }

    return;
}

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
bool IniParser::selectSection(std::string sectionname)
{
    std::vector<inisection_t>::iterator itSect;
    this->section_index = -1;
    for (itSect=iniSections.begin(); itSect!=iniSections.end(); itSect++) {
        this->section_index++;
        if (itSect->sectionname.compare(sectionname) == 0) {
            return true;
        }
    }
    this->section_index = -1;
    return false;
}


/***
* bool hasKey(key)
 *
 * Looks in the active section for the specified key.
 * @param key    name of the key to find
 * @return       true if the key is found
 */
bool IniParser::hasKey(std::string const& key) const
{
    int si = this->section_index;
    if (si < 0) {
        return false;
    }

    std::vector<struct keyvalpair_tt>::const_iterator itKey;     
    std::vector<struct keyvalpair_tt> const& entries = iniSections.at(si).entries;
    for (itKey=entries.begin(); itKey!=entries.end(); itKey++) {
        if (itKey->key.compare(key) == 0) {
            return true;
        }
    }
    return false;
}


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
bool IniParser::getKeyValue(const char* key, std::string& value) const
{
    int si = this->section_index;
    if (si < 0) {
        return false;
    }

    std::vector<keyvalpair_t>::const_iterator itKey;
    std::vector<keyvalpair_t> const& entries = iniSections.at(si).entries;
    for (itKey=entries.begin(); itKey!=entries.end(); itKey++) {
        if (itKey->key.compare(key) == 0) {
            value = itKey->value;
            return true;
        }
    }
    return false;
}


/**
 * bool getKeyValue(key, int value)
 * Gets the value assigned to the given key in the
 * currently active section. If the key is not found,
 * the function returns false and the contents of 'value' 
 * remains unchanged.
 * @param key   name of the key to find and read
 * @param value returns the integer value of the key if it is found
 * @return      true if the key was found
 */
bool IniParser::getKeyValue(const char* key, int& value) const
{
   std::string strvalue;
   bool rc = getKeyValue(key, strvalue);
   if (rc) {
      value = (int)atof(strvalue.c_str());
   }
   return rc;
}

bool IniParser::getKeyValue(const char* key, size_t& value) const
{
   int ivalue;
   bool rc = getKeyValue(key, ivalue);
   if (rc) {
      value = (size_t)ivalue;
   }
   return rc;
}

bool IniParser::getKeyValue(const char* key, float& value) const
{
   std::string strvalue;
   bool rc = getKeyValue(key, strvalue);
   if (rc) {
      value = atof(strvalue.c_str());
   }
   return rc;
}

bool IniParser::getKeyValue(const char* key, bool& value) const
{
   std::string strvalue;
   bool rc = getKeyValue(key, strvalue);
   if (rc) {
      value = Helpers::parse_YesNo(strvalue.c_str());
   }
   return rc;
}


#ifdef UNIT_TEST

// For testing:
// g++ -Wall -DUNIT_TEST=1 IniParser.cpp -o iniparser
// <config.ini>:
//   or=phan
//      [SectionA]#comment
//   # comment1
//   key=valueSectA
//   kay3=llasdkj // comment
//   key=dontreplaceValuesectaA
//
//   [SectionB]

int main(int argc, char** argv)
{
   IniParser iniParser("config.ini");
   std::string v;
   cerr << "Attempt to find section 'SectionA': " << iniParser.selectSection("SectionA") << endl;
   cerr << "Check if has key 'key':" << iniParser.hasKey("key") << endl;
   cerr << "Get value of key 'key':" << iniParser.getKeyValue("key", v) << " - " << v << endl;
   cerr << "Get value of key 'grrbage':" << iniParser.getKeyValue("grrbage", v) << " - " << v << endl;
   cerr << "Attempt to find section 'Nonexistant': " << iniParser.selectSection("Nonexistant") << endl;
   cerr << "Get value of key 'key':" << iniParser.getKeyValue("key", v) << " - " << v << endl;
   return 0;
}
#endif
