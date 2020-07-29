/**
 * @file
 * @brief inn keeper functions.
**/

#include "AppHdr.h"

#include "innping.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "artefact.h"
#include "branch.h"
#include "butcher.h"
#include "cio.h"
#include "colour.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "merc-name.h"
#include "merc-prop.h"
#include "merc-status-flag-type.h"
#include "mercs.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "prompt.h"
#include "rot.h"
#include "spl-book.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#ifdef USE_TILE_LOCAL
#include "tilereg-crt.h"
#endif
#include "travel.h"
#include "unicode.h"
#include "unwind.h"

innpingList innping_list;

static int _bargain_cost(int value)
{
    // 20% discount
    value *= 8;
    value /= 10;

    return max(value, 1);
}

static int _inn_get_merc_value(const merc_def& merc, int greed, bool id, bool ignore_bargain)
{
    int result = (greed * merc_value(merc, id) / 10);

    if (you.duration[DUR_BARGAIN] && !ignore_bargain)
        result = _bargain_cost(result);

    return max(result, 1);
}

int merc_price(const merc_def& merc, const inn_struct& inn, bool ignore_bargain)
{
    return _inn_get_merc_value(merc, inn.greed, inntype_identifies_stock(inn.type), ignore_bargain);
}

// This probably still needs some work. Rings used to be the only
// artefacts which had a change in price, and that value corresponds
// to returning 50 from this function. Good artefacts will probably
// be returning just over 30 right now. Note that this isn't used
// as a multiple, its used in the old ring way: 7 * ret is added to
// the price of the artefact. -- bwr
int artefact_value(const merc_def &merc)
{
    ASSERT(is_artefact(merc));

    int ret = 10;
    artefact_properties_t prop;
    artefact_properties(merc, prop);

    // Brands are already accounted for via existing ego checks

    // This should probably be more complex... but this isn't so bad:
    ret += 6 * prop[ ARTP_AC ]
            + 6 * prop[ ARTP_EVASION ]
            + 4 * prop[ ARTP_SHIELDING ]
            + 6 * prop[ ARTP_SLAYING ]
            + 3 * prop[ ARTP_STRENGTH ]
            + 3 * prop[ ARTP_INTELLIGENCE ]
            + 3 * prop[ ARTP_DEXTERITY ]
            + 4 * prop[ ARTP_HP ]
            + 3 * prop[ ARTP_MAGICAL_POWER ];

    // These resistances have meaningful levels
    if (prop[ ARTP_FIRE ] > 0)
        ret += 5 + 5 * (prop[ ARTP_FIRE ] * prop[ ARTP_FIRE ]);
    else if (prop[ ARTP_FIRE ] < 0)
        ret -= 10;

    if (prop[ ARTP_COLD ] > 0)
        ret += 5 + 5 * (prop[ ARTP_COLD ] * prop[ ARTP_COLD ]);
    else if (prop[ ARTP_COLD ] < 0)
        ret -= 10;

    if (prop[ ARTP_MAGIC_RESISTANCE ] > 0)
        ret += 4 + 4 * prop[ ARTP_MAGIC_RESISTANCE ];
    else if (prop[ ARTP_MAGIC_RESISTANCE ] < 0)
        ret -= 6;

    if (prop[ ARTP_NEGATIVE_ENERGY ] > 0)
        ret += 3 + 3 * (prop[ARTP_NEGATIVE_ENERGY] * prop[ARTP_NEGATIVE_ENERGY]);

    // Discount Stlth-, charge for Stlth+
    ret += 2 * prop[ARTP_STEALTH];
    // Stlth+ costs more than Stlth- cheapens
    if (prop[ARTP_STEALTH] > 0)
        ret += 2 * prop[ARTP_STEALTH];

    // only one meaningful level:
    if (prop[ ARTP_POISON ])
        ret += 6;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_ELECTRICITY ])
        ret += 10;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RCORR ])
        ret += 8;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RMUT ])
        ret += 8;

    if (prop[ ARTP_SEE_INVISIBLE ])
        ret += 6;

    // abilities:
    if (prop[ ARTP_FLY ])
        ret += 3;

    if (prop[ ARTP_BLINK ])
        ret += 10;

    if (prop[ ARTP_BERSERK ])
        ret += 5;

    if (prop[ ARTP_INVISIBLE ])
        ret += 10;

    if (prop[ ARTP_ANGRY ])
        ret -= 3;

    if (prop[ ARTP_CAUSE_TELEPORTATION ])
        ret -= 3;

    if (prop[ ARTP_NOISE ])
        ret -= 5;

    if (prop[ ARTP_PREVENT_TELEPORTATION ])
        ret -= 8;

    if (prop[ ARTP_PREVENT_SPELLCASTING ])
        ret -= 10;

    if (prop[ ARTP_CONTAM ])
        ret -= 8;

    if (prop[ ARTP_CORRODE ])
        ret -= 8;

    if (prop[ ARTP_DRAIN ])
        ret -= 8;

    if (prop[ ARTP_SLOW ])
        ret -= 8;

    if (prop[ ARTP_FRAGILE ])
        ret -= 8;

    if (prop[ ARTP_RMSL ])
        ret += 20;

    if (prop[ ARTP_CLARITY ])
        ret += 20;

    return (ret > 0) ? ret : 0;
}

