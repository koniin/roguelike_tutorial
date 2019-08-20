#include "libtcod.hpp"
#include <iostream>
#include <unordered_map>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <functional>


// http://rogueliketutorials.com/tutorials/tcod/part-13/
// http://www.roguebasin.com/index.php?title=Complete_roguelike_tutorial_using_C%2B%2B_and_libtcod_-_part_10.1:_persistence

/// Goals
// - make it work (with current setup)
    => Dropping an item doesnt seem to work very well (did it ever?)
// - better events (typed)
// - make a simpler iteration function
// - make systems
// - discrepancy between where events are queued 
//    - look at fighter, heal, attack and take_damage all have messages associated but are triggered in different ways

// ECS Improvements
// - easier to get a reference/pointer to component
    /* 
    value_type& operator[](key_type key){
        if (buffered_){
        auto remove_it = std::find(remove_.begin(), remove_.end(), key);
        if (remove_it != remove_.end()) return super::nullElement_;
        
        if (super::indices_.count(key) == 0){
            auto add_it = std::find_if(add_.begin(), add_.end(), [key](const T& t){ return t.id == key; });
            if (add_it != add_.end()) return *add_it;
        }
        }
        return super::operator[](key);
    }
    */
// - 

#include <bitset>
#include <initializer_list>

class ComponentID {
    static size_t counter;
    public:
        template<typename T>
        static size_t value() {
            static size_t id = counter++;
            return id;
        }
};
size_t ComponentID::counter = 0;

typedef std::bitset<512> ComponentMask;

// Entity handle
typedef struct {
    uint32_t id;
    uint32_t generation;
} Entity;

typedef struct {
    uint32_t index;
    uint32_t generation;
} generation_index_t;

#define MAX_ENTITIES 1024
uint32_t entity_id;
std::unordered_map<uint32_t, generation_index_t> entityid_to_index_map;
uint32_t num_entities;
Entity entities[MAX_ENTITIES];
ComponentMask masks[MAX_ENTITIES];

std::function<void(const uint32_t &index)> on_entity_created;
std::function<void(const uint32_t &index, const uint32_t &new_index)> on_entity_removed;

inline bool entity_equals(const Entity &first, const Entity &second) {
    return first.id == second.id && first.generation == second.generation;
}

const Entity InvalidEntity = Entity { 0, 0 };

Entity entity_create() {
    // CHECK IF ID IS IN ANY QUEUE

    uint32_t id = ++entity_id;
    uint32_t index = num_entities++;
    
    entityid_to_index_map[id].index = index;
    entityid_to_index_map[id].generation++;

    Entity e;
    e.id = id;
    e.generation = entityid_to_index_map[id].generation;
    entities[index] = e;

    masks[index].reset();

    on_entity_created(index);

    return e;
}

void entity_remove(Entity handle) {
    auto removed_entity_index = entityid_to_index_map[handle.id].index;
    entityid_to_index_map[handle.id].generation++;

    // PUT ID BACK IN A QUEUE

    num_entities--;

    entityid_to_index_map[entities[num_entities].id].index = removed_entity_index;
    masks[removed_entity_index] = masks[num_entities];
    entities[removed_entity_index] = entities[num_entities];    

    on_entity_removed(removed_entity_index, num_entities);
}

bool entity_alive(Entity handle) {
    return entityid_to_index_map[handle.id].generation == handle.generation;
}

struct ComponentHandle {
    uint32_t i;
};

ComponentHandle entity_get_handle(Entity handle) {
    ComponentHandle c;
    c.i = entityid_to_index_map[handle.id].index;
    return c;
}

template<typename Component>
bool entity_has_component(Entity handle) {
    auto index = entityid_to_index_map[handle.id].index;
    return masks[index].test(ComponentID::value<Component>());
}

bool entity_has_component(Entity handle, size_t component_id) {
    auto index = entityid_to_index_map[handle.id].index;
    return masks[index].test(component_id);
}

bool entity_has_component(const ComponentHandle &ch, size_t component_id) {
    return masks[ch.i].test(component_id);
}

template<typename Component>
void entity_add_component(const ComponentHandle &ch, Component ca[], Component c) {
    ca[ch.i] = c;
    masks[ch.i].set(ComponentID::value<Component>());
}

void entity_remove_component(const ComponentHandle &ch, size_t component_id) {
    masks[ch.i].reset(component_id);
}

template<typename Component>
void entity_remove_component(const ComponentHandle &ch) {
    masks[ch.i].reset(ComponentID::value<Component>());
}

template<typename Component>
void entity_disable_component(const ComponentHandle &ch) {
    masks[ch.i].reset(ComponentID::value<Component>());
}

template<typename Component>
void entity_enable_component(const ComponentHandle &ch) {
    masks[ch.i].set(ComponentID::value<Component>());
}

template <typename C>
ComponentMask create_mask() {
    ComponentMask mask;
    mask.set(ComponentID::value<C>());
    return mask;
}

template <typename C1, typename C2, typename ... Components>
ComponentMask create_mask() {
    return create_mask<C1>() | create_mask<C2, Components ...>();
}

inline void entity_iterate(const ComponentMask &system_mask, std::function<void(uint32_t &index)> f) {
    for(uint32_t i = 0; i < num_entities; i++) {
        if((masks[i] & system_mask) == system_mask) {
            f(i);
            //printf("%d  %c | ", positions[i].x, visuals[i].gfx);
        }
    }
}

// void test_entities() {
//     printf("%d \n", num_entities);
    

//     // Make 3 entities with components
//     // 2 different components

//     Entity e = entity_create();
//     ComponentHandle c = entity_get_handle(e);
//     entity_add_component(c, positions, Position_t { 44 });
//     entity_add_component(c, visuals, Visual_t { 'a' });
    
//     e = entity_create();
//     c = entity_get_handle(e);
//     entity_add_component(c, positions, Position_t { 55 });
    
//     e = entity_create();
//     c = entity_get_handle(e);
//     entity_add_component(c, positions, Position_t { 66 });
//     entity_add_component(c, visuals, Visual_t { 'c' });


//     entity_iterate(create_mask<Position_t, Visual_t>(), [](const uint32_t &i) {
//         printf("%d, %c | ", positions[i].x, visuals[i].gfx);
//     });
//     printf("\n");

//     // iterate entities with both components (checking what components the entity has)
//     ComponentMask system_mask = component_mask_make({ ComponentID::value<Position_t>(), ComponentID::value<Visual_t>() });
//     for(int i = 0; i < num_entities; i++) {
//         if((masks[i] & system_mask) == system_mask) {
//             printf("%d  %c | ", positions[i].x, visuals[i].gfx);
//         }
//     }
//     printf("\n");

//     // iterate entities with only one of the components
//     system_mask = component_mask_make({ ComponentID::value<Position_t>() });
//     for(int i = 0; i < num_entities; i++) {
//         if((masks[i] & system_mask) == system_mask) {
//             printf(" %d | ", positions[i].x);
//         }
//     }
//     printf("\n");

//     // get an entity and use it by reference somehow (update a value)
//     // in an iteration sstore an entity somewhere and use it later (next step)
//     Entity handle;
//     Entity handle2 = entities[num_entities - 1];
//     system_mask = component_mask_make({ ComponentID::value<Position_t>() });
//     for(uint32_t i = 0; i < num_entities; i++) {
//         if(i == 1) {
//             handle = entities[i];
//         }
//     }

//     // evaluate if entity is valid and use it if it is
//     if(entityid_to_index_map[handle.id].generation == handle.generation) {
//         positions[entityid_to_index_map[handle.id].index].x = 666;
//     }

//     // iterate entities
//     printf("-- iterate all positions \n");
//     for(int i = 0; i < num_entities; i++) {
//         printf(" %d | ", positions[i].x);
//     }
//     printf("\n");

//     // delete that entity
//     entity_remove(handle);


//     // evaluate if it is valid
//     if(entity_alive(handle)) {
//         printf("this is incorrect ;( \n");
//     } else {
//         printf("this is correct \n");
//     }

//     // evaluate if it is valid
//     if(entityid_to_index_map[handle2.id].generation == handle2.generation) {
//         printf("this is correct:  pos -> %d \n", positions[entityid_to_index_map[handle2.id].index].x);
//     } else {
//         printf("this should not be printed \n");
//     }

//     // iterate entities
//     for(int i = 0; i < num_entities; i++) {
//         printf(" pos: %d , vis: %c | ", positions[i].x, visuals[i].gfx);
//     }
//     printf("\n");

//     printf("Create a new entity: \n");
//     e = entity_create();
//     c = entity_get_handle(e);
    
//     positions[c.i].x = 123;
//     masks[c.i].set(ComponentID::value<Position_t>());
//     visuals[c.i].gfx = 'Y';
//     masks[c.i].set(ComponentID::value<Visual_t>());

