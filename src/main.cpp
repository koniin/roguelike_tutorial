#include "libtcod.hpp"

#include <iostream>

int playerx=40,playery=25;

int main( int argc, char *argv[] ) {
   // TCODConsole::setCustomFont("data/arial10x10.png", TCOD_FONT_TYPE_GREYSCALE, TCOD_FONT_LAYOUT_TCOD);
   TCODConsole::initRoot(80, 50, "libtcod C++ tutorial", false);
   TCOD_key_t key = {TCODK_NONE,0};

   while ( !TCODConsole::isWindowClosed() ) {
       TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS,&key,NULL);
       switch(key.vk) {
           case TCODK_UP : playery--; break;
           case TCODK_DOWN : playery++; break;
           case TCODK_LEFT : playerx--; break;
           case TCODK_RIGHT : playerx++; break;
           case TCODK_ESCAPE : return 0;
           default:break;
       }
       TCODConsole::root->clear();
       TCODConsole::root->putChar(playerx, playery, '@');
       
       TCODConsole::flush();
   }

   return 0;
}