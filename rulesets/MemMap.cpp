// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "Entity.h"
#include "MemMap.h"
#include "MemMap_methods.h"
#include "Script.h"

#include "modules/Location.h"

#include "common/debug.h"

#include <Atlas/Objects/Operation/Look.h>

void MemMap::addContents(const Element::MapType & entmap)
{
    Element::MapType::const_iterator I = entmap.find("contains");
    if ((I == entmap.end()) || (!I->second.IsList())) {
        return;
    }
    const Element::ListType & contlist = I->second.AsList();
    Element::ListType::const_iterator J = contlist.begin();
    for(;J != contlist.end(); J++) {
        if (!J->IsString()) {
            continue;
        }
        getAdd(J->AsString());
    }
}

Entity * MemMap::add(const Element::MapType & entmap)
{
    debug( std::cout << "MemMap::add" << std::endl << std::flush;);
    Element::MapType::const_iterator I = entmap.find("id");
    if ((I == entmap.end()) || (I->second.AsString().empty())) {
        return NULL;
    }
    const std::string & id = I->second.AsString();
    if (find(id)) {
        return update(entmap);
    }
    Entity * thing = new Entity(id);
    // thing->setId(id);
    I = entmap.find("name");
    if ((I != entmap.end()) && I->second.IsString()) {
        thing->setName(I->second.AsString());
    }
    I = entmap.find("type");
    if ((I != entmap.end()) && I->second.IsString()) {
        thing->setType(I->second.AsString());
    }
    thing->merge(entmap);
    I = entmap.find("loc");
    if ((I != entmap.end()) && I->second.IsString()) {
        getAdd(I->second.AsString());
    }
    thing->getLocation(entmap, m_things);
    addContents(entmap);
    return addObject(thing);
}

Entity * MemMap::update(const Element::MapType & entmap)
{
    debug( std::cout << "MemMap::update" << std::endl << std::flush;);
    Element::MapType::const_iterator I = entmap.find("id");
    if ((I == entmap.end()) || !I->second.IsString()) {
        return NULL;
    }
    const std::string & id = I->second.AsString();
    if (id.empty()) {
        return NULL;
    }
    debug( std::cout << " updating " << id << std::endl << std::flush;);
    EntityDict::const_iterator J = m_things.find(id);
    if (J == m_things.end()) {
        return add(entmap);
    }
    debug( std::cout << " " << id << " has already been spotted" << std::endl << std::flush;);
    Entity * thing = J->second;
    debug( std::cout << " got " << thing << std::endl << std::flush;);
    I = entmap.find("name");
    if (I != entmap.end() && I->second.IsString()) {
        thing->setName(I->second.AsString());
    }
    I = entmap.find("type");
    if (I != entmap.end() && I->second.IsString()) {
        thing->setType(I->second.AsString());
    }
    debug( std::cout << " got " << thing << std::endl << std::flush;);
    // It is important that the entity is not mutated here. Ie, an update
    // should not affect its type, contain or id, and location and
    // stamp should be updated with accurate information
    thing->merge(entmap);
    thing->getLocation(entmap,m_things);
    addContents(entmap);
    std::vector<std::string>::const_iterator K;
    for(K = m_updateHooks.begin(); K != m_updateHooks.end(); K++) {
        m_script->hook(*K, thing);
    }
    return thing;
}

EntityVector MemMap::findByType(const std::string & what)
{
    EntityVector res;
    EntityDict::const_iterator I;
    for(I = m_things.begin(); I != m_things.end(); I++) {
        Entity * item = I->second;
        debug( std::cout << "F" << what << ":" << item->getType() << ":" << item->getId() << std::endl << std::flush;);
        if (item->getType() == what) {
            res.push_back(I->second);
        }
    }
    return res;
}

EntityVector MemMap::findByLocation(const Location & loc, double radius)
{
    EntityVector res;
    EntityDict::const_iterator I;
    for(I = m_things.begin(); I != m_things.end(); I++) {
        const Location & oloc = I->second->m_location;
        if (!loc.isValid() || !oloc.isValid()) {
            continue;
        }
        if ((oloc.m_loc->getId() == loc.m_loc->getId()) &&
            (loc.m_pos.relativeDistance(oloc.m_pos) < (radius * radius))) {
            res.push_back(I->second);
        }
    }
    return res;
}

const Element MemMap::asObject()
{
    Element::MapType omap;
    EntityDict::const_iterator I = m_things.begin();
    for(;I != m_things.end(); I++) {
        I->second->addToObject((omap[I->first] = Element(Element::MapType())).AsMap());
    }
    return Element(omap);
}

void MemMap::flushMap()
{
    EntityDict::const_iterator I = m_things.begin();
    for (; I != m_things.end(); I++) {
        delete I->second;
    }
}