//     // iterate entities
//     system_mask = component_mask_make({ ComponentID::value<Position_t>(), ComponentID::value<Visual_t>() });
//     for(int i = 0; i < num_entities; i++) {
//         if((masks[i] & system_mask) == system_mask) {
//             printf("%d  %c | ", positions[i].x, visuals[i].gfx);
//         }
//     }
//     printf("\n");

//     // evaluate if it is valid
//     if(entity_alive(handle)) {
//         printf("this is incorrect ;( \n");
//     } else {
//         printf("this is correct \n");
//     }
//     // evaluate if it is valid
//     if(entity_alive(handle2)) {
//         printf("this is correct:  pos -> %d \n", positions[entityid_to_index_map[handle2.id].index].x);
//     } else {
//         printf("this should not be printed \n");
//     }

//     handle2 = entities[num_entities - 1];


//     if(entity_alive(handle2)) {
//         c = entity_get_handle(e);
//         if(entity_has_component(handle2, ComponentID::value<Position_t>())) {
//             printf("this is correct:  pos -> %d \n", positions[c.i].x);
//         } else {
//             printf("this should not be printed \n");
//         }
//     } else {
//         printf("this should not be printed \n");
//     }
    
//     entity_remove(handle2);

//     if(entity_alive(handle2)) {
//         printf("this should not be printed \n");
//     } else {
//         printf("this is correct \n");
//     }

//     // iterate entities
//     system_mask = component_mask_make({ ComponentID::value<Position_t>() });
//     for(int i = 0; i < num_entities; i++) {
//         if((masks[i] & system_mask) == system_mask) {
//             printf("%d | ", positions[i].x);
//         }
//     }
//     printf("\n");
// }

// Skipped things:
// - A* movement for monsters (Part 6)
// - Separate drop item inventory listing (Part 8)
// - SAVING & LOADING => too many pointers and fucked up architecture
//   - better handle that from the start on refactoring phase

// Things to do

// - input handling (check ben porter how its done)
// - event handling and decoupling
// - remove all the naked pointers (do this or ECS first?)
// - save/load

// Intressant uppdelning 
//   => http://i.imgur.com/PCKu0ip.png
//  speciellt att ha olika "managers" som hanterar delar (som en 2 eller 3 - lagers arkitektur (services etc))

// Look at Ben porter example game also!
// And dod playground (at least ecs part with references to vector elements)
// - Component naming and split fighter into more components?
// - also systems..
// - fix event handling - what is what (message? seems like a shitty event)
//      also logic in events? yeah why not, but whats the border between?
// - marked_for_deletion to remove entities
// - Memory handling is atrociuos (pointers and memory everywhere)
// - most things are specific to player (don't know if enemies can have inventory)
//    => all event handling is specific to player now
//    => also ties into event handling fix (what is and what is not an event?)

// http://www.roguebasin.com/index.php?title=Complete_roguelike_tutorial_using_C%2B%2B_and_libtcod_-_part_2:_map_and_actors
// 

// ECS
// https://blog.therocode.net/2018/08/simplest-entity-component-system
// https://austinmorlan.com/posts/entity_component_system/

int rand_int(int min, int max) {
    int lower = min;
    int upper = max;
    return (rand() % (upper - lower + 1)) + lower; 
}

/// returns an weighted index from an array of weights 
/// input e.g.: [10, 10, 80], 3
/// output: one of 0, 1, 2 depending on rand_int result
int rand_weighted_index(int *chances, int size) {
    int sum_of_chances = 0;
    for(int i = 0; i < size; i++) {
        sum_of_chances += chances[i];
    }
    int random_chance = rand_int(1, sum_of_chances);

    int running_sum = 0;
    int choice = 0;
    for(int i = 0; i < size; i++) {
        running_sum += chances[i];
        if(random_chance <= running_sum) {
            return choice;
        }
        choice += 1;
    }
    return choice;
}

float distance_to(int x, int y, int target_x, int target_y) {
    auto dx = (float)target_x - x;
    auto dy = (float)target_y - y;
    return sqrtf(dx*dx + dy*dy);
}

struct WeightByLevel {
    int weight;
    int level;
};
int from_dungeon_level(std::vector<WeightByLevel> &table, int dungeon_level) {
    for(size_t i = table.size(); i--;) {
        if(dungeon_level >= table[i].level) {
            return table[i].weight;
        }
    }
    return 0;
}

struct Rect {
    int x, y, w, h, x2, y2;
};
Rect rect_make(int x, int y, int w, int h) {
    return { x, y, w, h, x + w, y + h };
}

void rect_center(const Rect &rect, int &x, int &y) {
    x = rect.x + (rect.w / 2);
    y = rect.y + (rect.h / 2);
}

bool rect_intersects(const Rect &r, const Rect &other) {
    return r.x <= other.x2 && r.x2 >= other.x &&
        r.y <= other.y2 && r.y2 >= other.y;
}

enum LogStatus {
    Information,
    Warning,
    Error
};

void engine_log(const LogStatus &status, const std::string &message) {
    static const std::string delimiter = "|";
    switch(status) {
        case Information: printf("INFO %s %s\n", delimiter.c_str(), message.c_str()); break;
        case Warning: printf("WARN %s %s\n", delimiter.c_str(), message.c_str()); break; 
        case Error: printf("ERRO %s %s\n", delimiter.c_str(), message.c_str()); break;
    }
}

bool engine_running = true;
void engine_exit() {
    engine_running = false;
}

// Input ---
TCOD_key_t key = {TCODK_NONE,0};
TCOD_mouse_t mouse;

void input_update() {
    TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE, &key, &mouse);
}

bool input_key_pressed(char k) {
    return key.c == k;
}

bool input_key_pressed(TCOD_keycode_t k) {
    return key.vk == k;
}

char input_key_char_get() {
    return key.c;
}

bool input_lalt() {
    return key.lalt;
}
///

const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 50;

struct Colors {
    TCODColor dark_wall = TCODColor(0, 0, 100);
    TCODColor dark_ground = TCODColor(50, 50, 150);
    TCODColor light_wall = TCODColor(130, 110, 50);
    TCODColor light_ground = TCODColor(200, 180, 50);
} color_table;

enum GameState {
    PLAYER_DEAD,
    PLAYER_TURN,
    ENEMY_TURN,
    SHOW_INVENTORY,
    TARGETING,
    MAIN_MENU,
    LEVEL_UP,
    CHARACTER_SCREEN
};

enum EventType {
    Message,
    EntityDead,
    ItemPickup,
    NextFloor,
    EquipmentChange
};

struct Event {
    EventType type;
    Entity entity;
    std::string message;
    TCOD_color_t color;
    int flag;
};

std::vector<Event> _event_queue; 
void events_queue(Event e) {
    _event_queue.push_back(e);
}

const int Map_Width = 80;
const int Map_Height = 43;
const int Room_max_size = 10;
const int Room_min_size = 6;
const int Max_rooms = 30; 
const TCOD_fov_algorithm_t fov_algorithm = FOV_BASIC;
const bool fov_light_walls = true;
const int fov_radius = 10;

struct Tile {
    bool blocked = true;
    bool block_sight = true;
    bool explored = false;
};

struct GameMap {
    Tile tiles[Map_Width * Map_Height];
    TCODMap *tcod_fov_map;
    int num_rooms = 0;
    std::vector<Rect> rooms;
    int level = 1;
};

void move_towards(const GameMap &map, int &x, int &y, const int target_x, const int target_y);

struct RenderPriority {
    // lower is lower prio
    int STAIRS = 0;
    int CORPSE = 1;
    int ITEM = 2;
    int ENTITY = 3;
} render_priority;

struct EntityFat {
    int x,y;
    int gfx;
    TCODColor color;
    std::string name;
    bool blocks = false;
    int render_order = 0;

    EntityFat() {}
    EntityFat(int x_, int y_, int gfx_, TCODColor color_, std::string name_, bool blocks_, int render_order_) : 
        x(x_), y(y_), gfx(gfx_), color(color_), name(name_), blocks(blocks_), render_order(render_order_) {
    }
};

EntityFat &get_entityfat(Entity e);

struct ItemArgs {
    int amount = 0;
    float range = 0.0f;
    Entity target;
    int target_x = 0;
    int target_y = 0;
};

struct Context {
    GameMap &map;

    Context(GameMap &map) : map(map) {}
};

enum Targeting {
    None,
    Position
};

struct Stairs {
    int floor;

    Stairs() {}
    Stairs(int floor) : floor(floor) {} 
};

enum EquipmentSlot {
    MAIN_HAND,
    OFF_HAND
};
struct Equippable {
    EquipmentSlot slot;
    int power_bonus;
    int defense_bonus;
    int max_hp_bonus;

    Equippable() {}
    Equippable(EquipmentSlot slot, int power_bonus, int defense_bonus, int max_hp_bonus) 
        : slot(slot), power_bonus(power_bonus), defense_bonus(defense_bonus), max_hp_bonus(max_hp_bonus)
        {}
};

Equippable &get_equippable(Entity &e);

struct Equipment {
    Entity main_hand = InvalidEntity;
    Entity off_hand = InvalidEntity;

