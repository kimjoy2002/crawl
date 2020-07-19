#pragma once

enum shop_type
{
    SHOP_WEAPON,
    SHOP_ARMOUR,
    SHOP_WEAPON_ANTIQUE,
    SHOP_ARMOUR_ANTIQUE,
    SHOP_GENERAL_ANTIQUE,
    SHOP_JEWELLERY,
    SHOP_EVOKABLES, // wands, rods, and misc items
    SHOP_BOOK,
    SHOP_FOOD,
    SHOP_DISTILLERY,
    SHOP_SCROLL,
    SHOP_GENERAL,
    NUM_SHOPS, // must remain last 'regular' member {dlb}
    SHOP_UNASSIGNED = 100,
    SHOP_RANDOM,
};

enum inn_type
{
    INN_NAGA,
    INN_KOBOLD,
    INN_ORC,
    INN_ELF,
    INN_GENERAL,
    INN_PROFESSIONAL,   // you can get permant colleagues, they are very expensive, but strong.
    NUM_INNS,
    INN_UNASSIGNED = 100,
    INN_RANDOM,
};