unsigned int merc_value(merc_def merc, bool ident)
{
    // Note that we pass merc in by value, since we want a local
    // copy to mangle as necessary.
    merc.flags = (ident) ? (merc.flags | ISFLAG_IDENT_MASK) : (merc.flags);

    if (is_unrandom_artefact(merc)
        && merc_ident(merc, ISFLAG_KNOW_PROPERTIES))
    {
        const unrandart_entry *entry = get_unrand_entry(merc.unrand_idx);
        if (entry->value != 0)
            return entry->value;
    }

    int valued = 0;

    switch (merc.base_type)
    {
    case OBJ_WEAPONS:
        valued += weapon_base_price((weapon_type)merc.sub_type);

        if (merc_type_known(merc))
        {
            switch (get_weapon_brand(merc))
            {
            case SPWPN_NORMAL:
            default:            // randart
                valued *= 10;
                break;

            case SPWPN_SPEED:
            case SPWPN_VAMPIRISM:
            case SPWPN_ANTIMAGIC:
                valued *= 30;
                break;

            case SPWPN_DISTORTION:
            case SPWPN_ELECTROCUTION:
            case SPWPN_PAIN:
            case SPWPN_ACID: // Unrand-only.
            case SPWPN_PENETRATION: // Unrand-only.
                valued *= 25;
                break;

            case SPWPN_CHAOS:
            case SPWPN_DRAINING:
            case SPWPN_FLAMING:
            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                valued *= 18;
                break;

            case SPWPN_VORPAL:
                valued *= 15;
                break;

            case SPWPN_PROTECTION:
            case SPWPN_VENOM:
                valued *= 12;
                break;
            }

            valued /= 10;
        }

        if (merc_ident(merc, ISFLAG_KNOW_PLUSES))
            valued += 50 * merc.plus;

        if (is_artefact(merc))
        {
            if (merc_type_known(merc))
                valued += (7 * artefact_value(merc));
            else
                valued += 50;
        }
        else if (merc_type_known(merc)
                 && get_equip_desc(merc) != 0) // ???
        {
            valued += 20;
        }
        else if (!(merc.flags & ISFLAG_IDENT_MASK)
                 && (get_equip_desc(merc) != 0))
        {
            valued += 30; // un-id'd "glowing" - arbitrary added cost
        }

        if (merc_known_cursed(merc))
            valued -= 30;

        break;

    case OBJ_MISSILES:          // ammunition
        valued += missile_base_price((missile_type)merc.sub_type);

        if (merc_type_known(merc))
        {
            switch (get_ammo_brand(merc))
            {
            case SPMSL_NORMAL:
            default:
                valued *= 10;
                break;

            case SPMSL_CHAOS:
                valued *= 40;
                break;

            case SPMSL_CURARE:
            case SPMSL_BLINDING:
            case SPMSL_SILVER:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_PARALYSIS:
            case SPMSL_PENETRATION:
            case SPMSL_STEEL:
#endif
            case SPMSL_DISPERSAL:
                valued *= 30;
                break;

#if TAG_MAJOR_VERSION == 34
            case SPMSL_FLAME:
            case SPMSL_FROST:
            case SPMSL_SLEEP:
            case SPMSL_CONFUSION:
                valued *= 25;
                break;
#endif

            case SPMSL_POISONED:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_RETURNING:
            case SPMSL_EXPLODING:
            case SPMSL_SLOW:
            case SPMSL_SICKNESS:
#endif
            case SPMSL_FRENZY:
                valued *= 20;
                break;
            }

            valued /= 10;
        }
        break;

    case OBJ_ARMOUR:
        valued += armour_base_price((armour_type)merc.sub_type);

        if (merc_type_known(merc))
        {
            const int sparm = get_armour_ego_type(merc);
            switch (sparm)
            {
            case SPARM_RUNNING:
            case SPARM_ARCHMAGI:
            case SPARM_RESISTANCE:
                valued += 250;
                break;

            case SPARM_COLD_RESISTANCE:
            case SPARM_DEXTERITY:
            case SPARM_FIRE_RESISTANCE:
            case SPARM_SEE_INVISIBLE:
            case SPARM_INTELLIGENCE:
            case SPARM_FLYING:
            case SPARM_STEALTH:
            case SPARM_STRENGTH:
            case SPARM_INVISIBILITY:
            case SPARM_MAGIC_RESISTANCE:
            case SPARM_PROTECTION:
            case SPARM_ARCHERY:
            case SPARM_REPULSION:
                valued += 50;
                break;

            case SPARM_POSITIVE_ENERGY:
            case SPARM_POISON_RESISTANCE:
            case SPARM_REFLECTION:
            case SPARM_SPIRIT_SHIELD:
                valued += 20;
                break;

            case SPARM_PONDEROUSNESS:
                valued -= 250;
                break;
            }
        }

        if (merc_ident(merc, ISFLAG_KNOW_PLUSES))
            valued += 50 * merc.plus;

        if (is_artefact(merc))
        {
            if (merc_type_known(merc))
                valued += (7 * artefact_value(merc));
            else
                valued += 50;
        }
        else if (merc_type_known(merc) && get_equip_desc(merc) != 0)
            valued += 20;  // ???
        else if (!(merc.flags & ISFLAG_IDENT_MASK)
                 && (get_equip_desc(merc) != 0))
        {
            valued += 30; // un-id'd "glowing" - arbitrary added cost
        }

        if (merc_known_cursed(merc))
            valued -= 30;

        break;

    case OBJ_WANDS:
        if (!merc_type_known(merc))
            valued += 40;
        else
        {
            // true if the wand is of a good type, a type with significant
            // inherent value even when empty. Good wands are less expensive
            // per charge.
            bool good = false;
            switch (merc.sub_type)
            {
            case WAND_CLOUDS:
            case WAND_SCATTERSHOT:
            case WAND_TELEPORTATION:
            case WAND_HASTING:
            case WAND_HEAL_WOUNDS:
                valued += 120;
                good = true;
                break;

            case WAND_ACID:
            case WAND_DIGGING:
                valued += 80;
                good = true;
                break;

            case WAND_ICEBLAST:
            case WAND_DISINTEGRATION:
                valued += 40;
                good = true;
                break;

            case WAND_ENSLAVEMENT:
            case WAND_POLYMORPH:
            case WAND_PARALYSIS:
                valued += 20;
                break;

            case WAND_FLAME:
            case WAND_RANDOM_EFFECTS:
                valued += 10;
                break;

            default:
                valued += 6;
                break;
            }

            if (merc_ident(merc, ISFLAG_KNOW_PLUSES))
            {
                if (good) valued += (valued * merc.plus) / 4;
                else      valued += (valued * merc.plus) / 2;
            }
        }
        break;

    case OBJ_POTIONS:
        if (!merc_type_known(merc))
            valued += 9;
        else
        {
            switch (merc.sub_type)
            {
            case POT_EXPERIENCE:
                valued += 500;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_GAIN_DEXTERITY:
            case POT_GAIN_INTELLIGENCE:
            case POT_GAIN_STRENGTH:
                valued += 350;
                break;

            case POT_CURE_MUTATION:
                valued += 250;
                break;
#endif

            case POT_RESISTANCE:
            case POT_HASTE:
                valued += 100;
                break;

            case POT_MAGIC:
            case POT_INVISIBILITY:
            case POT_CANCELLATION:
            case POT_AMBROSIA:
            case POT_MUTATION:
                valued += 80;
                break;

            case POT_BERSERK_RAGE:
            case POT_HEAL_WOUNDS:
#if TAG_MAJOR_VERSION == 34
            case POT_RESTORE_ABILITIES:
#endif
                valued += 50;
                break;

            case POT_MIGHT:
            case POT_AGILITY:
            case POT_BRILLIANCE:
            case POT_UNSTABLE_MUTATION:
                valued += 40;
                break;

            case POT_CURING:
            case POT_LIGNIFY:
            case POT_FLIGHT:
                valued += 30;
                break;

            case POT_POISON:
#if TAG_MAJOR_VERSION == 34
            case POT_STRONG_POISON:
            case POT_PORRIDGE:
            case POT_SLOWING:
            case POT_DECAY:
            case POT_BLOOD:
#endif
            case POT_DEGENERATION:
                valued += 10;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_BLOOD_COAGULATED:
                valued += 5;
                break;
#endif
            case POT_WATER:
                valued++;
                break;
            }
        }
        break;

    case OBJ_FOOD:
        switch (merc.sub_type)
        {
        case FOOD_RATION:
            valued = 50;
            break;

        case FOOD_CHUNK:
        default:
            break;
        }
        break;

    case OBJ_CORPSES:
        valued = max_corpse_chunks(merc.mon_type) * 5;

    case OBJ_SCROLLS:
        if (!merc_type_known(merc))
            valued += 10;
        else
        {
            switch (merc.sub_type)
            {
            case SCR_ACQUIREMENT:
                valued += 520;
                break;

            case SCR_BRAND_WEAPON:
                valued += 200;
                break;

            case SCR_SUMMONING:
                valued += 95;
                break;

            case SCR_BLINKING:
            case SCR_ENCHANT_ARMOUR:
            case SCR_ENCHANT_WEAPON:
            case SCR_TORMENT:
            case SCR_HOLY_WORD:
            case SCR_SILENCE:
            case SCR_VULNERABILITY:
                valued += 75;
                break;

            case SCR_AMNESIA:
            case SCR_FEAR:
            case SCR_IMMOLATION:
            case SCR_MAGIC_MAPPING:
                valued += 35;
                break;

            case SCR_REMOVE_CURSE:
            case SCR_TELEPORTATION:
                valued += 30;
                break;

            case SCR_FOG:
            case SCR_IDENTIFY:
#if TAG_MAJOR_VERSION == 34
            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_CURSE_JEWELLERY:
#endif
                valued += 20;
                break;

            case SCR_NOISE:
            case SCR_RANDOM_USELESSNESS:
                valued += 10;
                break;
            }
        }
        break;

    case OBJ_JEWELLERY:
        if (merc_known_cursed(merc))
            valued -= 30;

        if (!merc_type_known(merc))
            valued += 50;
        else
        {
            // Variable-strength rings.
            if (merc_ident(merc, ISFLAG_KNOW_PLUSES)
                && (merc.sub_type == RING_PROTECTION
                    || merc.sub_type == RING_STRENGTH
                    || merc.sub_type == RING_EVASION
                    || merc.sub_type == RING_DEXTERITY
                    || merc.sub_type == RING_INTELLIGENCE
                    || merc.sub_type == RING_SLAYING
                    || merc.sub_type == AMU_REFLECTION))
            {
                // Formula: price = kn(n+1) / 2, where k depends on the subtype,
                // n is the power. (The base variable is equal to 2n.)
                int base = 0;
                int coefficient = 0;
                if (merc.sub_type == RING_SLAYING)
                    base = 3 * merc.plus;
                else
                    base = 2 * merc.plus;

                switch (merc.sub_type)
                {
                case RING_SLAYING:
                case RING_PROTECTION:
                case RING_EVASION:
                    coefficient = 40;
                    break;
                case RING_STRENGTH:
                case RING_DEXTERITY:
                case RING_INTELLIGENCE:
                case AMU_REFLECTION:
                    coefficient = 30;
                    break;
                default:
                    break;
                }

                if (base <= 0)
                    valued += 25 * base;
                else
                    valued += (coefficient * base * (base + 1)) / 8;
            }
            else
            {
                switch (merc.sub_type)
                {
                case AMU_FAITH:
                case AMU_RAGE:
                    valued += 400;
                    break;

                case RING_WIZARDRY:
                case AMU_REGENERATION:
                case AMU_GUARDIAN_SPIRIT:
                case AMU_THE_GOURMAND:
                case AMU_HARM:
                case AMU_MANA_REGENERATION:
                case AMU_ACROBAT:
                    valued += 300;
                    break;

                case RING_FIRE:
                case RING_ICE:
                case RING_PROTECTION_FROM_COLD:
                case RING_PROTECTION_FROM_FIRE:
                case RING_PROTECTION_FROM_MAGIC:
                    valued += 250;
                    break;

                case RING_MAGICAL_POWER:
                case RING_LIFE_PROTECTION:
                case RING_POISON_RESISTANCE:
                case RING_RESIST_CORROSION:
                    valued += 200;
                    break;

                case RING_STEALTH:
                case RING_FLIGHT:
                    valued += 175;
                    break;

                case RING_SEE_INVISIBLE:
                    valued += 150;
                    break;

                case RING_ATTENTION:
                case RING_TELEPORTATION:
                case AMU_NOTHING:
                    valued += 75;
                    break;

                case AMU_INACCURACY:
                    valued -= 300;
                    break;
                    // got to do delusion!
                }
            }

            if (is_artefact(merc))
            {
                // in this branch we're guaranteed to know
                // the merc type!
                if (valued < 0)
                    valued = (artefact_value(merc) - 5) * 7;
                else
                    valued += artefact_value(merc) * 7;
            }

            // Hard minimum, as it's worth 20 to ID a ring.
            valued = max(20, valued);
        }
        break;

    case OBJ_MISCELLANY:
        switch (merc.sub_type)
        {
        case MISC_HORN_OF_GERYON:
        case MISC_ZIGGURAT:
            valued += 5000;
            break;

        case MISC_FAN_OF_GALES:
        case MISC_PHIAL_OF_FLOODS:
        case MISC_LAMP_OF_FIRE:
        case MISC_LIGHTNING_ROD:
        case MISC_TIN_OF_TREMORSTONES:
            valued += 400;
            break;

        case MISC_PHANTOM_MIRROR:
            valued += 300;
            break;

        case MISC_BOX_OF_BEASTS:
	    case MISC_DISC_OF_STORMS:
        case MISC_SACK_OF_SPIDERS:
            valued += 200;
            break;

        default:
            valued += 200;
        }
        break;

    case OBJ_BOOKS:
    {
        valued = 150;
        const book_type book = static_cast<book_type>(merc.sub_type);
#if TAG_MAJOR_VERSION == 34
        if (book == BOOK_BUGGY_DESTRUCTION)
            break;
#endif

        if (merc_type_known(merc))
        {
            double rarity = 0;
            if (is_random_artefact(merc))
            {
                const vector<spell_type>& spells = spells_in_book(merc);

                int rarest = 0;
                for (spell_type spell : spells)
                {
                    rarity += spell_rarity(spell);
                    if (spell_rarity(spell) > rarest)
                        rarest = spell_rarity(spell);
                }
                rarity += rarest * 2;
                rarity /= spells.size();

                // Surcharge for large books.
                if (spells.size() > 6)
                    rarity *= spells.size() / 6;

            }
            else
                rarity = book_rarity(book);

            valued += (int)(rarity * 50.0);
        }
        break;
    }

    case OBJ_STAVES:
        valued = merc_type_known(merc) ? 250 : 120;
        break;
    case OBJ_RODS:
        if (!merc_type_known(merc))
            valued = 120;
        else
            valued = 250;

        // Both max charges and enchantment.
        if (merc_ident(merc, ISFLAG_KNOW_PLUSES))
            valued += 50 * (merc.charge_cap / ROD_CHARGE_MULT + merc.rod_plus);
        break;
    case OBJ_ORBS:
        valued = 250000;
        break;

    case OBJ_RUNES:
        valued = 10000;
        break;

    default:
        break;
    }                           // end switch

    if (valued < 1)
        valued = 1;

    valued = stepdown_value(valued, 1000, 1000, 10000, 10000);

    return merc.quantity * valued;
}