    int max_hp_bonus() {
        int bonus = 0;
        if(!entity_equals(main_hand, InvalidEntity) && entity_has_component<Equippable>(main_hand)) {
            auto &eq = get_equippable(main_hand);
            bonus += eq.max_hp_bonus;
        }
        if(!entity_equals(off_hand, InvalidEntity) && entity_has_component<Equippable>(off_hand)) {
            auto &eq = get_equippable(off_hand);
            bonus += eq.max_hp_bonus;
        }
        return bonus;
    }

    int power_bonus() {
        int bonus = 0;
        if(!entity_equals(main_hand, InvalidEntity) && entity_has_component<Equippable>(main_hand)) {
            auto &eq = get_equippable(main_hand);
            bonus += eq.power_bonus;
        }
        if(!entity_equals(off_hand, InvalidEntity) && entity_has_component<Equippable>(off_hand)) {
            auto &eq = get_equippable(off_hand);
            bonus += eq.power_bonus;
        }
        return bonus;
    }

    int defense_bonus() {
        int bonus = 0;
        if(!entity_equals(main_hand, InvalidEntity) && entity_has_component<Equippable>(main_hand)) {
            auto &eq = get_equippable(main_hand);
            bonus += eq.defense_bonus;
        }
        if(!entity_equals(off_hand, InvalidEntity) && entity_has_component<Equippable>(off_hand)) {
            auto &eq = get_equippable(off_hand);
            bonus += eq.defense_bonus;
        }
        return bonus;
    }

    void toggle_equipment(Entity equippable_entity) {
        auto &eq = get_equippable(equippable_entity);
        auto slot = eq.slot;

        if(slot == MAIN_HAND) {
            if(entity_equals(main_hand, equippable_entity)) {
                events_queue({ EventType::EquipmentChange, equippable_entity, "", TCOD_amber, 0 });
                main_hand = InvalidEntity;
            } else {
                if(entity_equals(main_hand, InvalidEntity)) {
                    events_queue({ EventType::EquipmentChange, main_hand, "", TCOD_amber, 0 });
                }
                main_hand = equippable_entity;
                events_queue({ EventType::EquipmentChange, equippable_entity, "", TCOD_amber, 1 });
            }
        } else if(slot == OFF_HAND) {
            if(entity_equals(off_hand, equippable_entity)) {
                events_queue({ EventType::EquipmentChange, equippable_entity, "", TCOD_amber, 0 });
                off_hand = InvalidEntity;
            } else {
                if(!entity_equals(off_hand, InvalidEntity)) {
                    events_queue({ EventType::EquipmentChange, off_hand, "", TCOD_amber, 0 });
                }
                off_hand = equippable_entity;
                events_queue({ EventType::EquipmentChange, equippable_entity, "", TCOD_amber, 1 });
            }
        } else {
            engine_log(LogStatus::Warning, "Equipment slot is not implemented " + std::to_string(slot));
        }
    }
};

Equipment &get_equipment(Entity &e);

struct Item {
    int id;
    std::string name;
    std::function<bool(Entity entity, const ItemArgs &args, Context &context)> on_use = NULL;
    ItemArgs args;
    Targeting targeting = Targeting::None;
    std::string targeting_message = "";
};

Item &get_item(Entity e);

struct Inventory {
    Entity _owner;
    std::vector<Entity> items;
    size_t capacity;
    Inventory() {}
    Inventory(Entity owner, size_t capacity) : _owner(owner), capacity(capacity) {}

    int _dirty_shit = 0;

    bool add_item(Entity  item) {
        if(items.size() >= capacity) {
            return false;
        } 
        items.push_back(item);
        return true;
    }

    bool use(size_t index, Context &context) {
        return use(items[index], context);
    }

    bool use(Entity &entity, Context &context) {
        if(entity_has_component<Equippable>(entity)) {
            auto &equipment = get_equipment(entity);
            equipment.toggle_equipment(entity);
            return false;
        }

        auto &item = get_item(entity);

        if(!item.on_use) {    
            std::string msg = "The " + item.name + " cannot be used.";
            events_queue({ EventType::Message, Entity(), msg, TCOD_yellow});
            return false;
        }

        if(requires_target(entity)) {
            return false;
        }

        bool consumed = item.on_use(_owner, item.args, context);
        remove(entity);
        return true;
    }
    
    bool requires_target(size_t index) {
        return requires_target(items[index]);
    }

    bool requires_target(Entity &entity) {    
        auto &item = get_item(entity);
        if(item.targeting == Targeting::None) {
            return false;
        }
        if(item.targeting != Targeting::None && !(item.args.target_x || item.args.target_y)) {
            return true;
        }
        return false;
    }

    void remove(Entity &item) {
        int delete_count = 0;
        int index = 0;
        for(auto &i : items) {
            if(entity_equals(i, item)) {
                delete_count++;
                break;
            }
            index++;
        }
        if(delete_count == 0) {
            engine_log(LogStatus::Error, "NO ITEM REMOVED WHEN PICKED UP! (Inventory::remove)");
        }
        items.erase(items.begin() + index);
    }
};

Inventory &get_inventory(Entity e);

struct Fighter {
    Entity owner;
    int hp;
    int hp_max;
    int defense_max;
    int power_max;
    int xp;
    
    Fighter() {}

    Fighter(Entity &owner, int hp_, int defense_, int power_, int xp = 0) 
        : owner(owner), hp(hp_), hp_max(hp_), defense_max(defense_), power_max(power_), xp(xp) {}

    int max_hp() {
        int bonus = 0;
        if(entity_has_component<Equipment>(owner)) {
            auto &eq = get_equipment(owner);
            bonus += eq.max_hp_bonus();
        }
        return hp_max + bonus;
    }

    int power() {
        int bonus = 0;
        if(entity_has_component<Equipment>(owner)) {
            auto &eq = get_equipment(owner);
            bonus += eq.power_bonus();
        }
        return power_max + bonus;
    }

    int defense() {
        int bonus = 0;
        if(entity_has_component<Equipment>(owner)) {
            auto &eq = get_equipment(owner);
            bonus += eq.defense_bonus();
        }
        return defense_max + bonus;
    }
};

Fighter &get_fighter(Entity e);

void take_damage(Entity entity, int amount) {
    Fighter &f = get_fighter(entity);
    f.hp -= amount;
    if(f.hp <= 0) {
        f.hp = 0;
        events_queue({ EventType::EntityDead, entity });
    }
}

void attack(Entity attacker_e, Entity defender_e) {
    EntityFat &e_attacker = get_entityfat(attacker_e);
    EntityFat &e_defender = get_entityfat(defender_e);
    
    Fighter &attacker = get_fighter(attacker_e);
    Fighter &target_fighter = get_fighter(defender_e);

    if(target_fighter.hp == 2) {
        int a = 0;
        a++;
    }

    int damage = attacker.power() - target_fighter.defense();
    if(damage > 0) {
        // VERY SHITTY STRING ALLOCATION
        char buffer[255];
        sprintf(buffer, "%s attacks %s for %d hit points", e_attacker.name.c_str(), e_defender.name.c_str(), damage);
        events_queue({ EventType::Message, attacker_e, buffer, TCOD_amber });
        take_damage(defender_e, damage);
    } else {
        // VERY SHITTY STRING ALLOCATION
        char buffer[255];
        sprintf(buffer, "%s attacks %s but deals no damage", e_attacker.name.c_str(), e_defender.name.c_str());
        events_queue({ EventType::Message, attacker_e, buffer, TCOD_light_grey });
    }
}

void heal(Fighter &f, int amount) {
    f.hp += amount;
    if(f.hp > f.hp_max) {
        f.hp = f.hp_max;
    }
}

struct AiContext {
    std::function<void(Entity &owner, Entity &target, GameMap &map, AiContext &context)> secondary;
    int counter = 0;
};

struct Ai {
    AiContext context;
    Entity _owner;
    std::function<void(Entity &owner, Entity &target, GameMap &map, AiContext &context)> on_take_turn;
    void take_turn(Entity &target, GameMap &map) {
        on_take_turn(_owner, target, map, context);
    }

    Ai() {}
    Ai(Entity _owner, std::function<void(Entity &owner, Entity &target, GameMap &map, AiContext &context)> on_take_turn) :
        _owner(_owner), on_take_turn(on_take_turn) {}
};
Ai &get_ai(Entity &e);

void BasicMonster_take_turn(Entity &_owner, Entity &target, GameMap &map, AiContext &context) {
    auto &entity_fat = get_entityfat(_owner);
    if(map.tcod_fov_map->isInFov(entity_fat.x, entity_fat.y)) {
        auto &target_fat = get_entityfat(target);
        auto &target_fighter = get_fighter(target);
        if(distance_to(entity_fat.x, entity_fat.y, target_fat.x, target_fat.y) >= 2.0f) {

            // CAN REPLACE HITS WITH ASTAR MOVEMENT

            move_towards(map, entity_fat.x, entity_fat.y, target_fat.x, target_fat.y);
        } else if(target_fighter.hp > 0) {
            attack(_owner, target);
        }
    }
}

