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

int playerx=SCREEN_WIDTH/2,playery=SCREEN_HEIGHT/2;

int main( int argc, char *argv[] ) {
    TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD);
    TCODConsole::initRoot(SCREEN_WIDTH, SCREEN_HEIGHT, "libtcod C++ tutorial", false);
    TCOD_key_t key = {TCODK_NONE,0};
     
    auto root_console = TCODConsole::root;

    while ( !TCODConsole::isWindowClosed() ) {
        TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS,&key,NULL);
        switch(key.vk) {
            case TCODK_UP : playery--; break;
            case TCODK_DOWN : playery++; break;
            case TCODK_LEFT : playerx--; break;
            case TCODK_RIGHT : playerx++; break;
            case TCODK_ESCAPE : return 0;
            case TCODK_ENTER: {
                if(key.lalt) {
                    TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
                }
            }
            default:break;
        }
        
        root_console->setDefaultForeground(TCODColor::white);
        root_console->clear();
        root_console->putChar(playerx, playery, '@');
        
        TCODConsole::flush();
    }

    return 0;
}