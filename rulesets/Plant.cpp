// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "Plant.h"

#include "Script.h"

#include "common/const.h"
#include "common/random.h"

#include "common/Tick.h"

#include <Atlas/Objects/Operation/Create.h>
#include <Atlas/Objects/Operation/Set.h>

Plant::Plant(const std::string & id) : Thing(id), fruits(0), radius(1), fruitName("seed")
{
    // Default to a 1m cube
    location.m_bBox = BBox(Vector3D(-0.5, -0.5, 0), Vector3D(0.5, 0.5, 1));

    subscribe("tick", OP_TICK);
}

Plant::~Plant()
{
}

const Element Plant::get(const std::string & aname) const
{
    if (aname == "fruits") {
        return fruits;
    } else if (aname == "radius") {
        return radius;
    } else if (aname == "fruitName") {
        return fruitName;
    } else if (aname == "fruitChance") {
        return fruitChance;
    } else if (aname == "sizeAdult") {
        return sizeAdult;
    }
    return Thing::get(aname);
}

void Plant::set(const std::string & aname, const Element & attr)
{
    if ((aname == "fruits") && attr.IsInt()) {
        fruits = attr.AsInt();
        update_flags |= a_fruit;
    } else if ((aname == "radius") && attr.IsInt()) {
        radius = attr.AsInt();
    } else if ((aname == "fruitName") && attr.IsString()) {
        fruitName = attr.AsString();
    } else if ((aname == "fruitChance") && attr.IsInt()) {
        fruitChance = attr.AsInt();
    } else if ((aname == "sizeAdult") && attr.IsNum()) {
        sizeAdult = attr.AsNum();
    } else {
        Thing::set(aname, attr);
    }
}

int Plant::dropFruit(OpVector & res)
{
    if (fruits < 1) { return 0; }
    int drop = std::min(fruits, randint(minuDrop, maxuDrop));
    fruits = fruits - drop;
    double height = location.m_bBox.farPoint().Z(); 
    for(int i = 0; i < drop; i++) {
        double rx = location.m_pos.X()+uniform(height*radius, -height*radius);
        double ry = location.m_pos.X()+uniform(height*radius, -height*radius);
        Element::MapType fmap;
        fmap["name"] = fruitName;
        fmap["parents"] = Element::ListType(1,fruitName);
        Location floc(location.m_loc, Vector3D(rx, ry, 0));
        floc.addToObject(fmap);
        RootOperation * create = new Create(Create::Instantiate());
        create->SetArgs(Element::ListType(1, fmap));
        res.push_back(create);
    }
    return drop;
}

OpVector Plant::TickOperation(const Tick & op)
{
    OpVector res;
    script->Operation("tick", op, res);
    RootOperation * tickOp = new Tick(Tick::Instantiate());
    tickOp->SetTo(getId());
    tickOp->SetFutureSeconds(consts::basic_tick * speed);
    res.push_back(tickOp);
    int dropped = dropFruit(res);
    if (location.m_bBox.farPoint().Z() > sizeAdult) {
        if (randint(1, fruitChance) == 1) {
            fruits++;
            dropped--;
        }
    }
    if (dropped != 0) {
        RootOperation * set = new Set(Set::Instantiate());
        Element::MapType pmap;
        pmap["id"] = getId();
        pmap["fruits"] = fruits;
        set->SetArgs(Element::ListType(1,pmap));
        res.push_back(set);
    }
    return res;
}