void ConfusedMonster_take_turn(Entity &owner, Entity &target, GameMap &map, AiContext &context) {
    if(context.counter > 0) {
        context.counter--;

        auto &entity_fat = get_entityfat(target);
        int rx = entity_fat.x + rand_int(0, 2) - 1;
        int ry = entity_fat.y + rand_int(0, 2) - 1;

        if(rx != entity_fat.x && ry != entity_fat.y) {
            move_towards(map, entity_fat.x, entity_fat.y, rx, ry);
        }
    } else {
        auto &entity_fat = get_entityfat(target);
        std::string msg = "The " + entity_fat.name + " is no longer confused!";
        events_queue({ EventType::Message, InvalidEntity, msg, TCOD_red });
        
        Ai &ai = get_ai(target);
        ai.on_take_turn = context.secondary;
    }
}

struct Level {
    int current_level;
    int current_xp;
    int level_up_base;
    int level_up_factor;

    Level(int current_level = 1, int current_xp = 0, int level_up_base = 200, int level_up_factor = 150) :
        current_level(current_level), current_xp(current_xp), level_up_base(level_up_base), level_up_factor(level_up_factor) 
    {}

    int experience_to_next_level() {
        return level_up_base + current_level * level_up_factor;
    }

    bool add_xp(int amount) {
        current_xp += amount;

        if(current_xp > experience_to_next_level()) {
            current_xp -= experience_to_next_level();
            current_level += 1;
            return true;
        } 

        return false;
    }
};

Level &get_level(Entity e);


GameMap game_map;

GameState game_state = MAIN_MENU;
GameState previous_game_state = MAIN_MENU;
Entity player_handle;
Entity targeting_item;

Level levels[1]; // test to only give component to player
Inventory inventories[1];  // test to only give component to player
Equipment equipments[1];  // test to only give component to player

EntityFat entity_fats[MAX_ENTITIES];
Fighter fighters[MAX_ENTITIES];
Ai ais[MAX_ENTITIES];
Item items[MAX_ENTITIES];
Stairs stairs[MAX_ENTITIES];
Equippable equippables[MAX_ENTITIES];

void entity_created(const uint32_t &index) {
    // Here you can decide if you want to reset or do nothing
    // If all components have proper constructors this doesnt need to do anything
    
    // const Fighter f;
    // fighters[index] = f;
}

void entity_removed(const uint32_t &index_to, const uint32_t &index_from) {
    // levels[index_to] = levels[index_from];
    // inventories[index_to] = inventories[index_from];
    // equipments[index_to] = equipments[index_from];

    entity_fats[index_to] = entity_fats[index_from];
    fighters[index_to] = fighters[index_from];
    ais[index_to] = ais[index_from];
    items[index_to] = items[index_from];
}

EntityFat &get_entityfat(Entity e) {
    ComponentHandle c = entity_get_handle(e);
    return entity_fats[c.i];
}

Fighter &get_fighter(Entity e) {
    ComponentHandle c = entity_get_handle(e);
    return fighters[c.i];
}

Level &get_level(Entity e) {
    ComponentHandle c = entity_get_handle(e);
    return levels[c.i];
}

Ai &get_ai(Entity &e) {
    ComponentHandle c = entity_get_handle(e);
    return ais[c.i];
}

Item &get_item(Entity e) {
    ComponentHandle c = entity_get_handle(e);
    return items[c.i];
}

Inventory &get_inventory(Entity e) {
    ComponentHandle c = entity_get_handle(e);
    return inventories[c.i];
}

Equippable &get_equippable(Entity &e) {
    ComponentHandle c = entity_get_handle(e);
    return equippables[c.i];
}

Equipment &get_equipment(Entity &e) {
    ComponentHandle c = entity_get_handle(e);
    return equipments[c.i];
}

bool cast_heal_entity(Entity entity, const ItemArgs &args, Context &context) {
    auto &fighter = get_fighter(entity);
    if(fighter.hp == fighter.hp_max) {
        events_queue({ EventType::Message, InvalidEntity, "You are already at full health", TCOD_yellow });
        return false;
    } 
    heal(fighter, args.amount);
    events_queue({ EventType::Message, InvalidEntity, "Your wounds start to feel better!", TCOD_green });
    return true;
}

bool cast_lightning_bolt(Entity caster, const ItemArgs &args, Context &context) {
    Entity closest = InvalidEntity;
    float closest_distance = 1000000.f;
    auto &caster_fat = get_entityfat(caster);
    entity_iterate(create_mask<EntityFat, Fighter>(), [&](const uint32_t &i) {
        EntityFat &entity_fat = entity_fats[i];
        if(!entity_equals(caster, entities[i]) && context.map.tcod_fov_map->isInFov(entity_fat.x, entity_fat.y)) {
            float distance = distance_to(caster_fat.x, caster_fat.y, entity_fat.x, entity_fat.y);
            if(distance < closest_distance) {
                closest = entities[i];
                closest_distance = distance;
            }
        }
    });

    if(!entity_equals(closest, InvalidEntity)) {
        auto &closest_fat = get_entityfat(closest);

        take_damage(closest, args.amount);
        std::string msg = "A lighting bolt strikes the " + closest_fat.name + " with a loud thunder! \nThe damage is " + std::to_string(args.amount);
        events_queue({ EventType::Message, InvalidEntity, msg, TCOD_amber });
        return true;
    } else {
        events_queue({ EventType::Message, InvalidEntity, "No enemy is close enough to strike.", TCOD_red });
        return false;
    }
}

bool cast_fireball(Entity caster, const ItemArgs &args, Context &context) {
    if(!context.map.tcod_fov_map->isInFov(args.target_x, args.target_y)) {
        events_queue({ EventType::Message, InvalidEntity, "You cannot target a tile outside your field of view.", TCOD_yellow });
        return false;
    }

    std::string msg = "The fireball explodes, burning everything within " + std::to_string(args.range) + " tiles!";
    events_queue({ EventType::Message, InvalidEntity, msg, TCOD_orange });

    entity_iterate(create_mask<EntityFat, Fighter>(), [&](const uint32_t &i) {
        EntityFat &entity_fat = entity_fats[i];

        if(distance_to(entity_fat.x, entity_fat.y, args.target_x, args.target_y) <= args.range) {
            msg = "The " + entity_fat.name + " gets burned for " + std::to_string(args.amount) + " hit points.";
            events_queue({ EventType::Message, InvalidEntity, msg, TCOD_orange });
            take_damage(entities[i], args.amount);
        }
    });

    return true;
}

bool cast_confuse(Entity caster, const ItemArgs &args, Context &context) {
    if(!context.map.tcod_fov_map->isInFov(args.target_x, args.target_y)) {
        events_queue({ EventType::Message, InvalidEntity, "You cannot target a tile outside your field of view.", TCOD_yellow });
        return false;
    }

    bool executed = false;
    entity_iterate(create_mask<EntityFat, Ai>(), [&](const uint32_t &i) {
        EntityFat &entity_fat = entity_fats[i];
        if(entity_fat.x == args.target_x &&  entity_fat.y == args.target_y) {
            std::string msg = "The eyes of the " + entity_fat.name + " looks vacant as it starts to stumble around!";
            events_queue({ EventType::Message, InvalidEntity, msg, TCOD_light_green });
            ais[i].context.secondary = ais[i].on_take_turn;
            ais[i].on_take_turn = ConfusedMonster_take_turn;
            ais[i].context.counter = 10;
            executed = true;
        }
    });
    if(executed) {
        return true;
    }

    std::string msg = "There is no targetable entity at that location.";
    events_queue({ EventType::Message, InvalidEntity, msg, TCOD_yellow });
    return false;
}

// UI
const int Bar_width = 20;
const int Panel_height = 7;
const int Panel_y = SCREEN_HEIGHT - Panel_height;
//

int map_index(int x, int y) {
    return x + Map_Width * y;
}

bool map_blocked(const GameMap &map, int x, int y) {
    if(map.tiles[map_index(x, y)].blocked) {
        return true;
    }
    return false;
}

void map_make_room(GameMap &map, const Rect &room) {
    for(int x = room.x + 1; x < room.x2; x++) {
        for(int y = room.y + 1; y < room.y2; y++) {
            map.tiles[map_index(x, y)].blocked = false;
            map.tiles[map_index(x, y)].block_sight = false;   
            map.tcod_fov_map->setProperties(x, y, true, true);
        }    
    }
}

