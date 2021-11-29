#include "nesjoypad.hpp"

NESJoypad::NESJoypad()
{
    nr_buttons = 8;
    buttons_descriptions = new std::string[8];
    buttons_descriptions[0] = "A";
    buttons_descriptions[1] = "B";
    buttons_descriptions[2] = "Select";
    buttons_descriptions[3] = "Start";
    buttons_descriptions[4] = "Up";
    buttons_descriptions[5] = "Down";
    buttons_descriptions[6] = "Left";
    buttons_descriptions[7] = "Right";
    
    buttons_active = new UINT8[8];
    for(int i=0; i<8; i++) buttons_active[i] = 0;
}

NESJoypad::~NESJoypad() {
    delete[] buttons_descriptions;
    delete[] buttons_active;
}