#include "libtcod.hpp"
#include <iostream>
#include <time.h>
#include <stdlib.h>

// 
// http://rogueliketutorials.com/tutorials/tcod/part-4/

// http://www.roguebasin.com/index.php?title=Complete_roguelike_tutorial_using_C%2B%2B_and_libtcod_-_part_2:_map_and_actors
// 
// These differ somewhat

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

struct Movement {
    int x, y;
};

struct Entity {
    int x,y;
    int gfx;
    TCODColor color;
};
Entity entity_make(int x, int y, int gfx, TCODColor color) {
    return { x, y, gfx, color };
}

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

std::vector<Entity> _entities;

const int Map_Width = 80;
const int Map_Height = 50;
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
TCODMap *tcod_fov_map;

const int Max_monsters_per_room = 3;

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
    }
// +   def create_v_tunnel(self, y1, y2, x):
// +       for y in range(min(y1, y2), max(y1, y2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}

void map_generate(int max_rooms, int room_min_size, int room_max_size, int map_width, int map_height, Entity &player) {
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

        if(num_rooms == 0) {
            player.x = new_x;
            player.y = new_y;
        } else {
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

void map_add_entities(int max_monsters_per_room) {
    for(const Rect &room : rooms) {
        int number_of_monsters = rand_int(0, max_monsters_per_room);
        for(int i = 0; i < number_of_monsters; i++) {
            int x = rand_int(room.x + 1, room.x2 - 1); 
            int y = rand_int(room.y + 1, room.y2 - 1);

            bool occupied = false;
            for(auto &e : _entities) {
                if(e.x == x && e.y == y) {
                    occupied = true;
                    break;
                }
            }
            if(occupied) {
                continue;
            }

            Entity e;
            if(rand_int(0, 100) < 80) {
                e = { x, y, 'o', TCOD_desaturated_green };
            } else {
                e = { x, y, 'T', TCOD_darker_green };
            }
            _entities.push_back(e);
        }
    }
}

int main( int argc, char *argv[] ) {
    srand((unsigned int)time(NULL));

    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);
    TCOD_key_t key = {TCODK_NONE,0};
     
    auto root_console = TCODConsole::root;

    _entities.reserve(1000);
    _entities.push_back(entity_make(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, '@', TCODColor::white));
    
    Entity &player = _entities[0];

    // generate map and fov
    tcod_fov_map = new TCODMap(Map_Width, Map_Height);
    map_generate(Max_rooms, Room_min_size, Room_max_size, Map_Width, Map_Height, _entities[0]);
    tcod_fov_map->computeFov(player.x, player.y, fov_radius, fov_light_walls, fov_algorithm);

    // add entities to map
    map_add_entities(Max_monsters_per_room);

    while ( !TCODConsole::isWindowClosed() ) {
        TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS,&key,NULL);

        //// INPUT
        
        Movement m = { 0, 0 };
        switch(key.vk) {
            case TCODK_UP : m.y = -1; break;
            case TCODK_DOWN : m.y = 1; break;
            case TCODK_LEFT : m.x = -1; break;
            case TCODK_RIGHT : m.x = 1; break;
            case TCODK_ESCAPE : return 0;
            case TCODK_ENTER: {
                if(key.lalt) {
                    TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
                }
            }
            default:break;
        }
        
        //// UPDATE

        player = _entities[0];
        if((m.x != 0 || m.y != 0) && !map_blocked(player.x + m.x, player.y + m.y)) {
            player.x += m.x;
            player.y += m.y;
            tcod_fov_map->computeFov(player.x, player.y, fov_radius, fov_light_walls, fov_algorithm);
        }

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

        for(auto &entity : _entities) {
            if(!tcod_fov_map->isInFov(entity.x, entity.y)) {
                continue;
            }
            root_console->setDefaultForeground(entity.color);
            root_console->putChar(entity.x, entity.y, entity.gfx);
        }

        
        TCODConsole::flush();
    }

    return 0;
}