void map_make_h_tunnel(GameMap &map, int x1, int x2, int y) {
    for(int x = std::min(x1, x2); x < std::max(x1, x2) + 1; x++) {
        map.tiles[map_index(x, y)].blocked = false;
        map.tiles[map_index(x, y)].block_sight = false;
        map.tcod_fov_map->setProperties(x, y, true, true);
    }
//     def create_h_tunnel(self, x1, x2, y):
// +       for x in range(min(x1, x2), max(x1, x2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}
void map_make_v_tunnel(GameMap &map, int y1, int y2, int x) {
    for(int y = std::min(y1, y2); y < std::max(y1, y2) + 1; y++) {
        map.tiles[map_index(x, y)].blocked = false;
        map.tiles[map_index(x, y)].block_sight = false;
        map.tcod_fov_map->setProperties(x, y, true, true);
    }
// +   def create_v_tunnel(self, y1, y2, x):
// +       for y in range(min(y1, y2), max(y1, y2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}

void map_generate(GameMap &map, int max_rooms, int room_min_size, int room_max_size, int map_width, int map_height) {
    for(int i = 0; i < max_rooms; i++) {
        // random width and height
        int w = rand_int(room_min_size, room_max_size);
        int h = rand_int(room_min_size, room_max_size);
        // random position without going out of the boundaries of the map
        int x = rand_int(0, map_width - w - 1);
        int y = rand_int(0, map_height - h - 1);

        Rect new_room = rect_make(x, y, w, h);

        // See if any room intersects
        bool intersects = false;
        for(auto &r : map.rooms) {
            if(rect_intersects(r, new_room)) {
                intersects = true;
                break;
            }
        }

        if(intersects) {
            continue;
        }

        // "paint" it to the map's tiles
        map_make_room(map, new_room);
        int new_x, new_y;
        rect_center(new_room, new_x, new_y);

        if(map.num_rooms > 0) {
            int prev_x, prev_y;
            rect_center(map.rooms[map.num_rooms - 1], prev_x, prev_y);
            if(rand_int(0, 1) == 1) {
                map_make_h_tunnel(map, prev_x, new_x, prev_y);
                map_make_v_tunnel(map, prev_y, new_y, new_x);
            } else {
                map_make_v_tunnel(map, prev_y, new_y, new_x);
                map_make_h_tunnel(map, prev_x, new_x, prev_y);
            }
        }

        map.rooms.push_back(new_room);
        map.num_rooms++;
    }
}

struct MonsterBlueprint {
    std::vector<WeightByLevel> weights;
    std::string name;
    char visual;
    TCODColor color;
    int hp;
    int defense;
    int power;
    int xp;
};
std::vector<MonsterBlueprint> monster_data = {  
    { 
        { { 80, 1 } },
        "Orc", 'o', TCOD_desaturated_green, 10, 0, 4, 35 
    },
    { 
        { { 15, 3 }, { 30, 5 }, { 60, 7 } },
        "Troll", 'T', TCOD_darker_green, 30, 2, 8, 100 
    }
};

void map_add_monsters(GameMap &map) {
    for(const Rect &room : map.rooms) {
        std::vector<WeightByLevel> weights = { { 2, 1 }, { 3, 4 }, { 5, 6 } };
        int number_of_monsters = from_dungeon_level(weights, map.level);
        // int number_of_monsters = rand_int(0, max_monsters_per_room);
        for(int i = 0; i < number_of_monsters; i++) {
            int x = rand_int(room.x + 1, room.x2 - 1); 
            int y = rand_int(room.y + 1, room.y2 - 1);

            bool occupied = false;
            for(uint32_t ei = 0; ei < num_entities; ei++) {
                //we assume all entities have the base entity component
                EntityFat *e = &entity_fats[ei];
                if(e->x == x && e->y == y) {
                    occupied = true;
                    break;
                }
            }
            if(occupied) {
                continue;
            }

            std::vector<int> chances;
            chances.reserve(monster_data.size());
            for(auto md : monster_data) {
                int chance = from_dungeon_level(md.weights, map.level);
                if(chance > 0)
                    chances.push_back(chance);
            }
            auto blueprint_index = rand_weighted_index(chances.data(), chances.size());
            auto &m = monster_data[blueprint_index];

            auto e = entity_create();
            auto c = entity_get_handle(e);
            entity_add_component(c, entity_fats, EntityFat(x, y, m.visual, m.color, m.name, true, render_priority.ENTITY ));
            entity_add_component(c, fighters, Fighter(e, m.hp, m.defense, m.power, m.xp));
            entity_add_component(c, ais, Ai(e, BasicMonster_take_turn));           
        }
    }
}

struct ItemBlueprint {
    std::vector<WeightByLevel> weights;
    int id;
    std::string name;
    char visual;
    TCODColor color;
};
std::vector<ItemBlueprint> item_data = {  
    { 
        { { 35, 1 } },
        0, "Health Potion", '!', TCOD_violet
    },
    { 
        { { 25, 4 } },
        1, "Fireball Scroll", '#', TCOD_red
    },
    { 
        { { 25, 6 } },
        2, "Confusion Scroll", '#', TCOD_light_pink
    },
    { 
        { { 10, 2 } },
        3, "Lightning Scroll", '#', TCOD_violet
    },
    { 
        { { 5, 4 } },
        4, "Sword", '/', TCOD_sky
    },
    { 
        { { 15, 8 } },
        5, "Shield", '[', TCOD_darker_orange
    }
};

void map_add_items(GameMap &map) {
    for(const Rect &room : map.rooms) {
        std::vector<WeightByLevel> weights = { {1, 1}, {2, 4} };
        int number_of_items = from_dungeon_level(weights, map.level);
        // int number_of_items = rand_int(0, max_items_per_room);
        for(int i = 0; i < number_of_items; i++) {
            int x = rand_int(room.x + 1, room.x2 - 1); 
            int y = rand_int(room.y + 1, room.y2 - 1);

            bool occupied = false;
            for(uint32_t ei = 0; ei < num_entities; ei++) {
                //we assume all entities have the base entity component
                EntityFat *e = &entity_fats[ei];
                if(e->x == x && e->y == y) {
                    occupied = true;
                    break;
                }
            }
            if(occupied) {
                continue;
            }

            // We probably want some other way of generating the item etc
            // so this will change in the future anyway

            // Also perhaps split items into different categories
            // so we can select a random category or a set number from each category
            std::vector<int> chances;
            chances.reserve(item_data.size());
            for(auto id : item_data) {
                int chance = from_dungeon_level(id.weights, map.level);
                if(chance > 0) {
                    chances.push_back(chance);
                }
            }
            auto blueprint_index = rand_weighted_index(chances.data(), chances.size());
            auto &item_blueprint = item_data[blueprint_index];
            //int index = rand_weighted_index(item_weights.data(), item_weights.size());
            auto e = entity_create();
            auto c = entity_get_handle(e);
            entity_add_component(c, entity_fats, 
                EntityFat(x, y, item_blueprint.visual, item_blueprint.color, item_blueprint.name, false, render_priority.ITEM));
            
            Item item;
            item.name = item_blueprint.name;
            if(item_blueprint.id == 0) {
                item.args = { 40 };
                item.on_use = cast_heal_entity;
            } else if(item_blueprint.id == 1) {
                item.args = { 25, 3 };
                item.on_use = cast_fireball;
                item.targeting = Targeting::Position;
                item.targeting_message = "Left-click a target tile for the fireball, or right click to cancel.";
            } else if(item_blueprint.id == 2) {
                item.on_use = cast_confuse;
                item.targeting = Targeting::Position;
                item.targeting_message = "Left-click an enemy to confuse it, or right click to cancel.";
            } else if(item_blueprint.id == 3) {
                item.args = { 40, 5 };
                item.on_use = cast_lightning_bolt;
            } else if(item_blueprint.id == 4) {
                entity_add_component(c, equippables, Equippable(MAIN_HAND, 3, 0, 0));
            } else if(item_blueprint.id == 5) {
                entity_add_component(c, equippables, Equippable(OFF_HAND, 0, 1, 0));
            } else {
                std::string message = "No item with id; " + std::to_string(item.id);
                engine_log(LogStatus::Warning, message);
                entity_remove(e);
                continue;
            }
            entity_add_component(c, items, item);
        }
    }
}

void map_add_stairs(GameMap &map) {
    auto &last_room = map.rooms[map.num_rooms - 1];
    //auto &last_room = map.rooms[0];
    int center_x, center_y;
    rect_center(last_room, center_x, center_y);
    auto e = entity_create();
    auto c = entity_get_handle(e);
    entity_add_component(c, entity_fats, 
        EntityFat(center_x, center_y, '>', TCOD_white, "Stairs", false, render_priority.STAIRS ));
    entity_add_component(c, stairs, Stairs(map.level + 1));
}

bool entity_blocking_at(int x, int y, Entity &found_entity) {
    for(uint32_t i = 0; i < num_entities; i++) {
        auto entity = &entity_fats[i];
        if(x == entity->x && y == entity->y && entity->blocks) {
            found_entity = entities[i];
            return true;
        }
    }
    return false;
}

bool can_walk(const GameMap &map, int x, int y) {
    Entity target;
    return !map_blocked(map, x, y) && !entity_blocking_at(x, y, target);
}

void move_towards(const GameMap &map, int &x, int &y, const int target_x, const int target_y) {
    int dx, dy;
    dx = target_x - x;
    dy = target_y - y;
    float distance = sqrtf((float)dx*dx+(float)dy*dy);
    
    dx = (int)(round(dx/distance));
    dy = (int)(round(dy/distance));

    if(can_walk(map, x + dx, y + dy)) {
        x = x + dx;
        y = y + dy;
    } else if(can_walk(map, x + dx, y)) {
        x = x + dx;
    } else if(can_walk(map, x, y + dy)) {
        y = y + dy;
    }
}

void gui_render_bar(TCODConsole *panel, int x, int y, int total_width, std::string name, 
                    int value, int maximum, TCOD_color_t bar_color, TCOD_color_t back_color) {
    int bar_width = int(float(value) / maximum * total_width);

    panel->setDefaultBackground(back_color);
    panel->rect(x, y, total_width, 1, false, TCOD_BKGND_SCREEN);

    panel->setDefaultBackground(bar_color);
    if(bar_width > 0) {
        panel->rect(x, y, bar_width, 1, false, TCOD_BKGND_SCREEN);
    }

    panel->setDefaultForeground(TCOD_white);
    panel->printEx(x + total_width / 2, y, TCOD_BKGND_NONE, TCOD_CENTER, "%s: %d/%d", name.c_str(), value, maximum);
}

struct LogEntry {
    char *text;
    TCODColor color;
    LogEntry(const char *text_, const TCODColor &col_) : 
        text(strdup(text_)), color(col_) {    
    }
    ~LogEntry() {
        free(text);
    } 
};

std::vector<LogEntry*> gui_log;
static const int Log_x = Bar_width + 2;
static const int Log_height = Panel_height - 1;
TCODConsole *root_console;
TCODConsole *bar;
TCODConsole *character_screen;
TCODConsole *menu;

void gui_log_message(const TCODColor &col, const char *text, ...) {
    va_list ap;
    char buf[128];
    va_start(ap, text);
    vsprintf(buf, text, ap);
    va_end(ap);

    char *lineBegin = buf;
    char *lineEnd;

    do {
        // make room for the new message
        if ( gui_log.size() == Log_height ) {
            LogEntry *toRemove = gui_log.at(0);
            gui_log.erase(gui_log.begin());
            delete toRemove;
        }
        // detect end of the line
        lineEnd=strchr(lineBegin,'\n');
        if ( lineEnd ) {
            *lineEnd='\0';
        }
        // add a new message to the log
        LogEntry *msg = new LogEntry(lineBegin, col);
        gui_log.push_back(msg);
        // go to next line
        lineBegin = lineEnd+1;
   } while ( lineEnd );
}

void gui_render_mouse_look(TCODConsole *con, const GameMap &map, int mouse_x, int mouse_y) {
    if(!map.tcod_fov_map->isInFov(mouse_x, mouse_y)) {
        return;
    }

    std::string name_list = "";
    entity_iterate(create_mask<EntityFat>(), [&](const uint32_t &i) {
        auto &entity_fat = entity_fats[i];
        if(entity_fat.x == mouse_x && entity_fat.y == mouse_y) {
            if(name_list == "") {
                name_list = entity_fat.name;
            } else {
                name_list += ", " + entity_fat.name;
            }
        }
    });

    con->setDefaultForeground(TCODColor::lightGrey);
    con->print(1, 0, name_list.c_str());
}

void gui_render_menu(TCODConsole *con, std::string header, const std::vector<std::string> &options, 
    int width, int screen_width, int screen_height) {
    if(options.size() > 26) {
        engine_log(LogStatus::Error, "Cannot have a menu with more than 26 options");
    }
    
    // calculate total height for the header (after auto-wrap) and one line per option
    int header_height = con->getHeightRect(0, 0, width, screen_height, header.c_str());
    int height = options.size() + header_height;

    // create an off-screen console that represents the menu's window
    // SEEMS REALLY BAD TO KEEP CREATING NEW CONSOLE INSTANCES
    if(menu) {
        delete menu;
    }
    menu = new TCODConsole(width, height);

    // # print the header, with auto-wrap
    menu->setDefaultForeground(TCOD_white);
    menu->printRectEx(0, 0, width, height, TCOD_BKGND_NONE, TCOD_LEFT, header.c_str());

    int y = header_height;
    char letter_index = 'a';
    for(auto &o : options) {        
        menu->printEx(0, y, TCOD_BKGND_NONE, TCOD_LEFT, "(%c) %s", letter_index, o.c_str());
        y++;
        letter_index++;
    }

    int x = int(screen_width / 2 - width / 2);
    y = int(screen_height / 2 - height / 2);
    TCODConsole::blit(menu, 0, 0, width, height, con, x, y, 1.0f, 0.7f);
}

void gui_render_inventory(TCODConsole *con, const std::string header, Entity &player, int inventory_width, int screen_width, int screen_height) {
    std::vector<std::string> options;

    auto &player_inventory = get_inventory(player);
    if(player_inventory.items.size() == 0) {
        options.push_back("Inventory is empty.");
    } else {
        auto &player_equipment = get_equipment(player);
        for(auto item_entity : player_inventory.items) {
            auto &item = get_item(item_entity);

            if(entity_equals(player_equipment.main_hand, item_entity)) {
                options.push_back(item.name + " (in main hand)");
            } else if(entity_equals(player_equipment.off_hand, item_entity)) {
                options.push_back(item.name + " (in off hand)");
            } else {
                options.push_back(item.name);
            }
        }
    } 

    gui_render_menu(con, header, options, inventory_width, screen_width, screen_height);
}

void gui_render_main_menu(TCODConsole *con, int screen_width, int screen_height) {
    // static so only executed once
    static TCODImage img("data/menu_background.png");
    img.blit2x(con, 0, 0);
    
    con->setDefaultForeground(TCOD_light_yellow);
    con->printEx(screen_width / 2, (screen_height / 2) - 4, TCOD_BKGND_NONE, TCOD_CENTER, 
        "CASTLE OF THE DRAGON");
    con->printEx(screen_width / 2, (screen_height / 2) - 3, TCOD_BKGND_NONE, TCOD_CENTER, 
        "By Henke.");

    static std::vector<std::string> options = { "New Game", "Continue", "Quit" }; 
    gui_render_menu(con, "", options, 24, screen_width, screen_height);
}

void gui_render_level_up_menu(TCODConsole *con, std::string header, Entity &player, int menu_width, int screen_width, int screen_height) {
    auto &fighter = get_fighter(player);
    std::vector<std::string> options = {
        "Constitution | +20 HP, current: " + std::to_string(fighter.hp_max),
        "Strength | +1 attack, current: " + std::to_string(fighter.power_max),
        "Agility | +1 defense, current: " + std::to_string(fighter.defense_max)
    };

    gui_render_menu(con, header, options, menu_width, screen_width, screen_height);
}

void gui_render_character_screen(TCODConsole *con, Entity player, int character_screen_width, int character_screen_height,  
    int screen_width, int screen_height) {
    // create an off-screen console that represents the menu's window
    // SEEMS REALLY BAD TO KEEP CREATING NEW CONSOLE INSTANCES
    if(character_screen) {
        delete character_screen;
    }
    character_screen = new TCODConsole(character_screen_width, character_screen_height);

    auto &player_level = get_level(player_handle);
    auto &player_fighter = get_fighter(player_handle);

    character_screen->setDefaultForeground(TCOD_white);
    character_screen->printRectEx(0, 1, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Character Information");
    character_screen->printRectEx(0, 2, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Level: %d", player_level.current_level);
    character_screen->printRectEx(0, 3, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Experience: %d", player_level.current_xp);
    character_screen->printRectEx(0, 4, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Experience to level: %d", player_level.experience_to_next_level());
    character_screen->printRectEx(0, 6, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Maximum HP: %d", player_fighter.hp_max);
    character_screen->printRectEx(0, 7, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Attack: %d", player_fighter.power());
    character_screen->printRectEx(0, 8, character_screen_width, character_screen_height, TCOD_BKGND_NONE, TCOD_LEFT,
        "Defense: %d", player_fighter.defense());

    int x = screen_width / 2 - character_screen_width / 2;
    int y = screen_height / 2 - character_screen_height / 2;
    
    TCODConsole::blit(character_screen, 0, 0, character_screen_width, character_screen_height, 
                        con, x, y, 1.0f, 0.7f);
}

void new_game() {
    player_handle = entity_create();
    const auto c = entity_get_handle(player_handle);
    entity_add_component(c, entity_fats, 
        EntityFat(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, '@', TCODColor::white, "Player", true, render_priority.ENTITY));
    entity_add_component(c, fighters, Fighter(player_handle, 100, 1, 2));
    entity_add_component(c, inventories, Inventory(player_handle, 26));
    entity_add_component(c, levels, Level());
    entity_add_component(c, equipments, Equipment());

    auto starting_item = entity_create();
    const auto c_item = entity_get_handle(starting_item);
    entity_add_component(c_item, entity_fats, EntityFat(0, 0, '-', TCOD_sky, "Dagger", false, render_priority.ITEM));
    Item item;
    item.name = "Dagger";
    entity_add_component(c_item, items, item);
    entity_add_component(c_item, equippables, Equippable(MAIN_HAND, 2, 0, 0));
    entity_disable_component<EntityFat>(c_item);

    auto &player_inventory = get_inventory(player_handle);
    player_inventory.add_item(starting_item);
    auto &player_equipment = get_equipment(player_handle);
    player_equipment.toggle_equipment(starting_item);

    // generate map and fov
    game_map.tcod_fov_map = new TCODMap(Map_Width, Map_Height);
    // Should separate fov from map_generate (make_room)
    map_generate(game_map, Max_rooms, Room_min_size, Room_max_size, Map_Width, Map_Height);
    
    // Place player in first room
    rect_center(game_map.rooms[0], entity_fats[c.i].x, entity_fats[c.i].y);
    // Setup fov from players position
    game_map.tcod_fov_map->computeFov(entity_fats[c.i].x, entity_fats[c.i].y, fov_radius, fov_light_walls, fov_algorithm);

    // add entities to map
    map_add_monsters(game_map);
    map_add_items(game_map);
    map_add_stairs(game_map);
    
    game_state = PLAYER_TURN;    

    gui_log_message(TCOD_light_azure, "Welcome %s \nA throne is the most devious trap of them all..", entity_fats[c.i].name.c_str());
}

void next_floor(GameMap &map) {
    map.level += 1;
    
    std::vector<Entity> _entities_to_remove;

    for(size_t i = 1; i < num_entities; i++) {
        if(!entity_equals(player_handle, entities[i])) {
            _entities_to_remove.push_back(entities[i]);
        }
    }

    for(auto e : _entities_to_remove) {
        entity_remove(e);
    }
    
    game_map.rooms.clear();
    game_map.num_rooms = 0;
    delete game_map.tcod_fov_map;

    targeting_item = InvalidEntity;
    
    Tile t_base;
    for(int x = 0; x < Map_Width; x++) {
        for(int y = 0; y < Map_Height; y++) {
            game_map.tiles[map_index(x, y)] = t_base;   
        }    
    }

    // generate map and fov
    game_map.tcod_fov_map = new TCODMap(Map_Width, Map_Height);
    // Should separate fov from map_generate (make_room)
    map_generate(game_map, Max_rooms, Room_min_size, Room_max_size, Map_Width, Map_Height);
    
    auto &player_fat = get_entityfat(player_handle);
    // Place player in first room
    rect_center(game_map.rooms[0], player_fat.x, player_fat.y);
    // Setup fov from players position
    game_map.tcod_fov_map->computeFov(player_fat.x, player_fat.y, fov_radius, fov_light_walls, fov_algorithm);

    // add entities to map
    map_add_monsters(game_map);
    map_add_items(game_map);
    map_add_stairs(game_map);
    
    game_state = PLAYER_TURN;

    auto &player_fighter = get_fighter(player_handle);
    heal(player_fighter, player_fighter.hp_max / 2);

    events_queue({ EventType::Message, InvalidEntity, "You take a moment to rest, and recover your strength." });
}

struct RenderItem {
    int x;
    int y;
    TCODColor color;
    int gfx;
    int render_order;
    bool override_fov = false;
};

bool entity_render_sort(const RenderItem &first, const RenderItem &second) {
    return first.render_order < second.render_order;
}

void render_game() {
    for(int y = 0; y < Map_Height; y++) {
        for(int x = 0; x < Map_Width; x++) {
            if (game_map.tcod_fov_map->isInFov(x, y)) {
                game_map.tiles[map_index(x, y)].explored = true;

                root_console->setCharBackground(x, y,
                    game_map.tiles[map_index(x, y)].block_sight ? color_table.light_wall : color_table.light_ground );
            } else if ( game_map.tiles[map_index(x, y)].explored ) {
                root_console->setCharBackground(x,y,
                    game_map.tiles[map_index(x, y)].block_sight ? color_table.dark_wall : color_table.dark_ground );
            }
        }
    }

    std::vector<RenderItem> render_items;
    render_items.reserve(MAX_ENTITIES);

    entity_iterate(create_mask<EntityFat>(), [&](const uint32_t &i) {
        render_items.push_back({
            entity_fats[i].x,
            entity_fats[i].y,
            entity_fats[i].color,
            entity_fats[i].gfx,
            entity_fats[i].render_order,
            masks[i].test(ComponentID::value<Stairs>())
        });
    });
    std::sort(render_items.begin(), render_items.end(), entity_render_sort);    

    for(auto &ri : render_items) {
        if((ri.override_fov && game_map.tiles[map_index(ri.x, ri.y)].explored)
            || game_map.tcod_fov_map->isInFov(ri.x, ri.y)) {
            root_console->setDefaultForeground(ri.color);
            root_console->putChar(ri.x, ri.y, ri.gfx);
        }
    }

    bar->setDefaultBackground(TCODColor::black);
    bar->clear();

    ComponentHandle c = entity_get_handle(player_handle);
    Fighter *player_fighter = &fighters[c.i];        
    gui_render_bar(bar, 1, 1, Bar_width, "HP", player_fighter->hp, player_fighter->hp_max, TCOD_light_red, TCOD_darker_red);
    gui_render_mouse_look(bar, game_map, mouse.cx, mouse.cy);
    bar->printEx(1, 3, TCOD_BKGND_NONE, TCOD_LEFT, "Dungeon level: %d", game_map.level);
    
    float colorCoef = 0.4f;
    for(size_t i = 0, y = 1; i < gui_log.size(); i++, y++) {
        bar->setDefaultForeground(gui_log[i]->color * colorCoef);
        bar->print(Log_x, y, gui_log[i]->text);
        // could one-line this with a clamp;
        if (colorCoef < 1.0f ) {
            colorCoef += 0.3f;
        }
    }

    TCODConsole::blit(bar, 0, 0, SCREEN_WIDTH, Panel_height, root_console, 0, Panel_y);
}


// States --
struct State {
    virtual void update() = 0;
    virtual void render() = 0;
};

std::unordered_map<GameState, State*> states;

///

struct MainMenu : State {
    void update() override {
        int index = (int)input_key_char_get() - (int)'a';
        if(index == 0) {    
            new_game();
        } else if(index == 1) {
            engine_log(LogStatus::Information, "Continue is not implemented (only show if available)");
        } else if(index == 2) {
            engine_exit();
        }

        if(input_key_pressed(TCODK_ESCAPE)) {
            engine_exit();
        } else if(input_key_pressed(TCODK_ENTER) && input_lalt()) {
            TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
        }
    };

    void render() override {
        gui_render_main_menu(root_console, SCREEN_WIDTH, SCREEN_HEIGHT);
    };
};

struct GamePlay : State {
    struct Movement { int x; int y; };

    void update() override {
        if(input_key_pressed(TCODK_ESCAPE)) {
            engine_exit();
        } else if(input_key_pressed(TCODK_ENTER) && input_lalt()) {
            TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
        }

        if(game_state == PLAYER_TURN) {
            Movement m = { 0, 0 };
            bool pickup = false;
            bool take_stairs = false;
            if(key.vk == TCODK_UP) {
                m.y = -1;
            } else if(key.vk == TCODK_DOWN) {
                m.y = 1;
            } else if(key.vk == TCODK_LEFT) {
                m.x = -1;
            } else if(key.vk == TCODK_RIGHT) {
                m.x = 1;
            } else if(key.c == 'r') {
                m.x = 1;
                m.y = -1;
            } else if(key.c == 'e') {
                m.x = -1;
                m.y = -1;
            } else if(key.c == 'd') {
                m.x = -1;
                m.y = 1;
            } else if(key.c == 'f') {
                m.x = 1;
                m.y = 1;
            } else if(key.c == 'z') {
                game_state = ENEMY_TURN;
            } else if(key.c == 'g') {
                pickup = true;
            } else if(key.c == 'i') {
                previous_game_state = game_state;
                game_state = SHOW_INVENTORY;
            } else if(key.c == 'c') {
                previous_game_state = game_state;
                game_state = CHARACTER_SCREEN;
            } else if(key.vk == TCODK_ENTER) {
                take_stairs = true;
            }

            auto &player = get_entityfat(player_handle);
            int dx = player.x + m.x, dy = player.y + m.y; 
            if((m.x != 0 || m.y != 0) && !map_blocked(game_map, dx, dy)) {
                Entity target;
                if(entity_blocking_at(dx, dy, target)) {
                    attack(player_handle, target);
                } else {
                    player.x = dx;
                    player.y = dy;
                    game_map.tcod_fov_map->computeFov(player.x, player.y, fov_radius, fov_light_walls, fov_algorithm);
                }

                game_state = ENEMY_TURN;
            } else if(pickup) {
                entity_iterate(create_mask<EntityFat, Item>(), [&](const uint32_t &i) {
                    if(player.x == entity_fats[i].x && player.y == entity_fats[i].y) {
                        events_queue({ EventType::ItemPickup, entities[i] });
                        game_state = ENEMY_TURN;
                        pickup = false;
                    }
                });
                // if we still want to pickup after we checked entities there is nothing to pickup
                if(pickup) {
                    events_queue({ EventType::Message, Entity(), "There is nothing here to pick up.", TCOD_yellow });
                }
            } else if(take_stairs) {
                entity_iterate(create_mask<EntityFat, Stairs>(), [&](const uint32_t &i) {
                    if(player.x == entity_fats[i].x && player.y == entity_fats[i].y) {
                        events_queue({ EventType::NextFloor, entities[i] });
                        game_state = ENEMY_TURN;
                        take_stairs = false;
                    }
                });
                if(take_stairs) {
                    events_queue({ EventType::Message, InvalidEntity, "There are no stairs here.", TCOD_yellow });
                }
            }    
        } else {
            entity_iterate(create_mask<EntityFat, Fighter, Ai>(), [&](const uint32_t &i) {
                if(!entity_equals(entities[i], player_handle)) {
                    ais[i].take_turn(player_handle, game_map);
                }
            });
            game_state = PLAYER_TURN;
        }
    }

    void render() override {
        render_game();
    }
};

struct PlayerDead : State {
    void update() override {
        if(input_key_pressed('i')) {
            previous_game_state = game_state;
            game_state = SHOW_INVENTORY;
        } else if(input_key_pressed(TCODK_ESCAPE)) {
            engine_exit();
        }
    };

    void render() override {
        render_game();
    };
};

struct InventoryState : State {
    void update() override {
        int index = (int)key.c - (int)'a';
        auto &player_inventory = get_inventory(player_handle);
        if(index >= 0 && previous_game_state != PLAYER_DEAD && index < (int)player_inventory.items.size()) {
            if(key.lalt) {
                // DROP ITEM
                auto item_entity = player_inventory.items[index];
                auto handle = entity_get_handle(item_entity);
                entity_enable_component<EntityFat>(handle);
                auto &entity_fat = get_entityfat(item_entity);
                auto &player_fat = get_entityfat(player_handle);
                entity_fat.x = player_fat.x;
                entity_fat.y = player_fat.y;
                player_inventory.remove(item_entity);
                
                auto &player_equipment = get_equipment(player_handle);
                if(entity_equals(player_equipment.main_hand, item_entity) 
                        || entity_equals(player_equipment.off_hand, item_entity)) {
                    player_equipment.toggle_equipment(item_entity);
                }
                game_state = ENEMY_TURN;
            } else {
                if(player_inventory.requires_target(index)) {
                    targeting_item = player_inventory.items[index];
                    previous_game_state = PLAYER_TURN;
                    game_state = TARGETING;
                    auto &item = get_item(targeting_item);
                    events_queue({ EventType::Message, InvalidEntity, item.targeting_message, TCOD_yellow });   
                } else {
                    Context context = Context(game_map);
                    bool consumed = player_inventory.use(index, context);
                    game_state = ENEMY_TURN;
                }
            }
        }
        
        if(key.vk == TCODK_ESCAPE) {
            game_state = previous_game_state;
        }
    };

    void render() override {
        render_game();
        gui_render_inventory(root_console, 
            "Press the key next to an item to use it (hold alt to drop), or Esc to cancel.\n", 
            player_handle, 50, SCREEN_WIDTH, SCREEN_HEIGHT);
    };
};

struct TargetingState : State {
    void update() override {
        int x = mouse.cx, y = mouse.cy;            
        if(mouse.lbutton_pressed) {
            auto &item = get_item(targeting_item);
            //targeting_item->item->args.target_x
            item.args.target_x = x;
            item.args.target_y = y;
            Context context = Context(game_map);
            auto &player_inventory = get_inventory(player_handle);
            if(player_inventory.use(targeting_item, context)) {
                game_state = ENEMY_TURN;
            }
        } else if(key.vk == TCODK_ESCAPE || mouse.rbutton_pressed) {
            game_state = previous_game_state;
            events_queue({ EventType::Message, InvalidEntity, "Targeting cancelled", TCOD_yellow });   
        }
    };

    void render() override {
        render_game();
    };
};

struct LevelUpState : State {
    void update() override {
        auto &player_fighter = get_fighter(player_handle);
        char key_char = key.c;
        if(key_char == 'a') {
            player_fighter.hp_max += 20;
            player_fighter.hp += 20;
            game_state = previous_game_state;
        } else if(key_char == 'b') {
            player_fighter.power_max += 1;
            game_state = previous_game_state;
        } else if(key_char == 'c') {
            player_fighter.defense_max += 1;
            game_state = previous_game_state;
        }
    };

    void render() override {
        render_game();
        gui_render_level_up_menu(root_console, "Level up! Choose a stat to raise:", player_handle, 40, SCREEN_WIDTH, SCREEN_HEIGHT);
    };
};

struct CharacterScreenState : State {
    void update() override {
        if(input_key_pressed(TCODK_ESCAPE)) {
            game_state = previous_game_state;
        }
    };

    void render() override {
        render_game();
        gui_render_character_screen(root_console, player_handle, 30, 10, SCREEN_WIDTH, SCREEN_HEIGHT);
    };
};

int main( int argc, char *argv[] ) {
    //test_entities();

    // Setup ECS
    on_entity_created = entity_created;
    on_entity_removed = entity_removed;

    srand((unsigned int)time(NULL));

    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);

    root_console = TCODConsole::root;
    bar = new TCODConsole(SCREEN_WIDTH, Panel_height);

    states.insert({ MAIN_MENU, new MainMenu() });
    auto game_play = new GamePlay();
    states.insert({ PLAYER_TURN, game_play });
    states.insert({ ENEMY_TURN, game_play });
    states.insert({ PLAYER_DEAD, new PlayerDead() });
    states.insert({ SHOW_INVENTORY , new InventoryState() });
    states.insert({ TARGETING, new TargetingState() });
    states.insert({ LEVEL_UP, new LevelUpState() });
    states.insert({ CHARACTER_SCREEN, new CharacterScreenState() });

    while ( !TCODConsole::isWindowClosed() && engine_running ) {
        input_update();
        
        //// UPDATE
        states[game_state]->update();
        
        // EVENTS
        for(auto &e : _event_queue) {
            switch(e.type) {
                case EventType::Message: {
                    gui_log_message(e.color, e.message.c_str());
                    break;
                }
                case EventType::EntityDead: {
                    if(entity_equals(e.entity, player_handle)) {
                        auto &player = get_entityfat(e.entity);
                        
                        gui_log_message(TCOD_red, "YOU died!");
                        game_state = PLAYER_DEAD;
                        player.gfx = '%';
                        player.color = TCOD_dark_red;
                        player.render_order = render_priority.CORPSE;
                    } else {
                        ComponentHandle c = entity_get_handle(e.entity);
                        auto &entity_fat = entity_fats[c.i];
                        auto &entity_fighter = fighters[c.i];

                        gui_log_message(TCOD_light_green, "%s died!", entity_fat.name.c_str());
                        
                        auto &player_level = get_level(player_handle);
                        auto xp_gained = entity_fighter.xp;
                        bool leveled_up = player_level.add_xp(xp_gained);
                        gui_log_message(TCOD_yellow, "You gain %d experience points.", xp_gained);
                        
                        if(leveled_up) {
                            gui_log_message(TCOD_yellow, "You become stronger! You reached level %d", player_level.current_level);
                            previous_game_state = game_state;
                            game_state = LEVEL_UP;
                        }

                        entity_fat.gfx = '%';
                        entity_fat.color = TCOD_dark_red;
                        entity_fat.render_order = render_priority.CORPSE;
                        entity_fat.blocks = false;
                        entity_fat.name = "remains of " + entity_fat.name;

                        entity_remove_component(c, ComponentID::value<Fighter>());
                        // entity_remove_component<Ai>(e.entity);
                    }
                    break;
                }
                case EventType::ItemPickup: {
                    auto &player_inventory = get_inventory(player_handle);
                    auto success = player_inventory.add_item(e.entity);
                    if(success) {
                        auto &item = get_item(e.entity);
                        gui_log_message(TCOD_yellow, "You picked up the %s !", item.name.c_str());
                    } else {
                        gui_log_message(TCOD_yellow, "You cannot carry anymore, inventory full");
                    }
                    const auto &c = entity_get_handle(e.entity);
                    entity_disable_component<EntityFat>(c);
                    break;
                }
                case EventType::NextFloor: {
                    next_floor(game_map);
                    break;
                }
                case EventType::EquipmentChange: {
                    auto &item = get_item(e.entity);
                    if(e.flag == 0) {
                        gui_log_message(TCOD_yellow, "You dequipped the %s", item.name.c_str());
                    } else if(e.flag == 1) {
                        gui_log_message(TCOD_yellow, "You equipped the %s", item.name.c_str());
                    }
                    break;
                }
            }
        }
        _event_queue.clear();

        //// RENDER
        
        root_console->setDefaultForeground(TCODColor::white);
        root_console->clear();

        states[game_state]->render();

        TCODConsole::flush();
    }

    return 0;
}