bool is_worthless_consumable(const merc_def &merc)
{
    switch (merc.base_type)
    {
    case OBJ_POTIONS:
        switch (merc.sub_type)
        {
        // Blood potions are worthless because they are easy to make.
#if TAG_MAJOR_VERSION == 34
        case POT_BLOOD:
        case POT_BLOOD_COAGULATED:
        case POT_SLOWING:
        case POT_DECAY:
#endif
        case POT_POISON:
        case POT_DEGENERATION:
        case POT_WATER:
            return true;
        default:
            return false;
        }
    case OBJ_SCROLLS:
        switch (merc.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_WEAPON:
        case SCR_CURSE_JEWELLERY:
#endif
        case SCR_NOISE:
        case SCR_RANDOM_USELESSNESS:
            return true;
        default:
            return false;
        }

    // Only consumables are worthless.
    default:
        return false;
    }
}

static int _count_identical(const vector<merc_def>& stock, const merc_def& merc)
{
    int count = 0;
    for (const merc_def& other : stock)
        if (innpingList::mercs_are_same(merc, other))
            count++;

    return count;
}

/** Buy an merc from a inn!
 *
 *  @param inn  the inn to purchase from.
 *  @param pos   where the inn is located
 *  @param index the index of the merc to buy in inn.stock
 *  @returns true if it went in your inventory, false otherwise.
 */
static bool _purchase(inn_struct& inn, const level_pos& pos, int index)
{
    merc_def merc = inn.stock[index]; // intentional copy
    const int cost = merc_price(merc, inn);
    inn.stock.erase(inn.stock.begin() + index);

    // Remove from innping list if it's unique
    // (i.e., if the inn has multiple scrolls of
    // identify, don't remove the other scrolls
    // from the innping list if there's any
    // left).
    if (innping_list.is_on_list(merc, &pos)
        && _count_identical(inn.stock, merc) == 0)
    {
        innping_list.del_thing(merc, &pos);
    }

    // Take a note of the purchase.
    take_note(Note(NOTE_BUY_merc, cost, 0,
                   merc.name(DESC_A).c_str()));

    // But take no further similar notes.
    merc.flags |= ISFLAG_NOTED_GET;

    if (fully_identified(merc))
        merc.flags |= ISFLAG_NOTED_ID;

    you.del_gold(cost);

    you.attribute[ATTR_PURCHASES] += cost;

    origin_purchased(merc);

    if (inntype_identifies_stock(inn.type))
    {
        // Identify the merc and its type.
        // This also takes the ID note if necessary.
        set_ident_type(merc, true);
        set_ident_flags(merc, ISFLAG_IDENT_MASK);
    }

    // innkeepers will place goods you can't carry outside the inn.
    if (merc_is_stationary(merc)
        || !move_merc_to_inv(merc))
    {
        copy_merc_to_grid(merc, inn.pos);
        return false;
    }
    return true;
}

static string _hyphenated_letters(int how_many, char first)
{
    string s = "<w>";
    s += first;
    s += "</w>";
    if (how_many > 1)
    {
        s += "-<w>";
        s += first + how_many - 1;
        s += "</w>";
    }
    return s;
}

enum innping_order
{
    ORDER_DEFAULT,
    ORDER_PRICE,
    ORDER_ALPHABETICAL,
    ORDER_TYPE,
    NUM_ORDERS
};

static const char * const innping_order_names[NUM_ORDERS] =
{
    "default", "price", "name", "type"
};

static innping_order operator++(innping_order &x)
{
    x = static_cast<innping_order>(x + 1);
    if (x == NUM_ORDERS)
        x = ORDER_DEFAULT;
    return x;
}

class innMenu : public InvMenu
{
    friend class innEntry;

    inn_struct& inn;
    innping_order order = ORDER_DEFAULT;
    level_pos pos;
    bool can_purchase;

    int selected_cost() const;

    void init_entries();
    void update_help();
    void resort();
    void purchase_selected();

    virtual bool process_key(int keyin) override;

public:
    bool bought_something = false;

    innMenu(inn_struct& _inn, const level_pos& _pos, bool _can_purchase);
};

class innEntry : public InvEntry
{
    innMenu& menu;

