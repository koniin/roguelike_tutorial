#include "libtcod.hpp"
#include <iostream>

// 
// http://rogueliketutorials.com/tutorials/tcod/part-1/
// 
// http://www.roguebasin.com/index.php?title=Complete_roguelike_tutorial_using_C%2B%2B_and_libtcod_-_part_2:_map_and_actors
// 
// These differ somewhat

const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 50;

struct Colors {
    TCODColor dark_wall = TCODColor(0, 0, 100);
    TCODColor dark_ground = TCODColor(50, 50, 150);
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
};

struct Rect {
    int x, y, w, h, x2, y2;
};
Rect rect_make(int x, int y, int w, int h) {
    return { x, y, w, h, x + w, y + h };
}

std::vector<Entity> _entities;

const int Map_Width = 80;
const int Map_Height = 50;
Tile _map[Map_Width * Map_Height];

int map_index(int x, int y) {
    return x + Map_Width * y;
}

bool map_blocked(int x, int y) {
    if(_map[map_index(x, y)].blocked) {
        return true;
    }
    return false;
}

void map_block(int x, int y) {
    _map[map_index(x, y)].blocked = true;
    _map[map_index(x, y)].block_sight = true;
}

void map_make_room(Rect room) {
    for(int x = room.x + 1; x < room.x2; x++) {
        for(int y = room.y + 1; y < room.y2; y++) {
            _map[map_index(x, y)].blocked = false;
            _map[map_index(x, y)].block_sight = false;           
        }    
    }
//     for x in range(room.x1 + 1, room.x2):
// +           for y in range(room.y1 + 1, room.y2):
// +               self.tiles[x][y].blocked = False
// +               self.tiles[x][y].block_sight = False

}

void map_make_h_tunnel(int x1, int x2, int y) {
    for(int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
        _map[map_index(x, y)].blocked = false;
        _map[map_index(x, y)].block_sight = false;
    }
//     def create_h_tunnel(self, x1, x2, y):
// +       for x in range(min(x1, x2), max(x1, x2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}
void map_make_v_tunnel(int y1, int y2, int x) {
    for(int y = std::min(y1, y2); x <= std::max(y1, y2); y++) {
        _map[map_index(x, y)].blocked = false;
        _map[map_index(x, y)].block_sight = false;
    }
// +   def create_v_tunnel(self, y1, y2, x):
// +       for y in range(min(y1, y2), max(y1, y2) + 1):
// +           self.tiles[x][y].blocked = False
// +           self.tiles[x][y].block_sight = False
}

int main( int argc, char *argv[] ) {
    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);
    TCOD_key_t key = {TCODK_NONE,0};
     
    auto root_console = TCODConsole::root;

    _entities.push_back(entity_make(SCREEN_WIDTH/2,SCREEN_HEIGHT/2, '@', TCODColor::white));
    _entities.push_back(entity_make(SCREEN_WIDTH/2 - 5,SCREEN_HEIGHT/2, '@', TCODColor::yellow));

    map_make_room(rect_make(20, 15, 10, 15));
    map_make_room(rect_make(35, 15, 10, 15));
    map_make_h_tunnel(25, 40, 23);

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

        Entity &player = _entities[0];
        if(!map_blocked(player.x + m.x, player.y + m.y)) {
            player.x += m.x;
            player.y += m.y;
        }

        //// RENDER

        root_console->setDefaultForeground(TCODColor::white);
        root_console->clear();

        for(int y = 0; y < Map_Height; y++) {
            for(int x = 0; x < Map_Width; x++) {
                if(_map[map_index(x, y)].block_sight) {
                    root_console->setCharBackground(x, y, color_table.dark_wall);
                } else {
                    root_console->setCharBackground(x, y, color_table.dark_ground);
                }
            }
        }

        for(auto &entity : _entities) {
            root_console->setDefaultForeground(entity.color);
            root_console->putChar(entity.x, entity.y, entity.gfx);
        }

        
        TCODConsole::flush();
    }

    return 0;
}