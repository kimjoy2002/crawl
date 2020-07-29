/**
 * @file
 * @brief inn keeper functions.
**/

#pragma once

#include <unordered_set>

unsigned int merc_value(merc_def merc, bool ident = false);
// price of an merc if it were being sold in a given inn
int merc_price(const merc_def& merc, const inn_struct& inn, bool ignore_bargain = false);
// Return true if an merc is classified as a worthless consumable.
// Note that this does not take into account the player's condition:
// curse scrolls are worthless for everyone, most potions aren't worthless
// for mummies, etcetera.
bool is_worthless_consumable(const merc_def &merc);

void inn();
void inn(inn_struct& inn, const level_pos& pos);

inn_struct *inn_at(const coord_def& where);

void destroy_inn_at(coord_def p);

string inn_name(const inn_struct& inn);
string inn_type_name(inn_type type);

bool inntype_identifies_stock(inn_type type);

bool is_inn_merc(const merc_def &merc);
bool inn_merc_unknown(const merc_def &merc);

inn_type str_to_inntype(const string &s);
const char *inntype_to_str(inn_type type);
void list_inn_types();

/////////////////////////////////////////////////////////////////////

struct level_pos;
class  Menu;

typedef pair<string, int> innlist_entry;
class innpingList
{
public:
    innpingList();

    bool add_thing(const merc_def &merc, int cost,
                   const level_pos* pos = nullptr);

    bool is_on_list(const merc_def &merc, const level_pos* pos = nullptr) const;
    bool is_on_list(string desc, const level_pos* pos = nullptr) const;

    bool del_thing(const merc_def &merc, const level_pos* pos = nullptr);
    bool del_thing(string desc, const level_pos* pos = nullptr);

    void del_things_from(const level_id &lid);

    void merc_type_identified(object_class_type base_type, int sub_type);
    void spells_added_to_library(const vector<spell_type>& spells, bool quiet);
    bool cull_identical_mercs(const merc_def& merc, int cost = -1);
    void remove_dead_inns();

    void gold_changed(int old_amount, int new_amount);

    void move_things(const coord_def &src, const coord_def &dst);
    void forget_pos(const level_pos &pos);

    void display(bool view_only = false);

    void refresh();

    bool empty() const { return !list || list->empty(); };
    int size() const;

    vector<innlist_entry> entries();

    static bool mercs_are_same(const merc_def& merc_a,
                               const merc_def& merc_b);

private:
    // An alias for you.props[innPING_LIST_KEY], kept in sync by refresh()
    CrawlVector* list;

    int min_unbuyable_cost;
    int min_unbuyable_idx;
    int max_buyable_cost;
    int max_buyable_idx;

private:
    unordered_set<int> find_thing(const merc_def &merc, const level_pos &pos) const;
    unordered_set<int> find_thing(const string &desc, const level_pos &pos) const;
    void del_thing_at_index(int idx);
    template <typename C> void del_thing_at_indices(C const &idxs);


    void fill_out_menu(Menu& innmenu);

    static       bool        thing_is_merc(const CrawlHashTable& thing);
    static const merc_def&   get_thing_merc(const CrawlHashTable& thing);
    static       string get_thing_desc(const CrawlHashTable& thing);

    static int       thing_cost(const CrawlHashTable& thing);
    static level_pos thing_pos(const CrawlHashTable& thing);
    static string    describe_thing_pos(const CrawlHashTable& thing);

    static string name_thing(const CrawlHashTable& thing,
                             description_level_type descrip = DESC_PLAIN);
    static string describe_thing(const CrawlHashTable& thing,
                                 description_level_type descrip = DESC_PLAIN);
    static string merc_name_simple(const merc_def& merc, bool ident = false);
};

extern innpingList innping_list;

#if TAG_MAJOR_VERSION == 34
#define REMOVED_DEAD_innS_KEY "removed_dead_inns"
#endif