    string get_text(bool need_cursor = false) const override
    {
        need_cursor = need_cursor && show_cursor;
        const int cost = merc_price(*merc, menu.inn);
        const int total_cost = menu.selected_cost();
        const bool on_list = innping_list.is_on_list(*merc, &menu.pos);
        // Colour stock as follows:
        //  * lightcyan, if on the innping list and not selected.
        //  * lightred, if you can't buy all you selected.
        //  * lightgreen, if this merc is purchasable along with your selections
        //  * red, if this merc is not purchasable even by itself.
        //  * yellow, if this merc would be purchasable if you deselected
        //            something else.

        // Is this too complicated? (jpeg)
        const colour_t keycol =
            !selected() && on_list              ? LIGHTCYAN :
            selected() && total_cost > you.gold ? LIGHTRED  :
            cost <= you.gold - total_cost       ? LIGHTGREEN :
            cost > you.gold                     ? RED :
                                                  YELLOW;
        const string keystr = colour_to_str(keycol);
        const string mercstr =
            colour_to_str(menu_colour(text, merc_prefix(*merc), tag));
        return make_stringf(" <%s>%c%c%c%c</%s><%s>%4d gold   %s%s</%s>",
                            keystr.c_str(),
                            hotkeys[0],
                            need_cursor ? '[' : ' ',
                            selected() ? '+' : on_list ? '$' : '-',
                            need_cursor ? ']' : ' ',
                            keystr.c_str(),
                            mercstr.c_str(),
                            cost,
                            text.c_str(),
                            inn_merc_unknown(*merc) ? " (unknown)" : "",
                            mercstr.c_str());
    }

    virtual void select(int qty = -1) override
    {
        if (innping_list.is_on_list(*merc, &menu.pos) && qty != 0)
            innping_list.del_thing(*merc, &menu.pos);

        InvEntry::select(qty);
    }
public:
    innEntry(const merc_def& i, innMenu& m)
        : InvEntry(i),
          menu(m)
    {
        show_background = false;
    }
};

innMenu::innMenu(inn_struct& _inn, const level_pos& _pos, bool _can_purchase)
    : InvMenu(MF_MULTISELECT | MF_NO_SELECT_QTY | MF_QUIET_SELECT
               | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING),
      inn(_inn),
      pos(_pos),
      can_purchase(_can_purchase)
{
    menu_action = can_purchase ? ACT_EXECUTE : ACT_EXAMINE;
    set_flags(get_flags() & ~MF_USE_TWO_COLUMNS);

    set_tag("inn");

    init_entries();

    update_help();

    set_title("Welcome to " + inn_name(inn) + "! What would you "
              "like to do?");
}

void innMenu::init_entries()
{
    menu_letter ckey = 'a';
    for (merc_def& merc : inn.stock)
    {
        auto newentry = make_unique<innEntry>(merc, *this);
        newentry->hotkeys.clear();
        newentry->add_hotkey(ckey++);
        add_entry(move(newentry));
    }
}

int innMenu::selected_cost() const
{
    int cost = 0;
    for (auto merc : selected_entries())
        cost += merc_price(*dynamic_cast<innEntry*>(merc)->merc, inn);
    return cost;
}

void innMenu::update_help()
{
    string top_line = make_stringf("<yellow>You have %d gold piece%s.",
                                   you.gold,
                                   you.gold != 1 ? "s" : "");
    const int total_cost = selected_cost();
    if (total_cost > you.gold)
    {
        top_line += "<lightred>";
        top_line +=
            make_stringf(" You are short %d gold piece%s for the purchase.",
                         total_cost - you.gold,
                         (total_cost - you.gold != 1) ? "s" : "");
        top_line += "</lightred>";
    }
    else if (total_cost)
    {
        top_line +=
            make_stringf(" After the purchase, you will have %d gold piece%s.",
                         you.gold - total_cost,
                         (you.gold - total_cost != 1) ? "s" : "");
    }
    top_line += "</yellow>";

    // Ensure length >= 80ch, which prevents the local tiles menu from resizing
    // as the player selects/deselects entries. Blegh..
    int top_line_width = strwidth(formatted_string::parse_string(top_line).tostring());
    top_line += string(max(0, 80 - top_line_width), ' ') + '\n';

    set_more(formatted_string::parse_string(top_line + make_stringf(
        //You have 0 gold pieces.
        //[Esc/R-Click] exit  [!] buy|examine mercs  [a-i] select merc for purchase
        //[/] sort (default)  [Enter] make purchase  [A-I] put merc on innping list
#if defined(USE_TILE) && !defined(TOUCH_UI)
        "[<w>Esc</w>/<w>R-Click</w>] exit  "
#else
        //               "/R-Click"
        "[<w>Esc</w>] exit          "
#endif
        "%s  [%s] %s\n"
        "[<w>/</w>] sort (%s)%s  %s  [%s] put merc on innping list",
        !can_purchase ? " " " "  "  " "       "  "          " :
        menu_action == ACT_EXECUTE ? "[<w>!</w>] <w>buy</w>|examine mercs" :
                                     "[<w>!</w>] buy|<w>examine</w> mercs",
        _hyphenated_letters(merc_count(), 'a').c_str(),
        menu_action == ACT_EXECUTE ? "select merc for purchase" : "examine merc",
        innping_order_names[order],
        // strwidth("default")
        string(7 - strwidth(innping_order_names[order]), ' ').c_str(),
        !can_purchase ? " " "     "  "               " :
                        "[<w>Enter</w>] make purchase",
        _hyphenated_letters(merc_count(), 'A').c_str())));
}

void innMenu::purchase_selected()
{
    bool buying_from_list = false;
    vector<MenuEntry*> selected = selected_entries();
    int cost = selected_cost();
    if (selected.empty())
    {
        ASSERT(cost == 0);
        buying_from_list = true;
        for (auto merc : mercs)
        {
            const merc_def& it = *dynamic_cast<innEntry*>(merc)->merc;
            if (innping_list.is_on_list(it, &pos))
            {
                selected.push_back(merc);
                cost += merc_price(it, inn, true);
            }
        }
    }
    if (selected.empty())
        return;
    const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
    update_help();
    const formatted_string old_more = more;
    if (cost > you.gold)
    {
        more = formatted_string::parse_string(make_stringf(
                   "<%s>You don't have enough money.</%s>\n",
                   col.c_str(),
                   col.c_str()));
        more += old_more;
        update_more();
        return;
    }
    more = formatted_string::parse_string(make_stringf(
               "<%s>Purchase mercs%s for %d gold? (y/N)</%s>\n",
               col.c_str(),
               buying_from_list ? " in innping list" : "",
               cost,
               col.c_str()));
    more += old_more;
    update_more();
    if (!yesno(nullptr, true, 'n', false, false, true))
    {
        more = old_more;
        update_more();
        return;
    }
    sort(begin(selected), end(selected),
         [](MenuEntry* a, MenuEntry* b)
         {
             return a->data > b->data;
         });
    vector<int> bought_indices;
    int outside_mercs = 0;

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with mercs purchased.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    // Will iterate backwards through the inn (because of the earlier sort).
    // This means we can erase() from inn.stock (since it only invalidates
    // pointers to later elements), but nothing else.
    for (auto entry : selected)
    {
        const int i = static_cast<merc_def*>(entry->data) - inn.stock.data();
        merc_def& merc(inn.stock[i]);
        // Can happen if the price changes due to id status
        if (merc_price(merc, inn) > you.gold)
            continue;
        const int quant = merc.quantity;

        if (!_purchase(inn, pos, i))
        {
            // The purchased merc didn't fit into your
            // knapsack.
            outside_mercs += quant;
        }

        bought_indices.push_back(i);
        bought_something = true;
    }

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    // Since the old innEntrys may now point to past the end of inn.stock (or
    // just the wrong place in general) nuke the whole thing and start over.
    deleteAll(mercs);
    init_entries();
    resort();

    if (outside_mercs)
    {
        update_help();
        const formatted_string next_more = more;
        more = formatted_string::parse_string(make_stringf(
            "<%s>I'll put %s outside for you.</%s>\n",
            col.c_str(),
            bought_indices.size() == 1             ? "it" :
      (int) bought_indices.size() == outside_mercs ? "them"
                                                   : "some of them",
            col.c_str()));
        more += next_more;
        update_more();
    }
    else
        update_help();

    update_menu(true);
}

