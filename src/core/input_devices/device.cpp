#include <iostream>
#include "device.hpp"
#include "../exceptions.hpp"

ButtonsDevice::ButtonsDevice() 
{
}

ButtonsDevice::~ButtonsDevice() 
{
}

void ButtonsDevice::set_buttons(UINT8 nr_button, UINT8 val) {
    if(nr_button >= nr_buttons) return;
    buttons_active[nr_button] = val;
}

UINT8 ButtonsDevice::check_status() {
    if(current_button >= nr_buttons) return 1;
    return buttons_active[current_button];
}

void ButtonsDevice::set_current(UINT8 current) {
    current_button = current;
}

UINT8 ButtonsDevice::get_current() {
    return current_button;
}

DevicesManager::DevicesManager() {
    // By default, one NESJoypad ?
    nr_devices = 1;
    automatic_poll_empty = true;
    devices.push_back(new NESJoypad);
    // with default keys ?
    resolve_key[SDLK_w] = {0, 0}; // means first device, first button
    resolve_key[SDLK_x] = {0, 1}; // first device, second button, etc
    resolve_key[SDLK_SPACE] = {0, 2};
    resolve_key[SDLK_RETURN] = {0, 3};
    resolve_key[SDLK_UP] = {0, 4};
    resolve_key[SDLK_DOWN] = {0, 5};
    resolve_key[SDLK_LEFT] = {0, 6};
    resolve_key[SDLK_RIGHT] = {0, 7};
    strobe = 0;

    std::printf("DevicesManager created at %p\n", (void*)this);
}

DevicesManager::~DevicesManager() {
    for(auto it = devices.begin(); it != devices.end(); it++) {
        delete *it;
    }
    std::printf("DevicesManager deleted at %p\n", (void*)this);
    std::fflush(NULL);
}

void DevicesManager::write_4016(UINT8 val) {
    strobe = val & 0x01;
    //std::printf("%p\n", this);

    for(auto it = devices.begin();
        it != devices.end(); 
        it++) 
        (*it)->set_current(0);
    
    if(strobe && automatic_poll_empty) {
        update_pending_keys();
    }
}

UINT8 DevicesManager::read_4016() {
    UINT8 res = 0;
    if(nr_devices > 0) {
        res = devices[0]->check_status();
        if(!strobe) devices[0]->set_current(devices[0]->get_current()+1); // incr current button
    }
    return res;
}

UINT8 DevicesManager::read_4017() {
    UINT8 res = 0;
    if(nr_devices > 1) {
        res = devices[1]->check_status();
        if(!strobe) devices[1]->set_current(devices[1]->get_current()+1); // incr current button
    }
    return res;
}

bool DevicesManager::pop_event(SDL_Event *ev) {
    if(pending_keys.empty()) return false;
    *ev = pending_keys.front();
    pending_keys.pop();
    return true;
}

void DevicesManager::push_pending_key(SDL_Event ev) {
    pending_keys.push(ev);
}

void DevicesManager::update_pending_keys() {
    std::pair<UINT8, UINT8> p;
    SDL_Keycode k;
    SDL_Event ev;
    if(!strobe) return;

    while(pop_event(&ev)) {

        switch(ev.type) {
        case SDL_KEYUP:
            k = ev.key.keysym.sym;
            if(!resolve_key.count(k)) break;
            p = resolve_key[k];
            devices[p.first]->set_buttons(p.second, 0);
            break;
        
        case SDL_KEYDOWN:
            k = ev.key.keysym.sym;
            if(!resolve_key.count(k)) break;
            p = resolve_key[k];
            devices[p.first]->set_buttons(p.second, 1);
            break;

        default:
            break;
        }

    }
}

SDL_Events_Manager::SDL_Events_Manager(std::shared_ptr<DevicesManager> devices_manager) : devices_manager(devices_manager)
{
    sp_actions_keys[SDLK_ESCAPE] = EMU_SP_ACTIONS::QUIT;
}

SDL_Events_Manager::~SDL_Events_Manager() {}

void SDL_Events_Manager::push_event(SDL_Event ev) {
    pending_events.push(ev);
}

bool SDL_Events_Manager::pop_event(SDL_Event *ev) {
    if(pending_events.empty()) return false;
    *ev = pending_events.front();
    pending_events.pop();
    return true;
}

void SDL_Events_Manager::sdl_handle_poll() {
    SDL_Event ev;
    SDL_Keycode k;
    while(pop_event(&ev)) {
        switch(ev.type) {
        case SDL_QUIT:
            throw ExitedGame();
            break;

        case SDL_KEYDOWN:
            k = ev.key.keysym.sym;
            if(sp_actions_keys.count(k)) {

            } else {
                devices_manager->push_pending_key(ev);
            }
            break;

        case SDL_KEYUP:
            k = ev.key.keysym.sym;
            if(sp_actions_keys.count(k)) {

                switch(sp_actions_keys[k]) {
                    case EMU_SP_ACTIONS::QUIT:
                        throw ExitedGame();
                        break;
                }

            } else {
                devices_manager->push_pending_key(ev);
            }
            break;

        default:
            break;
        }
    }
}