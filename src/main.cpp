#include "libtcod.hpp"
#include <iostream>
#include <unordered_map>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>


// http://rogueliketutorials.com/tutorials/tcod/part-8/

// Skipped things:
// - A* movement for monsters (Part 6)

// Known bugs:
// - Sometimes pickups are not removed when picked up (possible pointer shit?)

// http://www.roguebasin.com/index.php?title=Complete_roguelike_tutorial_using_C%2B%2B_and_libtcod_-_part_2:_map_and_actors
// 

// ECS
// https://blog.therocode.net/2018/08/simplest-entity-component-system
// https://austinmorlan.com/posts/entity_component_system/
TCODMap *tcod_fov_map;

int rand_int(int min, int max) {
    int lower = min;
    int upper = max;
    return (rand() % (upper - lower + 1)) + lower; 
}

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
    DROP_INVENTORY
};

enum EventType {
    Message,
    EntityDead,
    ItemPickup
};
struct Entity;
struct Event {
    EventType type;
    Entity *entity = NULL;
    std::string message;
    TCOD_color_t color;
};

std::vector<Event> _event_queue; 
void events_queue(Event e) {
    _event_queue.push_back(e);
}

struct Movement {
    int x, y;
};

struct Fighter;
struct Ai;
struct Inventory;
struct Item;

struct RenderPriority {
    // lower is lower prio
    int CORPSE = 0;
    int ITEM = 1;
    int ENTITY = 2;
} render_priority;

struct Entity {
    int x,y;
    int gfx;
    TCODColor color;
    std::string name;
    bool blocks = false;
    int render_order = 0;

    Entity(int x_, int y_, int gfx_, TCODColor color_, std::string name_, bool blocks_, int render_order_) : 
        x(x_), y(y_), gfx(gfx_), color(color_), name(name_), blocks(blocks_), render_order(render_order_) {
    }

    // components
    Fighter *fighter = NULL;
    Ai *ai = NULL;
    Inventory *inventory = NULL;
    Item *item = NULL;
};

struct Item {
    std::string name;
};

struct Inventory {
    int capacity;
    std::vector<Item> items;
    Inventory(int capacity) : capacity(capacity) {}

    bool add_item(Item i) {
        if(items.size() >= capacity) {
            return false;
        } 
        
        items.push_back(i);
        return true;
    }
};

struct Fighter {
    int hp;
    int hp_max;
    int defense;
    int power;
    Entity *_owner;
    
    Fighter(Entity* owner, int hp_, int defense_, int power_) 
        : hp(hp_), hp_max(hp_), defense(defense_), power(power_), _owner(owner) {}

    void take_damage(int amount) {
        hp -= amount;

        if(hp <= 0) {
            events_queue({ EventType::EntityDead, _owner });
        }
    }

    void attack(Entity *entity) {
        int damage = power - entity->fighter->defense;

        if(damage > 0) {
            // VERY SHITTY STRING ALLOCATION
            char buffer[255];
            sprintf(buffer, "%s attacks %s for %d hit points", _owner->name.c_str(), entity->name.c_str(), damage);
            events_queue({ EventType::Message, NULL, buffer, TCOD_amber });

            entity->fighter->take_damage(damage);
        } else {
            // VERY SHITTY STRING ALLOCATION
            char buffer[255];
            sprintf(buffer, "%s attacks %s but deals no damage", _owner->name.c_str(), entity->name.c_str());
            events_queue({ EventType::Message, NULL, buffer, TCOD_light_grey });
        }
    }
};

struct Ai {
    virtual void take_turn(Entity *target) = 0;
};


void move_towards(Entity *entity, int target_x, int target_y);
float distance_to(int x, int y, int target_x, int target_y) {
    int dx = target_x - x;
    int dy = target_y - y;
    return sqrtf(dx*dx + dy*dy);
}

struct BasicMonster : Ai {
    Entity *_owner;
    void take_turn(Entity *target) override {
        if(tcod_fov_map->isInFov(_owner->x, _owner->y)) {
            if(distance_to(_owner->x, _owner->y, target->x, target->y) >= 2) {

                // CAN REPLACE HITS WITH ASTAR MOVEMENT

                move_towards(_owner, target->x, target->y);
            } else if(target->fighter->hp > 0) {
                _owner->fighter->attack(target);
                //printf("Deal damage to %s.\n", target->name);
            }

        }
    }