// Doesn't handle redrawing itself.
void innMenu::resort()
{
    switch (order)
    {
    case ORDER_DEFAULT:
        sort(begin(mercs), end(mercs),
             [](MenuEntry* a, MenuEntry* b)
             {
                 return a->data < b->data;
             });
        break;
    case ORDER_PRICE:
        sort(begin(mercs), end(mercs),
             [this](MenuEntry* a, MenuEntry* b)
             {
                 return merc_price(*dynamic_cast<innEntry*>(a)->merc, inn)
                        < merc_price(*dynamic_cast<innEntry*>(b)->merc, inn);
             });
        break;
    case ORDER_ALPHABETICAL:
        sort(begin(mercs), end(mercs),
             [this](MenuEntry* a, MenuEntry* b) -> bool
             {
                 const bool id = inntype_identifies_stock(inn.type);
                 return dynamic_cast<innEntry*>(a)->merc->name(DESC_PLAIN, false, id)
                        < dynamic_cast<innEntry*>(b)->merc->name(DESC_PLAIN, false, id);
             });
        break;
    case ORDER_TYPE:
        sort(begin(mercs), end(mercs),
             [](MenuEntry* a, MenuEntry* b) -> bool
             {
                 const auto ai = dynamic_cast<innEntry*>(a)->merc;
                 const auto bi = dynamic_cast<innEntry*>(b)->merc;
                 if (ai->base_type == bi->base_type)
                     return ai->sub_type < bi->sub_type;
                 else
                     return ai->base_type < bi->base_type;
             });
        break;
    case NUM_ORDERS:
        die("invalid ordering");
    }
    for (size_t i = 0; i < mercs.size(); ++i)
        mercs[i]->hotkeys[0] = index_to_letter(i);
}

bool innMenu::process_key(int keyin)
{
    switch (keyin)
    {
    case '!':
    case '?':
        if (can_purchase)
        {
            if (menu_action == ACT_EXECUTE)
                menu_action = ACT_EXAMINE;
            else
                menu_action = ACT_EXECUTE;
            update_help();
            update_more();
        }
        return true;
    case ' ':
    case CK_MOUSE_CLICK:
    case CK_ENTER:
        if (can_purchase)
            purchase_selected();
        return true;
    case '$':
    {
        const vector<MenuEntry*> selected = selected_entries();
        if (!selected.empty())
        {
            // Move selected to innping list.
            for (auto entry : selected)
            {
                const merc_def& merc = *dynamic_cast<innEntry*>(entry)->merc;
                entry->selected_qty = 0;
                if (!innping_list.is_on_list(merc, &pos))
                    innping_list.add_thing(merc, merc_price(merc, inn), &pos);
            }
        }
        else
            // Move innlist to selection.
            for (auto entry : mercs)
                if (innping_list.is_on_list(*dynamic_cast<innEntry*>(entry)->merc, &pos))
                    entry->select(-2);
        // Move innlist to selection.
        update_menu(true);
        return true;
    }
    case '/':
        ++order;
        resort();
        update_help();
        update_menu(true);
        return true;
    default:
        break;
    }

    if (keyin - 'a' >= 0 && keyin - 'a' < (int)mercs.size()
        && menu_action == ACT_EXAMINE)
    {
        merc_def& merc(*const_cast<merc_def*>(dynamic_cast<innEntry*>(
            mercs[letter_to_index(keyin)])->merc));
        // A hack to make the description more useful.
        // In theory, the user could kill the process at this
        // point and end up with valid ID for the merc.
        // That's not very useful, though, because it doesn't set
        // type-ID and once you can access the merc (by buying it)
        // you have its full ID anyway. Worst case, it won't get
        // noted when you buy it.
        {
            unwind_var<iflags_t> old_flags(merc.flags);
            if (inntype_identifies_stock(inn.type))
            {
                merc.flags |= (ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID
                               | ISFLAG_NOTED_GET);
            }
            describe_merc(merc);
        }
        return true;
    }
    else if (keyin - 'A' >= 0 && keyin - 'A' < (int)mercs.size())
    {
        const auto index = letter_to_index(keyin) % 26;
        auto entry = dynamic_cast<innEntry*>(mercs[index]);
        entry->selected_qty = 0;
        const merc_def& merc(*entry->merc);
        if (innping_list.is_on_list(merc, &pos))
            innping_list.del_thing(merc, &pos);
        else
            innping_list.add_thing(merc, merc_price(merc, inn), &pos);
        update_menu(true);
        return true;
    }

    auto old_selected = selected_entries();
    bool ret = InvMenu::process_key(keyin);
    if (old_selected != selected_entries())
    {
        // Update the footer to display the new $$$ info.
        update_help();
        update_menu(true);
    }
    return ret;
}

void inn()
{
    if (!inn_at(you.pos()))
    {
        mprf(MSGCH_ERROR, "Help! Non-existent inn.");
        return;
    }

    inn_struct& inn = *inn_at(you.pos());
    const string innname = inn_name(inn);

    // Quick out, if no inventory
    if (inn.stock.empty())
    {
        mprf("%s appears to be closed.", innname.c_str());
        destroy_inn_at(you.pos());
        return;
    }

    bool culled = false;
    for (const auto& merc : inn.stock)
    {
        const int cost = merc_price(merc, inn, true);
        culled |= innping_list.cull_identical_mercs(merc, cost);
    }
    if (culled)
        more(); // make sure all messages appear before menu

    innMenu menu(inn, level_pos::current(), true);
    menu.show();

    StashTrack.get_inn(inn.pos) = innInfo(inn);
    bool any_on_list = any_of(begin(inn.stock), end(inn.stock),
                              [](const merc_def& merc)
                              {
                                  return innping_list.is_on_list(merc);
                              });

    // If the inn is now empty, erase it from the overview.
    if (inn.stock.empty())
        destroy_inn_at(you.pos());
    redraw_screen();
    if (menu.bought_something)
        mprf("Thank you for innping at %s!", innname.c_str());
    if (any_on_list)
        mpr("You can access your innping list by pressing '$'.");
}

void inn(inn_struct& inn, const level_pos& pos)
{
    ASSERT(inn.pos == pos.pos);
    innMenu(inn, pos, false).show();
}

void destroy_inn_at(coord_def p)
{
    if (inn_at(p))
    {
        env.inn.erase(p);
        grd(p) = DNGN_ABANDONED_inn;
        unnotice_feature(level_pos(level_id::current(), p));
    }
}

inn_struct *inn_at(const coord_def& where)
{
    if (grd(where) != DNGN_ENTER_inn)
        return nullptr;

    auto it = env.inn.find(where);
    ASSERT(it != env.inn.end());
    ASSERT(it->second.pos == where);
    ASSERT(it->second.type != inn_UNASSIGNED);

    return &it->second;
}

string inn_type_name(inn_type type)
{
    switch (type)
    {
        case inn_WEAPON_ANTIQUE:
            return "Antique Weapon";
        case inn_ARMOUR_ANTIQUE:
            return "Antique Armour";
        case inn_WEAPON:
            return "Weapon";
        case inn_ARMOUR:
            return "Armour";
        case inn_JEWELLERY:
            return "Jewellery";
        case inn_EVOKABLES:
            return "Gadget";
        case inn_BOOK:
            return "Book";
        case inn_FOOD:
            return "Food";
        case inn_SCROLL:
            return "Magic Scroll";
        case inn_GENERAL_ANTIQUE:
            return "Assorted Antiques";
        case inn_DISTILLERY:
            return "Distillery";
        case inn_GENERAL:
            return "General Store";
        default:
            return "Bug";
    }
}

static const char *_inn_type_suffix(inn_type type, const coord_def &where)
{
    if (type == inn_GENERAL
        || type == inn_GENERAL_ANTIQUE
        || type == inn_DISTILLERY)
    {
        return "";
    }

    static const char * const suffixnames[] =
    {
        "innpe", "Boutique", "Emporium", "inn"
    };
    return suffixnames[(where.x + where.y) % ARRAYSZ(suffixnames)];
}

string inn_name(const inn_struct& inn)
{
    const inn_type type = inn.type;

    string sh_name = "";

#if TAG_MAJOR_VERSION == 34
    // xref innInfo::load
    if (inn.inn_name == " ")
        return inn.inn_type_name;
#endif
    if (!inn.inn_name.empty())
        sh_name += apostrophise(inn.inn_name) + " ";
    else
    {
        uint32_t seed = static_cast<uint32_t>(inn.keeper_name[0])
            | (static_cast<uint32_t>(inn.keeper_name[1]) << 8)
            | (static_cast<uint32_t>(inn.keeper_name[1]) << 16);

        sh_name += apostrophise(make_name(seed)) + " ";
    }

    if (!inn.inn_type_name.empty())
        sh_name += inn.inn_type_name;
    else
        sh_name += inn_type_name(type);

    if (!inn.inn_suffix_name.empty())
        sh_name += " " + inn.inn_suffix_name;
    else
    {
        string sh_suffix = _inn_type_suffix(type, inn.pos);
        if (!sh_suffix.empty())
            sh_name += " " + sh_suffix;
    }

    return sh_name;
}

bool is_inn_merc(const merc_def &merc)
{
    return merc.link == merc_IN_inn;
}

bool inntype_identifies_stock(inn_type type)
{
    return type != inn_WEAPON_ANTIQUE
           && type != inn_ARMOUR_ANTIQUE
           && type != inn_GENERAL_ANTIQUE;
}

