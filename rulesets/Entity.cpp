// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "Entity.h"
#include "Script.h"

#include "common/log.h"
#include "common/debug.h"
#include "common/inheritance.h"

#include "common/Setup.h"
#include "common/Tick.h"
#include "common/Chop.h"
#include "common/Cut.h"
#include "common/Eat.h"
#include "common/Nourish.h"
#include "common/Burn.h"

#include <Atlas/Objects/Operation/Create.h>
#include <Atlas/Objects/Operation/Sight.h>
#include <Atlas/Objects/Operation/Set.h>
#include <Atlas/Objects/Operation/Delete.h>
#include <Atlas/Objects/Operation/Imaginary.h>
#include <Atlas/Objects/Operation/Move.h>
#include <Atlas/Objects/Operation/Sound.h>
#include <Atlas/Objects/Operation/Touch.h>
#include <Atlas/Objects/Operation/Look.h>
#include <Atlas/Objects/Operation/Appearance.h>
#include <Atlas/Objects/Operation/Disappearance.h>

static const bool debug_flag = false;

std::set<std::string> Entity::m_immutable;

const std::set<std::string> & Entity::immutables()
{
    if (m_immutable.empty()) {
        m_immutable.insert("parents");
        m_immutable.insert("pos");
        m_immutable.insert("loc");
        m_immutable.insert("velocity");
        m_immutable.insert("orientation");
        m_immutable.insert("contains");
    }
    return m_immutable;
}

Entity::Entity(const std::string & id) : BaseEntity(id),
                                         m_script(new Script), m_seq(0),
                                         m_status(1), m_type("entity"),
                                         m_mass(-1), m_perceptive(false),
                                         m_world(NULL), m_update_flags(0)
{
}

Entity::~Entity()
{
    if (m_script != NULL) {
        delete m_script;
    }
}

bool Entity::get(const std::string & aname, Element & attr) const
{
    if (aname == "status") {
        attr = m_status;
        return true;
    } else if (aname == "id") {
        attr = getId();
        return true;
    } else if (aname == "name") {
        attr = m_name;
        return true;
    } else if (aname == "mass") {
        attr = m_mass;
        return true;
    } else if (aname == "bbox") {
        attr = m_location.m_bBox.asList();
        return true;
    } else if (aname == "contains") {
        attr = Element::ListType();
        Element::ListType & contlist = attr.AsList();
        for(EntitySet::const_iterator I = m_contains.begin();
            I != m_contains.end(); I++) {
            contlist.push_back(*I);
        }
        return true;
    } else {
        Element::MapType::const_iterator I = m_attributes.find(aname);
        if (I != m_attributes.end()) {
            attr = I->second;
            return true;
        } else {
            return false;
        }
    }
}

void Entity::set(const std::string & aname, const Element & attr)
{
    if ((aname == "status") && attr.IsNum()) {
        m_status = attr.AsNum();
        m_update_flags |= a_status;
    } else if (aname == "id") {
        return;
    } else if ((aname == "name") && attr.IsString()) {
        m_name = attr.AsString();
        m_update_flags |= a_name;
    } else if ((aname == "mass") && attr.IsNum()) {
        m_mass = attr.AsNum();
        m_update_flags |= a_mass;
    } else if ((aname == "bbox") && attr.IsList() &&
               (attr.AsList().size() > 2)) {
        m_update_flags |= a_bbox;
        m_location.m_bBox = BBox(attr.AsList());
    } else {
        m_update_flags |= a_attr;
        m_attributes[aname] = attr;
    }
}

void Entity::setScript(Script * scrpt)
{
    if (m_script != NULL) {
        delete m_script;
    }
    m_script = scrpt;
    return;
}

void Entity::destroy()
{
    assert(m_location.m_loc != NULL);
    EntitySet & refContains = m_location.m_loc->m_contains;
    EntitySet::const_iterator I = m_contains.begin();
    for(; I != m_contains.end(); I++) {
        Entity * obj = *I;
        obj->m_location.m_loc = m_location.m_loc;
        obj->m_location.m_pos += m_location.m_pos;
        refContains.insert(obj);
    }
    refContains.erase(this);
    if (m_location.m_loc->m_contains.empty()) {
        m_location.m_loc->m_update_flags |= a_cont;
        m_location.m_loc->updated.emit();
    }
    destroyed.emit();
}

void Entity::addToObject(Element::MapType & omap) const
{
    // We need to have a list of keys to pull from attributes.
    Element::MapType::const_iterator I = m_attributes.begin();
    for (; I != m_attributes.end(); I++) {
        omap[I->first] = I->second;
    }
    if (!m_name.empty()) {
        omap["name"] = m_name;
    }
    omap["type"] = m_type;
    omap["mass"] = m_mass;
    omap["status"] = m_status;
    omap["stamp"] = (double)m_seq;
    omap["parents"] = Element::ListType(1, m_type);
    m_location.addToObject(omap);
    Element::ListType contlist;
    EntitySet::const_iterator J = m_contains.begin();
    for(; J != m_contains.end(); J++) {
        contlist.push_back((*J)->getId());
    }
    if (!contlist.empty()) {
        omap["contains"] = contlist;
    }
    BaseEntity::addToObject(omap);
}

void Entity::merge(const Element::MapType & ent)
{
    const std::set<std::string> & imm = immutables();
    for(Element::MapType::const_iterator I = ent.begin(); I != ent.end(); I++){
        const std::string & key = I->first;
        if (imm.find(key) != imm.end()) continue;
        set(key, I->second);
    }
}

