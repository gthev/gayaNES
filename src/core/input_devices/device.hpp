#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "../types.hpp"
#include <vector>
#include <utility>
#include <queue>
#include <string>
#include <memory>
#include <map>
#include <SDL2/SDL.h>

typedef std::pair<UINT8,UINT8> button_id;

class ButtonsDevice
{
protected:
    UINT8                               nr_buttons;
    std::string                         *buttons_descriptions;
    UINT8                               *buttons_active;    
    UINT8                               current_button;
public:
    ButtonsDevice();
    virtual ~ButtonsDevice();
    virtual void        set_buttons(UINT8 nr_button, UINT8 val);
    virtual void        set_current(UINT8 current);
    virtual UINT8       check_status();
    virtual UINT8       get_current();
};

#include "nesjoypad.hpp"

class SDL_Events_Manager;

class DevicesManager
{
private:
    UINT8                               nr_devices;
    std::vector<ButtonsDevice*>         devices;
    UINT8                               strobe;
    std::map<SDL_Keycode, button_id>    resolve_key;
    /* This boolean indicates whether we should empty the SDL events poll
    each time the strobe is set */
    bool                                automatic_poll_empty;

    std::queue<SDL_Event>               pending_keys;

    bool                                pop_event(SDL_Event *ev);

public:
    DevicesManager();
    ~DevicesManager();

    std::shared_ptr<SDL_Events_Manager> keyboards_manager;
 
    void                               set_key_association(SDL_KeyCode k, button_id val){resolve_key[k] = val;};
    void                               set_automatic_poll_empty(bool val) {automatic_poll_empty = val;};
    void                               write_4016(UINT8 val);
    UINT8                              read_4016();
    //also them I suppose
    void                               write_4017(UINT8 val){}; // should probably be in APU but not absolutely sure
    UINT8                              read_4017();

    void                               update_pending_keys();
    void                               push_pending_key(SDL_Event ev);
};

enum class EMU_SP_ACTIONS {
    QUIT, NR_ACTIONS
};

class SDL_Events_Manager
{
public:

    SDL_Events_Manager(std::shared_ptr<DevicesManager> devices_manager);
    ~SDL_Events_Manager();

    void                                        sdl_handle_poll();
    void                                        push_event(SDL_Event ev);

private:
    std::map<SDL_Keycode, EMU_SP_ACTIONS>       sp_actions_keys;
    std::queue<SDL_Event>                       pending_events;
    std::shared_ptr<DevicesManager>             devices_manager;
    bool                                        pop_event(SDL_Event *ev);

};

#endif