bool inn_merc_unknown(const merc_def &merc)
{
    return merc_type_has_ids(merc.base_type)
           && merc_type_known(merc)
           && !get_ident_type(merc)
           && !is_artefact(merc);
}

static const char *inn_types[] =
{
    "weapon",
    "armour",
    "antique weapon",
    "antique armour",
    "antiques",
    "jewellery",
    "gadget",
    "book",
    "food",
    "distillery",
    "scroll",
    "general",
};

/** What inn type is this?
 *
 *  @param s the inn type, in a string.
 *  @returns the corresponding enum, or inn_UNASSIGNED if none.
 */
inn_type str_to_inntype(const string &s)
{
    if (s == "random" || s == "any")
        return inn_RANDOM;

    for (size_t i = 0; i < ARRAYSZ(inn_types); ++i)
        if (s == inn_types[i])
            return static_cast<inn_type>(i);

    return inn_UNASSIGNED;
}

const char *inntype_to_str(inn_type type)
{
    return inn_types[type];
}

void list_inn_types()
{
    mpr_nojoin(MSGCH_PLAIN, "Available inn types: ");
    for (const char *type : inn_types)
        mprf_nocap("%s", type);
}

////////////////////////////////////////////////////////////////////////

// TODO:
//   * Warn if buying something not on the innping list would put
//     something on innping list out of your reach.

#define innPING_LIST_KEY       "innping_list_key"
#define innPING_LIST_COST_KEY  "innping_list_cost_key"
#define innPING_THING_COST_KEY "cost_key"
#define innPING_THING_merc_KEY "merc_key"
#define innPING_THING_DESC_KEY "desc_key"
#define innPING_THING_VERB_KEY "verb_key"
#define innPING_THING_POS_KEY  "pos_key"

innpingList::innpingList()
    : list(nullptr), min_unbuyable_cost(0), min_unbuyable_idx(0),
      max_buyable_cost(0), max_buyable_idx(0)
{
}

#define SETUP_POS()                 \
    ASSERT(list); \
    level_pos pos;                  \
    if (_pos != nullptr)            \
        pos = *_pos;                \
    else                            \
        pos = level_pos::current(); \
    ASSERT(pos.is_valid());

bool innpingList::add_thing(const merc_def &merc, int cost,
                             const level_pos* _pos)
{
    ASSERT(merc.defined());
    ASSERT(cost > 0);

    SETUP_POS();

    if (!find_thing(merc, pos).empty()) // TODO: this check isn't working?
    {
        mprf(MSGCH_ERROR, "%s is already on the innping list.",
             merc.name(DESC_THE).c_str());
        return false;
    }

    CrawlHashTable *thing = new CrawlHashTable();
    (*thing)[innPING_THING_COST_KEY] = cost;
    (*thing)[innPING_THING_POS_KEY]  = pos;
    (*thing)[innPING_THING_merc_KEY] = merc;
    list->push_back(*thing);
    refresh();

    return true;
}

bool innpingList::is_on_list(const merc_def &merc, const level_pos* _pos) const
{
    SETUP_POS();

    return !find_thing(merc, pos).empty();
}

bool innpingList::is_on_list(string desc, const level_pos* _pos) const
{
    SETUP_POS();

    return !find_thing(desc, pos).empty();
}

void innpingList::del_thing_at_index(int idx)
{
    ASSERT_RANGE(idx, 0, list->size());
    list->erase(idx);
    refresh();
}

template <typename C>
void innpingList::del_thing_at_indices(C const &idxs)
{
    set<int,greater<int>> indices(idxs.begin(), idxs.end());

    for (auto idx : indices)
    {
        ASSERT_RANGE(idx, 0, list->size());
        list->erase(idx);
    }
    refresh();
}

void innpingList::del_things_from(const level_id &lid)
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing).is_on(lid))
            list->erase(i--);
    }
    refresh();
}

bool innpingList::del_thing(const merc_def &merc,
                             const level_pos* _pos)
{
    SETUP_POS();

    auto indices = find_thing(merc, pos);

    if (indices.empty())
    {
        mprf(MSGCH_ERROR, "%s isn't on innping list, can't delete it.",
             merc.name(DESC_THE).c_str());
        return false;
    }

    del_thing_at_indices(indices);
    return true;
}

bool innpingList::del_thing(string desc, const level_pos* _pos)
{
    SETUP_POS();

    auto indices = find_thing(desc, pos);

    if (indices.empty())
    {
        mprf(MSGCH_ERROR, "%s isn't on innping list, can't delete it.",
             desc.c_str());
        return false;
    }

    del_thing_at_indices(indices);
    return true;
}

#undef SETUP_POS

#define REMOVE_PROMPTED_KEY  "remove_prompted_key"
#define REPLACE_PROMPTED_KEY "replace_prompted_key"

// TODO:
//
// * If you get a randart which lets you turn invisible, then remove
//   any ordinary rings of invisibility from the innping list.
//
// * If you collected enough spellbooks that all the spells in a
//   innping list book are covered, then auto-remove it.
bool innpingList::cull_identical_mercs(const merc_def& merc, int cost)
{
    // Dead men can't update their innping lists.
    if (!crawl_state.need_save)
        return 0;

    // Can't put mercs in Bazaar inns in the innping list, so
    // don't bother transferring innping list mercs to Bazaar inns.
    // TODO: temporary innlists
    if (cost != -1 && !is_connected_branch(you.where_are_you))
        return 0;

    switch (merc.base_type)
    {
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_STAVES:
        // Only these are really interchangeable.
        break;
    case OBJ_MISCELLANY:
        // ... and a few of these.
        switch (merc.sub_type)
        {
            case MISC_CRYSTAL_BALL_OF_ENERGY:
            case MISC_DISC_OF_STORMS:
                break;
            default:
                if (!is_xp_evoker(merc))
                    return 0;
        }
        break;
    default:
        return 0;
    }

    if (!merc_type_known(merc) || is_artefact(merc))
        return 0;

    // Ignore stat-modification rings which reduce a stat, since they're
    // worthless.
    if (merc.base_type == OBJ_JEWELLERY && merc.plus < 0)
        return 0;

    // Manuals are consumable, and interesting enough to keep on list.
    if (merc.is_type(OBJ_BOOKS, BOOK_MANUAL))
        return 0;

    // merc is already on innping-list.
    const bool on_list = !find_thing(merc, level_pos::current()).empty();

    const bool do_prompt = merc.base_type == OBJ_JEWELLERY
                           && !jewellery_is_amulet(merc)
                           && ring_has_stackable_effect(merc);

    bool add_merc = false;

    typedef pair<merc_def, level_pos> list_pair;
    vector<list_pair> to_del;

    // NOTE: Don't modify the innping list while iterating over it.
    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_merc(thing))
            continue;

        const merc_def& list_merc = get_thing_merc(thing);

        if (list_merc.base_type != merc.base_type
            || list_merc.sub_type != merc.sub_type)
        {
            continue;
        }

        if (!merc_type_known(list_merc) || is_artefact(list_merc))
            continue;

        // Don't prompt to remove rings with strictly better pluses
        // than the new one. Also, don't prompt to remove rings with
        // known pluses when the new ring's pluses are unknown.
        if (merc.base_type == OBJ_JEWELLERY)
        {
            const bool has_plus = jewellery_has_pluses(merc);
            const int delta_p = merc.plus - list_merc.plus;
            if (has_plus
                && merc_ident(list_merc, ISFLAG_KNOW_PLUSES)
                && (!merc_ident(merc, ISFLAG_KNOW_PLUSES)
                     || delta_p < 0))
            {
                continue;
            }
        }

        // Don't prompt to remove known manuals when the new one is unknown
        // or for a different skill.
        if (merc.is_type(OBJ_BOOKS, BOOK_MANUAL)
            && merc_type_known(list_merc)
            && (!merc_type_known(merc) || merc.plus != list_merc.plus))
        {
            continue;
        }

        list_pair listed(list_merc, thing_pos(thing));

        // cost = -1, we just found a inn merc which is cheaper than
        // one on the innping list.
        if (cost != -1)
        {
            int list_cost = thing_cost(thing);

            if (cost >= list_cost)
                continue;

            // Only prompt once.
            if (thing.exists(REPLACE_PROMPTED_KEY))
                continue;
            thing[REPLACE_PROMPTED_KEY] = (bool) true;

            string prompt =
                make_stringf("innping list: replace %dgp %s with cheaper "
                             "one? (Y/n)", list_cost,
                             describe_thing(thing).c_str());

            if (yesno(prompt.c_str(), true, 'y', false))
            {
                add_merc = true;
                to_del.push_back(listed);
            }
            continue;
        }

        // cost == -1, we just got an merc which is on the innping list.
        if (do_prompt)
        {
            // Only prompt once.
            if (thing.exists(REMOVE_PROMPTED_KEY))
                continue;
            thing[REMOVE_PROMPTED_KEY] = (bool) true;

            string prompt = make_stringf("innping list: remove %s? (Y/n)",
                                         describe_thing(thing, DESC_A).c_str());

            if (yesno(prompt.c_str(), true, 'y', false))
            {
                to_del.push_back(listed);
                mprf("innping list: removing %s",
                     describe_thing(thing, DESC_A).c_str());
            }
            else
                canned_msg(MSG_OK);
        }
        else
        {
            mprf("innping list: removing %s",
                 describe_thing(thing, DESC_A).c_str());
            to_del.push_back(listed);
        }
    }

    for (list_pair &entry : to_del)
        del_thing(entry.first, &entry.second);

    if (add_merc && !on_list)
        add_thing(merc, cost);

    return to_del.size();
}