    BasicMonster(Entity *owner) {
        _owner = owner;
    }
};

struct Tile {
    bool blocked = true;
    bool block_sight = true;
    bool explored = false;
};

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

// UI
const int Bar_width = 20;
const int Panel_height = 7;
const int Panel_y = SCREEN_HEIGHT - Panel_height;
//

const int MAX_ENTITIES = 1000;
std::vector<Entity*> _entities;

const int Map_Width = 80;
const int Map_Height = 43;
const int Room_max_size = 10;
const int Room_min_size = 6;
const int Max_rooms = 30;
Tile _map[Map_Width * Map_Height];

int num_rooms = 0;
std::vector<Rect> rooms;

const TCOD_fov_algorithm_t fov_algorithm = FOV_BASIC;
const bool fov_light_walls = true;
const int fov_radius = 10;
bool fov_recompute = true;

const int Max_monsters_per_room = 3;
const int Max_items_per_room = 2;

int map_index(int x, int y) {
    return x + Map_Width * y;
}

bool map_blocked(int x, int y) {
    if(_map[map_index(x, y)].blocked) {
        return true;
    }
    return false;
}

void map_make_room(const Rect &room) {
    for(int x = room.x + 1; x < room.x2; x++) {
        for(int y = room.y + 1; y < room.y2; y++) {
            _map[map_index(x, y)].blocked = false;
            _map[map_index(x, y)].block_sight = false;   
            tcod_fov_map->setProperties(x, y, true, true);
        }    
    }
//     for x in range(room.x1 + 1, room.x2):
// +           for y in range(room.y1 + 1, room.y2):
// +               self.tiles[x][y].blocked = False
// +               self.tiles[x][y].block_sight = False

}

