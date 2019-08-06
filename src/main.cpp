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

struct Movement {
    int x, y;
};

struct Entity {
    int x,y;
    int gfx;
    TCODColor color;
};

Entity entity_make(int x, int y, int gfx, TCODColor color) {
    Entity e;
    e.x = x;
    e.y = y;
    e.gfx = gfx;
    e.color = color;
    return e;
}

std::vector<Entity> _entities;

struct Tile {
    bool blocked = false;
};

const int Map_Width = 80;
const int Map_Height = 50;
Tile _map[Map_Width * Map_Height];

int map_index(int x, int y) {
    return x + Map_Width * y;
}

int main( int argc, char *argv[] ) {
    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);
    TCOD_key_t key = {TCODK_NONE,0};
     
    auto root_console = TCODConsole::root;

    _entities.push_back(entity_make(SCREEN_WIDTH/2,SCREEN_HEIGHT/2, '@', TCODColor::white));
    _entities.push_back(entity_make(SCREEN_WIDTH/2 - 5,SCREEN_HEIGHT/2, '@', TCODColor::yellow));

    _map[map_index(1, 1)].blocked = true;
    _map[map_index(1, 2)].blocked = true;

    _map[map_index(_entities[0].x + 2, _entities[0].y)].blocked = true;

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
        if(!_map[map_index(player.x + m.x, player.y + m.y)].blocked) {
            player.x += m.x;
            player.y += m.y;
        }

        //// RENDER

        root_console->setDefaultForeground(TCODColor::white);
        root_console->clear();

        auto dark_wall = TCODColor(0, 0, 100);
        auto dark_floor = TCODColor(50, 50, 150);
        for(int y = 0; y < Map_Height; y++) {
            for(int x = 0; x < Map_Width; x++) {
                if(_map[map_index(x, y)].blocked) {
                    root_console->setCharBackground(x, y, dark_wall);
                } else {
                    root_console->setCharBackground(x, y, dark_floor);
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