void innpingList::merc_type_identified(object_class_type base_type,
                                        int sub_type)
{
    // Dead men can't update their innping lists.
    if (!crawl_state.need_save)
        return;

    // Only restore the excursion at the very end.
    level_excursion le;

#if TAG_MAJOR_VERSION == 34
    // Handle removed Gozag inns from old saves. Only do this once:
    // future Gozag abandonment will call remove_dead_inns itself.
    if (!you.props.exists(REMOVED_DEAD_innS_KEY))
    {
        remove_dead_inns();
        you.props[REMOVED_DEAD_innS_KEY] = true;
    }
#endif

    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_merc(thing))
            continue;

        const merc_def& merc = get_thing_merc(thing);

        if (merc.base_type != base_type || merc.sub_type != sub_type)
            continue;

        const level_pos place = thing_pos(thing);

        le.go_to(place.id);
        const inn_struct *inn = inn_at(place.pos);
        ASSERT(inn);
        if (inntype_identifies_stock(inn->type))
            continue;

        thing[innPING_THING_COST_KEY] =
            _inn_get_merc_value(merc, inn->greed, false, true);
    }

    // Prices could have changed.
    refresh();
}

void innpingList::spells_added_to_library(const vector<spell_type>& spells, bool quiet)
{
    if (!list) /* let's not make book starts crash instantly... */
        return;

    unordered_set<int> indices_to_del;
    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_merc(thing)) // ???
            continue;
        const merc_def& book = get_thing_merc(thing);
        if (book.base_type != OBJ_BOOKS || book.sub_type == BOOK_MANUAL)
            continue;

        const auto merc_spells = spells_in_book(book);
        if (any_of(merc_spells.begin(), merc_spells.end(), [&spells](const spell_type st) {
                    return find(spells.begin(), spells.end(), st) != spells.end();
                }) && is_useless_merc(book, false))
        {
            level_pos pos = thing_pos(thing); // TODO: unreliable?
            auto thing_indices = find_thing(get_thing_merc(thing), pos);
            indices_to_del.insert(thing_indices.begin(), thing_indices.end());
        }
    }
    if (!quiet)
    {
        for (auto idx : indices_to_del)
        {
            ASSERT_RANGE(idx, 0, list->size());
            mprf("innping list: removing %s",
                describe_thing((*list)[idx], DESC_A).c_str());
        }
    }
    del_thing_at_indices(indices_to_del);
}

void innpingList::remove_dead_inns()
{
    // Only restore the excursion at the very end.
    level_excursion le;

    set<level_pos> inns_to_remove;

    for (CrawlHashTable &thing : *list)
    {
        const level_pos place = thing_pos(thing);
        le.go_to(place.id); // thereby running DACT_REMOVE_GOZAG_innS
        const inn_struct *inn = inn_at(place.pos);

        if (!inn)
            inns_to_remove.insert(place);
    }

    for (auto pos : inns_to_remove)
        forget_pos(pos);

    // Prices could have changed.
    refresh();
}

vector<innlist_entry> innpingList::entries()
{
    ASSERT(list);

    vector<innlist_entry> list_entries;
    for (const CrawlHashTable &entry : *list)
    {
        list_entries.push_back(
            make_pair(name_thing(entry), thing_cost(entry))
        );
    }

    return list_entries;
}

int innpingList::size() const
{
    ASSERT(list);

    return list->size();
}

bool innpingList::mercs_are_same(const merc_def& merc_a,
                                  const merc_def& merc_b)
{
    return merc_name_simple(merc_a) == merc_name_simple(merc_b);
}

void innpingList::move_things(const coord_def &_src, const coord_def &_dst)
{
    if (crawl_state.map_stat_gen
        || crawl_state.obj_stat_gen
        || crawl_state.test)
    {
        return; // innping list is unitialized and uneeded.
    }

    const level_pos src(level_id::current(), _src);
    const level_pos dst(level_id::current(), _dst);

    for (CrawlHashTable &thing : *list)
        if (thing_pos(thing) == src)
            thing[innPING_THING_POS_KEY] = dst;
}

void innpingList::forget_pos(const level_pos &pos)
{
    if (!crawl_state.need_save)
        return; // innping list is uninitialized and unneeded.

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing) == pos)
        {
            list->erase(i);
            i--;
        }
    }

    // Maybe we just forgot about a inn.
    refresh();
}

void innpingList::gold_changed(int old_amount, int new_amount)
{
    ASSERT(list);

    if (new_amount > old_amount && new_amount >= min_unbuyable_cost)
    {
        ASSERT(min_unbuyable_idx < list->size());

        vector<string> descs;
        for (unsigned int i = min_unbuyable_idx; i < list->size(); i++)
        {
            const CrawlHashTable &thing = (*list)[i];
            const int            cost   = thing_cost(thing);

            if (cost > new_amount)
            {
                ASSERT(i > (unsigned int) min_unbuyable_idx);
                break;
            }

            string desc;

            if (thing.exists(innPING_THING_VERB_KEY))
                desc += thing[innPING_THING_VERB_KEY].get_string();
            else
                desc = "buy";
            desc += " ";

            desc += describe_thing(thing, DESC_A);

            descs.push_back(desc);
        }
        ASSERT(!descs.empty());

        mpr_comma_separated_list("You now have enough gold to ", descs,
                                 ", or ");
        mpr("You can access your innping list by pressing '$'.");

        // Our gold has changed, maybe we can buy different things now.
        refresh();
    }
    else if (new_amount < old_amount && new_amount < max_buyable_cost)
    {
        // As above.
        refresh();
    }
}

class innpingListMenu : public Menu
{
public:
    innpingListMenu() : Menu(MF_MULTISELECT | MF_ALLOW_FORMATTING) {}
    bool view_only {false};

protected:
    virtual formatted_string calc_title() override;
};

formatted_string innpingListMenu::calc_title()
{
    formatted_string fs;
    const int total_cost = you.props[innPING_LIST_COST_KEY];

    fs.textcolour(title->colour);
    fs.cprintf("%d %s%s, total %d gold",
                title->quantity, title->text.c_str(),
                title->quantity > 1 ? "s" : "",
                total_cost);

    string s = "<lightgrey>  [<w>a-z</w>] ";

    if (view_only)
    {
        fs += formatted_string::parse_string(s + "<w>examine</w>");
        return fs;
    }

    switch (menu_action)
    {
    case ACT_EXECUTE:
        s += "<w>travel</w>|examine|delete";
        break;
    case ACT_EXAMINE:
        s += "travel|<w>examine</w>|delete";
        break;
    default:
        s += "travel|examine|<w>delete</w>";
        break;
    }

    s += "  [<w>?</w>/<w>!</w>] change action</lightgrey>";
    fs += formatted_string::parse_string(s);
    return fs;
}

/**
 * Describe the location of a given innping list entry.
 *
 * @param thing     A innping list entry.
 * @return          Something like [Orc:4], probably.
 */
string innpingList::describe_thing_pos(const CrawlHashTable &thing)
{
    return make_stringf("[%s]", thing_pos(thing).id.describe().c_str());
}