void map_make_h_tunnel(int x1, int x2, int y) {
    for(int x = std::min(x1, x2); x < std::max(x1, x2) + 1; x++) {
        _map[map_index(x, y)].blocked = false;
        _map[map_index(x, y)].block_sight = false;
        tcod_fov_map->setProperties(x, y, true, true);
    }
//     def create_h_tunnel(self, x1, x2, y):
// +       for x in range(min(x1, x2), max(x1, x2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}
void map_make_v_tunnel(int y1, int y2, int x) {
    for(int y = std::min(y1, y2); y < std::max(y1, y2) + 1; y++) {
        _map[map_index(x, y)].blocked = false;
        _map[map_index(x, y)].block_sight = false;
        tcod_fov_map->setProperties(x, y, true, true);
    }
// +   def create_v_tunnel(self, y1, y2, x):
// +       for y in range(min(y1, y2), max(y1, y2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}

void map_generate(int max_rooms, int room_min_size, int room_max_size, int map_width, int map_height) {
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
        for(auto &r : rooms) {
            if(rect_intersects(r, new_room)) {
                intersects = true;
                break;
            }
        }

        if(intersects) {
            continue;
        }

        // "paint" it to the map's tiles
        map_make_room(new_room);
        int new_x, new_y;
        rect_center(new_room, new_x, new_y);

        if(num_rooms > 0) {
            int prev_x, prev_y;
            rect_center(rooms[num_rooms - 1], prev_x, prev_y);
            if(rand_int(0, 1) == 1) {
                map_make_h_tunnel(prev_x, new_x, prev_y);
                map_make_v_tunnel(prev_y, new_y, new_x);
            } else {
                map_make_v_tunnel(prev_y, new_y, new_x);
                map_make_h_tunnel(prev_x, new_x, prev_y);
            }
        }

        rooms.push_back(new_room);
        num_rooms++;
    }
}

void map_add_monsters(int max_monsters_per_room) {
    for(const Rect &room : rooms) {
        int number_of_monsters = rand_int(0, max_monsters_per_room);
        for(int i = 0; i < number_of_monsters; i++) {
            int x = rand_int(room.x + 1, room.x2 - 1); 
            int y = rand_int(room.y + 1, room.y2 - 1);

            bool occupied = false;
            for(int ei = 0; ei < _entities.size(); ei++) {
                Entity *e = _entities[ei];
                if(e->x == x && e->y == y) {
                    occupied = true;
                    break;
                }
            }
            if(occupied) {
                continue;
            }

            Entity *e;
            if(rand_int(0, 100) < 80) {
                e = new Entity(x, y, 'o', TCOD_desaturated_green, "Orc", true, render_priority.ENTITY );
                e->fighter = new Fighter(e, 10, 0, 3);
                e->ai = new BasicMonster(e);
            } else {
                e = new Entity(x, y, 'T', TCOD_darker_green, "Troll", true, render_priority.ENTITY );
                e->fighter = new Fighter(e, 16, 1, 4);
                e->ai = new BasicMonster(e);
            }
            _entities.push_back(e);            
        }
    }
}

void map_add_items(int max_items_per_room) {
    for(const Rect &room : rooms) {
        int number_of_monsters = rand_int(0, max_items_per_room);
        for(int i = 0; i < number_of_monsters; i++) {
            int x = rand_int(room.x + 1, room.x2 - 1); 
            int y = rand_int(room.y + 1, room.y2 - 1);

            bool occupied = false;
            for(int ei = 0; ei < _entities.size(); ei++) {
                Entity *e = _entities[ei];
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
            Entity *e;
            e = new Entity(x, y, '!', TCOD_violet, "Health potion", false, render_priority.ITEM );
            e->item = new Item();
            e->item->name = "Health potion";
            _entities.push_back(e);            
        }
    }
}

bool entity_blocking_at(int x, int y, Entity **found_entity) {
    for(int i = 0; i < _entities.size(); i++) {
        auto entity = _entities[i];
        if(x == entity->x && y == entity->y && entity->blocks) {
            *found_entity = _entities[i];
            return true;
        }
    }
    return false;
}

bool can_walk(int x, int y) {
    Entity *target;
    return !map_blocked(x, y) && !entity_blocking_at(x, y, &target);
}

void move_towards(Entity *entity, int target_x, int target_y) {
    int dx, dy;
    dx = target_x - entity->x;
    dy = target_y - entity->y;
    float distance = sqrtf(dx*dx+dy*dy);
    
    dx = (int)(round(dx/distance));
    dy = (int)(round(dy/distance));

    if(can_walk(entity->x + dx, entity->y + dy)) {
        entity->x = entity->x + dx;
        entity->y = entity->y + dy;
    } else if(can_walk(entity->x + dx, entity->y)) {
        entity->x = entity->x + dx;
    } else if(can_walk(entity->x, entity->y + dy)) {
        entity->y = entity->y + dy;
    }
}

bool entity_render_sort(const Entity *first, const Entity *second) {
    return first->render_order < second->render_order;
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

    // TCOD_console_set_default_background(panel, back_color);
    // TCOD_console_rect(panel, x, y, total_width, 1, false, TCOD_BKGND_SCREEN);

    // TCOD_console_set_default_background(panel, bar_color);
    // if(bar_width > 0) {
    //     TCOD_console_rect(panel, x, y, bar_width, 1, false, TCOD_BKGND_SCREEN);
    // }

    // TCOD_console_set_default_foreground(panel, TCOD_white);
    // TCOD_console_print_ex(panel, x + total_width / 2, y, TCOD_BKGND_NONE, TCOD_CENTER, "%s: %d/%d", name.c_str(), value, maximum);
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

void gui_render_mouse_look(TCODConsole *con, int mouse_x, int mouse_y) {
    if(!tcod_fov_map->isInFov(mouse_x, mouse_y)) {
        return;
    }

    std::string name_list = "";
    for(size_t i = 0; i < _entities.size(); i++) {
        auto entity = _entities[i];
        if(entity->x == mouse_x && entity->y == mouse_y) {
            if(name_list == "") {
                name_list = entity->name;
            } else {
                name_list += ", " + entity->name;
            }
        }
    }

    con->setDefaultForeground(TCODColor::lightGrey);
    con->print(1, 0, name_list.c_str());
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

TCODConsole *menu;
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
    TCODConsole::blit(menu, 0, 0, width, height, con, x, y, 1.0, 0.7);
}

void gui_render_inventory(TCODConsole *con, const std::string header, const Inventory &inventory, int inventory_width, int screen_width, int screen_height) {
    std::vector<std::string> options;
    if(inventory.items.size() == 0) {
        options.push_back("Inventory is empty.");
    } else {
        for(auto &item : inventory.items) {
            options.push_back(item.name);
        }
    } 

    gui_render_menu(con, header, options, inventory_width, screen_width, screen_height);
}

int main( int argc, char *argv[] ) {
    srand((unsigned int)time(NULL));

    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);
    TCOD_key_t key = {TCODK_NONE,0};
    TCOD_mouse_t mouse;
     
    auto root_console = TCODConsole::root;
    auto bar = new TCODConsole(SCREEN_WIDTH, Panel_height);

    // _entities.reserve(1000);
    _entities.push_back(new Entity(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, '@', TCODColor::white, "Player", true, render_priority.ENTITY));
    
    Entity *player = _entities[0];
    player->fighter = new Fighter(player, 30, 2, 5);
    player->inventory = new Inventory(26);
    
    // generate map and fov
    tcod_fov_map = new TCODMap(Map_Width, Map_Height);
    // Should separate fov from map_generate (make_room)
    map_generate(Max_rooms, Room_min_size, Room_max_size, Map_Width, Map_Height);
    
    // Place player in first room
    rect_center(rooms[0], player->x, player->y);
    // Setup fov from players position
    tcod_fov_map->computeFov(player->x, player->y, fov_radius, fov_light_walls, fov_algorithm);

    // add entities to map
    map_add_monsters(Max_monsters_per_room);
    map_add_items(Max_items_per_room);

    GameState game_state = PLAYER_TURN;
    GameState previous_game_state;

    gui_log_message(TCOD_light_azure, "Welcome %s \nA throne is the most devious trap of them all..", player->name.c_str());

    while ( !TCODConsole::isWindowClosed() ) {
        TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE, &key, &mouse);

        //// INPUT
        
        Movement m = { 0, 0 };
        bool pickup = false;
        if(game_state == PLAYER_TURN) {    
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
            } else if(key.c == 'g') {
                pickup = true;
            } else if(key.c == 'i') {
                previous_game_state = game_state;
                game_state = SHOW_INVENTORY;
            } else if(key.vk == TCODK_ESCAPE) {
                return 0;
            } else if(key.vk == TCODK_ENTER) {
                if(key.lalt) {
                    TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
                }
            }
        } else if(game_state == PLAYER_DEAD) {
            if(key.c == 'i') {
                previous_game_state = game_state;
                game_state = SHOW_INVENTORY;
            } else if(key.vk == TCODK_ESCAPE) {
                return 0;
            } else if(key.vk == TCODK_ENTER) {
                if(key.lalt) {
                    TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
                }
            }
        } else if(game_state == SHOW_INVENTORY) {
            int index = (int)key.c - (int)'a';
            if(index >= 0 && previous_game_state != PLAYER_DEAD && index < player->inventory->items.size()) {
                auto &item = player->inventory->items[index];
                engine_log(LogStatus::Information, item.name);  
            }
            
            if(key.vk == TCODK_ESCAPE) {
                game_state = previous_game_state;
            } else if(key.vk == TCODK_ENTER) {
                if(key.lalt) {
                    TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
                }
            }
        }
         

        //// UPDATE

        if(game_state == PLAYER_TURN) {
            int dx = player->x + m.x, dy = player->y + m.y; 
            if((m.x != 0 || m.y != 0) && !map_blocked(dx, dy)) {
                Entity *target;
                if(entity_blocking_at(dx, dy, &target)) {
                    player->fighter->attack(target);
                } else {
                    player->x = dx;
                    player->y = dy;
                    tcod_fov_map->computeFov(player->x, player->y, fov_radius, fov_light_walls, fov_algorithm);
                }

                game_state = ENEMY_TURN;
            } else if(pickup) {
                for(int i = 0; i < _entities.size(); i++) {
                    auto entity = _entities[i];
                    if(player->x == entity->x && player->y == entity->y && entity->item) {
                        events_queue({ EventType::ItemPickup, entity });
                        game_state = ENEMY_TURN;
                        pickup = false;
                        break;  
                    }
                }
                // if we still want to pickup after we checked entities there is nothing to pickup
                if(pickup) {
                    events_queue({ EventType::Message, NULL, "There is nothing here to pick up.", TCOD_yellow });                    
                }
            }
        } else if(game_state == ENEMY_TURN) {
            for(int i = 1; i < _entities.size(); i++) {
                const auto entity = _entities[i];
                if(entity->ai) {
                    entity->ai->take_turn(player);
                }
            }

           game_state = PLAYER_TURN;
        }

        // EVENTS
        for(auto &e : _event_queue) {
            switch(e.type) {
                case EventType::Message: {
                    gui_log_message(e.color, e.message.c_str());
                    //printf("|| %s \n", e.message.c_str());
                    break;
                }
                case EventType::EntityDead: {
                    // shitty way to know if player died
                    if(e.entity == player) {
                        gui_log_message(TCOD_red, "YOU died!");
                        game_state = PLAYER_DEAD;
                        player->gfx = '%';
                        player->color = TCOD_dark_red;
                        player->render_order = render_priority.CORPSE;
                    } else {
                        gui_log_message(TCOD_light_green, "%s died!", e.entity->name.c_str());
                        e.entity->gfx = '%';
                        e.entity->color = TCOD_dark_red;
                        e.entity->render_order = render_priority.CORPSE;
                        e.entity->blocks = false;
                        delete e.entity->fighter;
                        e.entity->fighter = NULL;
                        delete e.entity->ai;
                        e.entity->ai = NULL;
                        e.entity->name = "remains of " + e.entity->name;                    
                    }
                    break;
                }
                case EventType::ItemPickup: {
                    auto success = player->inventory->add_item(*e.entity->item);
                    if(success) {
                        gui_log_message(TCOD_yellow, "You picked up the %s !", e.entity->item->name.c_str());
                    } else {
                        gui_log_message(TCOD_yellow, "You cannot carry anymore, inventory full");
                    }
                    int delete_count = 0;
                    for(auto &ev : _entities) {
                        if(ev == e.entity) {
                            delete ev;
                            ev = nullptr;
                            delete_count++;
                            break;
                        }
                    }
                    if(delete_count == 0) {
                        engine_log(LogStatus::Error, "NO ITEM REMOVED WHEN PICKED UP!");

                    }
                    _entities.erase(std::remove(_entities.begin(), _entities.end(), nullptr), _entities.end());
                    break;
                }
            }
        }
        _event_queue.clear();

        //// RENDER

        root_console->setDefaultForeground(TCODColor::white);
        root_console->clear();

        for(int y = 0; y < Map_Height; y++) {
            for(int x = 0; x < Map_Width; x++) {
                if (tcod_fov_map->isInFov(x, y)) {
                    _map[map_index(x, y)].explored = true;

                    TCODConsole::root->setCharBackground(x, y,
                        _map[map_index(x, y)].block_sight ? color_table.light_wall : color_table.light_ground );
                } else if ( _map[map_index(x, y)].explored ) {
                    TCODConsole::root->setCharBackground(x,y,
                        _map[map_index(x, y)].block_sight ? color_table.dark_wall : color_table.dark_ground );
                }
            }
        }

        std::sort(_entities.begin(), _entities.end(), entity_render_sort);

        for(int i = 0; i < _entities.size(); i++) {
            const auto entity = _entities[i];
            if(!tcod_fov_map->isInFov(entity->x, entity->y)) {
                continue;
            }
            root_console->setDefaultForeground(entity->color);
            root_console->putChar(entity->x, entity->y, entity->gfx);
        }

        // UI RENDER
        bar->setDefaultBackground(TCODColor::black);
        bar->clear();
        gui_render_bar(bar, 1, 1, Bar_width, "HP", player->fighter->hp, player->fighter->hp_max, TCOD_light_red, TCOD_darker_red);
        gui_render_mouse_look(bar, mouse.cx, mouse.cy);

        float colorCoef = 0.4f;
        for(int i = 0, y = 1; i < gui_log.size(); i++, y++) {
            bar->setDefaultForeground(gui_log[i]->color * colorCoef);
            bar->print(Log_x, y, gui_log[i]->text);
            // could one-line this with a clamp;
            if (colorCoef < 1.0f ) {
                colorCoef += 0.3f;
            }
        }

        TCODConsole::blit(bar, 0, 0, SCREEN_WIDTH, Panel_height, root_console, 0, Panel_y);

        if(game_state == SHOW_INVENTORY) {
            gui_render_inventory(root_console, "Press the key next to an item to use it, or Esc to cancel.\n", *player->inventory, 50, SCREEN_WIDTH, SCREEN_HEIGHT);
        } else if(game_state == DROP_INVENTORY) {
            gui_render_inventory(root_console, "Press the key next to an item to drop it, or Esc to cancel.\n", *player->inventory, 50, SCREEN_WIDTH, SCREEN_HEIGHT);
        }

        TCODConsole::flush();
    }

    return 0;
}