bool Entity::getLocation(const Element::MapType & entmap,
                         const EntityDict & eobjects)
{
    debug( std::cout << "Thing::getLocation" << std::endl << std::flush;);
    Element::MapType::const_iterator I = entmap.find("loc");
    if ((I == entmap.end()) || !I->second.IsString()) {
        debug( std::cout << getId() << ".. has no loc" << std::endl << std::flush;);
        return true;
    }
    try {
        const std::string & ref_id = I->second.AsString();
        EntityDict::const_iterator J = eobjects.find(ref_id);
        if (J == eobjects.end()) {
            debug( std::cout << "ERROR: Can't get ref from objects dictionary" << std::endl << std::flush;);
            return true;
        }
            
        m_location.m_loc = J->second;
        I = entmap.find("pos");
        if (I != entmap.end()) {
            m_location.m_pos = Vector3D(I->second.AsList());
        }
        I = entmap.find("velocity");
        if (I != entmap.end()) {
            m_location.m_velocity = Vector3D(I->second.AsList());
        }
        I = entmap.find("orientation");
        if (I != entmap.end()) {
            m_location.m_orientation = Quaternion(I->second.AsList());
        }
        I = entmap.find("bbox");
        if (I != entmap.end()) {
            m_location.m_bBox = BBox(I->second.AsList());
        }
    }
    catch (Atlas::Message::WrongTypeException) {
        log(ERROR, "getLocation: Bad location data");
        return true;
    }
    return false;
}

Vector3D Entity::getXyz() const
{
    //Location l=location;
    if (!m_location.isValid()) {
        static Vector3D ret(0.0,0.0,0.0);
        return ret;
    }
    if (m_location.m_loc) {
        return Vector3D(m_location.m_pos) += m_location.m_loc->getXyz();
    } else {
        return m_location.m_pos;
    }
}

void Entity::scriptSubscribe(const std::string & op)
{
    OpNo n = Inheritance::instance().opEnumerate(op);
    if (n != OP_INVALID) {
        debug(std::cout << "SCRIPT requesting subscription to " << op
                        << std::endl << std::flush;);
        subscribe(op, n);
    } else {
        std::string msg = std::string("SCRIPT requesting subscription to ")
                        + op + " but inheritance could not give me a reference";
        log(ERROR, msg.c_str());
    }
}

OpVector Entity::SetupOperation(const Setup & op)
{
    OpVector res;
    m_script->Operation("setup", op, res);
    return res;
}

OpVector Entity::TickOperation(const Tick & op)
{
    OpVector res;
    m_script->Operation("tick", op, res);
    return res;
}

OpVector Entity::ActionOperation(const Action & op)
{
    OpVector res;
    m_script->Operation("action", op, res);
    return res;
}

OpVector Entity::ChopOperation(const Chop & op)
{
    OpVector res;
    m_script->Operation("chop", op, res);
    return res;
}

OpVector Entity::CreateOperation(const Create & op)
{
    OpVector res;
    m_script->Operation("create", op, res);
    return res;
}

OpVector Entity::CutOperation(const Cut & op)
{
    OpVector res;
    m_script->Operation("cut", op, res);
    return res;
}

OpVector Entity::DeleteOperation(const Delete & op)
{
    OpVector res;
    m_script->Operation("delete", op, res);
    return res;
}

OpVector Entity::EatOperation(const Eat & op)
{
    OpVector res;
    m_script->Operation("eat", op, res);
    return res;
}

OpVector Entity::BurnOperation(const Burn & op)
{
    OpVector res;
    m_script->Operation("burn", op, res);
    return res;
}

OpVector Entity::ImaginaryOperation(const Imaginary & op)
{
    OpVector res;
    m_script->Operation("imaginary", op, res);
    return res;
}

OpVector Entity::MoveOperation(const Move & op)
{
    OpVector res;
    m_script->Operation("move", op, res);
    return res;
}

OpVector Entity::NourishOperation(const Nourish & op)
{
    OpVector res;
    m_script->Operation("nourish", op, res);
    return res;
}

OpVector Entity::SetOperation(const Set & op)
{
    OpVector res;
    m_script->Operation("set", op, res);
    return res;
}

OpVector Entity::SightOperation(const Sight & op)
{
    OpVector res;
    m_script->Operation("sight", op, res);
    return res;
}

OpVector Entity::SoundOperation(const Sound & op)
{
    OpVector res;
    m_script->Operation("sound", op, res);
    return res;
}

OpVector Entity::TouchOperation(const Touch & op)
{
    OpVector res;
    m_script->Operation("touch", op, res);
    return res;
}

OpVector Entity::LookOperation(const Look & op)
{
    OpVector res;
    if (m_script->Operation("look", op, res) != 0) {
        return res;
    }

    Sight * s = new Sight( Sight::Instantiate());
    Element::ListType & args = s->GetArgs();
    args.push_back(Element::MapType());
    Element::MapType & amap = args.front().AsMap();
    addToObject(amap);
    s->SetTo(op.GetFrom());

    return OpVector(1,s);
}

OpVector Entity::AppearanceOperation(const Appearance & op)
{
    OpVector res;
    m_script->Operation("appearance", op, res);
    return res;
}

OpVector Entity::DisappearanceOperation(const Disappearance & op)
{
    OpVector res;
    m_script->Operation("disappearance", op, res);
    return res;
}

OpVector Entity::OtherOperation(const RootOperation & op)
{
    const std::string & op_type = op.GetParents().front().AsString();
    OpVector res;
    debug(std::cout << "Entity " << getId() << " got custom " << op_type << " op"
               << std::endl << std::flush;);
    m_script->Operation(op_type, op, res);
    return res;
}