void innpingList::fill_out_menu(Menu& innmenu)
{
    menu_letter hotkey;
    int longest = 0;
    // How much space does the longest entry need for proper alignment?
    for (const CrawlHashTable &thing : *list)
        longest = max(longest, strwidth(describe_thing_pos(thing)));

    for (CrawlHashTable &thing : *list)
    {
        int cost = thing_cost(thing);
        const bool unknown = thing_is_merc(thing)
                             && inn_merc_unknown(get_thing_merc(thing));

        if (you.duration[DUR_BARGAIN])
            cost = _bargain_cost(cost);

        const string etitle =
            make_stringf(
                "%*s%5d gold  %s%s",
                longest,
                describe_thing_pos(thing).c_str(),
                cost,
                name_thing(thing, DESC_A).c_str(),
                unknown ? " (unknown)" : "");

        MenuEntry *me = new MenuEntry(etitle, MEL_merc, 1, hotkey);
        me->data = &thing;

        if (thing_is_merc(thing))
        {
            // Colour innping list merc according to menu colours.
            const merc_def &merc = get_thing_merc(thing);

            const string colprf = merc_prefix(merc);
            const int col = menu_colour(merc.name(DESC_A),
                                        colprf, "inn");

#ifdef USE_TILE
            vector<tile_def> merc_tiles;
            get_tiles_for_merc(merc, merc_tiles, true);
            for (const auto &tile : merc_tiles)
                me->add_tile(tile);
#endif

            if (col != -1)
                me->colour = col;
        }
        if (cost > you.gold)
            me->colour = DARKGREY;

        innmenu.add_entry(me);
        ++hotkey;
    }
}

void innpingList::display(bool view_only)
{
    if (list->empty())
        return;

    innpingListMenu innmenu;
    innmenu.view_only = view_only;
    innmenu.set_tag("inn");
    innmenu.menu_action  = view_only ? Menu::ACT_EXAMINE : Menu::ACT_EXECUTE;
    innmenu.action_cycle = view_only ? Menu::CYCLE_NONE : Menu::CYCLE_CYCLE;
    string title          = "merc";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    mtitle->quantity = list->size();
    innmenu.set_title(mtitle);

    string more_str = make_stringf("<yellow>You have %d gp</yellow>", you.gold);
    innmenu.set_more(formatted_string::parse_string(more_str));

    innmenu.set_flags(MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE
                        | MF_ALLOW_FORMATTING);

    fill_out_menu(innmenu);

    innmenu.on_single_selection =
        [this, &innmenu, &mtitle](const MenuEntry& sel)
    {
        const CrawlHashTable* thing =
            static_cast<const CrawlHashTable *>(sel.data);

        const bool is_merc = thing_is_merc(*thing);

        if (innmenu.menu_action == Menu::ACT_EXECUTE)
        {
            int cost = thing_cost(*thing);
            if (you.duration[DUR_BARGAIN])
                cost = _bargain_cost(cost);

            if (cost > you.gold)
            {
                string prompt =
                   make_stringf("You cannot afford %s; travel there "
                                "anyway? (y/N)",
                                describe_thing(*thing, DESC_A).c_str());
                clrscr();
                if (!yesno(prompt.c_str(), true, 'n'))
                    return true;
            }

            const level_pos lp(thing_pos(*thing));
            start_translevel_travel(lp);
            return false;
        }
        else if (innmenu.menu_action == Menu::ACT_EXAMINE)
        {
            if (is_merc)
            {
                const merc_def &merc = get_thing_merc(*thing);
                describe_merc(const_cast<merc_def&>(merc));
            }
            else // not an merc, so we only stored a description.
            {
                // HACK: Assume it's some kind of portal vault.
                const string info = make_stringf(
                             "%s with an entry fee of %d gold pieces.",
                             describe_thing(*thing, DESC_A).c_str(),
                             (int) thing_cost(*thing));
                show_description(info.c_str());
            }
        }
        else if (innmenu.menu_action == Menu::ACT_MISC)
        {
            const int index = innmenu.get_entry_index(&sel);
            if (index == -1)
            {
                mprf(MSGCH_ERROR, "ERROR: Unable to delete thing from innping list!");
                more();
                return true;
            }

            del_thing_at_index(index);
            mtitle->quantity = this->list->size();
            innmenu.set_title(mtitle);

            if (this->list->empty())
            {
                mpr("Your innping list is now empty.");
                return false;
            }

            innmenu.clear();
            fill_out_menu(innmenu);
            innmenu.update_menu(true);
        }
        else
            die("Invalid menu action type");
        return true;
    };

    innmenu.show();
    redraw_screen();
}

static bool _compare_innping_things(const CrawlStoreValue& a,
                                     const CrawlStoreValue& b)
{
    const CrawlHashTable& hash_a = a.get_table();
    const CrawlHashTable& hash_b = b.get_table();

    const int a_cost = hash_a[innPING_THING_COST_KEY];
    const int b_cost = hash_b[innPING_THING_COST_KEY];

    const level_id id_a = hash_a[innPING_THING_POS_KEY].get_level_pos().id;
    const level_id id_b = hash_b[innPING_THING_POS_KEY].get_level_pos().id;

    // Put Bazaar mercs at the top of the innping list.
    if (!player_in_branch(BRANCH_BAZAAR) || id_a.branch == id_b.branch)
        return a_cost < b_cost;
    else
        return id_a.branch == BRANCH_BAZAAR;
}

// Reset max_buyable and min_unbuyable info. Call this anytime any of the
// player's gold, the innping list, and the prices of the mercs on it
// change.
void innpingList::refresh()
{
    if (!you.props.exists(innPING_LIST_KEY))
        you.props[innPING_LIST_KEY].new_vector(SV_HASH, SFLAG_CONST_TYPE);
    list = &you.props[innPING_LIST_KEY].get_vector();

    stable_sort(list->begin(), list->end(), _compare_innping_things);

    min_unbuyable_cost = INT_MAX;
    min_unbuyable_idx  = -1;
    max_buyable_cost   = -1;
    max_buyable_idx    = -1;

    int total_cost = 0;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        const int cost = thing_cost(thing);

        if (cost <= you.gold)
        {
            max_buyable_cost = cost;
            max_buyable_idx  = i;
        }
        else if (min_unbuyable_idx == -1)
        {
            min_unbuyable_cost = cost;
            min_unbuyable_idx  = i;
        }
        total_cost += cost;
    }
    you.props[innPING_LIST_COST_KEY].get_int() = total_cost;
}

unordered_set<int> innpingList::find_thing(const merc_def &merc,
                             const level_pos &pos) const
{
    unordered_set<int> result;
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos) // TODO: using thing_pos above seems to make this unreliable?
            continue;

        if (!thing_is_merc(thing))
            continue;

        const merc_def &_merc = get_thing_merc(thing);

        if (merc_name_simple(merc) == merc_name_simple(_merc))
            result.insert(i);
    }

    return result;
}

unordered_set<int> innpingList::find_thing(const string &desc, const level_pos &pos) const
{
    unordered_set<int> result;
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (thing_is_merc(thing))
            continue;

        if (desc == name_thing(thing))
            result.insert(i);
    }

    return result;
}

bool innpingList::thing_is_merc(const CrawlHashTable& thing)
{
    return thing.exists(innPING_THING_merc_KEY);
}

const merc_def& innpingList::get_thing_merc(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(innPING_THING_merc_KEY));

    const merc_def &merc = thing[innPING_THING_merc_KEY].get_merc();
    ASSERT(merc.defined());

    return merc;
}

string innpingList::get_thing_desc(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(innPING_THING_DESC_KEY));

    string desc = thing[innPING_THING_DESC_KEY].get_string();
    return desc;
}

int innpingList::thing_cost(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(innPING_THING_COST_KEY));
    return thing[innPING_THING_COST_KEY].get_int();
}

level_pos innpingList::thing_pos(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(innPING_THING_POS_KEY));
    return thing[innPING_THING_POS_KEY].get_level_pos();
}

string innpingList::name_thing(const CrawlHashTable& thing,
                                description_level_type descrip)
{
    if (thing_is_merc(thing))
    {
        const merc_def &merc = get_thing_merc(thing);
        return merc.name(descrip);
    }
    else
    {
        string desc = get_thing_desc(thing);
        return apply_description(descrip, desc);
    }
}

string innpingList::describe_thing(const CrawlHashTable& thing,
                                    description_level_type descrip)
{
    string desc = name_thing(thing, descrip) + " on ";

    const level_pos pos = thing_pos(thing);
    if (pos.id == level_id::current())
        desc += "this level";
    else
        desc += pos.id.describe();

    return desc;
}

// merc name without curse-status or inscription.
string innpingList::merc_name_simple(const merc_def& merc, bool ident)
{
    return merc.name(DESC_PLAIN, false, ident, false, false,
                     ISFLAG_KNOW_CURSE);
}
