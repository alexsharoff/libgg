#include "ggpo.h"

#include "memory_dump.h"
#include "util.h"

#include <ggponet.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <Windows.h>


#define GGPO_CHECK(expr) \
    do { \
        const auto res___ = expr; \
        if (!GGPO_SUCCEEDED(res___)) { \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " << #expr << " == " << res___; \
            throw std::logic_error("ggpo error"); \
        } \
    } while(false)

#pragma once


namespace ggpo
{

namespace
{

GGPOSession* g_session = nullptr;
GGPOPlayerHandle g_player_handles[2];
bool g_call_ggpo_idle_manually = true;
bool g_during_match = false;
bool g_replaying_input = false;
bool g_is_active = false;
size_t g_frame_base = 0;
bool g_waiting_for_first_match = false;
std::unordered_map<size_t, std::shared_ptr<game_state>> g_saved_state_map;
size_t g_vs_2p_jmp_addr = 0;
IGame* g_game = nullptr;

void activate()
{
    g_is_active = true;
    g_waiting_for_first_match = true;
    g_saved_state_map.clear();
}

#pragma warning(push)
// warning: flow in or out of inline asm code suppresses global optimization
#pragma warning(disable: 4740)
void __declspec(naked) jmp_menu_network()
{
    __asm {
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
    }
    activate();
    __asm {
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        jmp g_vs_2p_jmp_addr
    }
}
#pragma warning(pop)

using memory_dump::dump;
using memory_dump::load;

void apply_patches(size_t image_base)
{
    load(image_base + 0x226448 + 2 * 4, g_vs_2p_jmp_addr);
    // Replace main menu jump table entry for NETWORK
    void* ptr = jmp_menu_network;
    dump(ptr, image_base + 0x226448 + 3 * 4);
}

bool begin_game(const char*)
{
    LIBGG_LOG() << std::endl;
    return true;
}

bool save_game_state(unsigned char **buffer, int *len, int *checksum, int frame)
{
    LIBGG_LOG() << std::endl;

    auto state_ptr = std::make_shared<game_state>(g_game->GetState());
    assert(state_ptr->frame - g_frame_base == static_cast<size_t>(frame));

    *buffer = (unsigned char*)state_ptr.get();
    *len = sizeof(game_state);
    *checksum = state_checksum(*state_ptr);

    g_saved_state_map[frame] = state_ptr;

    return true;
}

bool load_game_state(unsigned char *buffer, int /* len */)
{
    LIBGG_LOG() << std::endl;
    auto state_ptr = (game_state*)(buffer);
    g_game->SetState(*state_ptr);
    return true;
}

bool log_game_state(char* /* filename */, unsigned char* /* buffer */, int /* len */)
{
    return true;
}

void free_buffer(void *buffer)
{
    if (!buffer)
        return;

    LIBGG_LOG() << std::endl;

    auto state = (game_state*)buffer;
    g_saved_state_map.erase(state->frame);
}

bool advance_frame(int)
{
    LIBGG_LOG() << std::endl;
    g_game->EnableDrawing(false);
    g_replaying_input = true;
    g_game->GameTick();
    g_game->EnableDrawing(true);
    g_replaying_input = false;
    return true;
}

bool on_event(GGPOEvent *info)
{
    LIBGG_LOG() << info->code << std::endl;
    return true;
}

void on_match_start()
{
    LIBGG_LOG() << std::endl;

    // ggpo_idle() must be called instead of Sleep().
    // In Steam release Sleep() is called just before IDirect3D::Present,
    // but sometimes it's skipped, which is a problem.
    // To be consistent with recommended implementation (VectorWar),
    // ggpo_idle() should be called at the beginning of game tick,
    // or at least before doing any processing for current frame.
    // TODO: research, will this change game timing?
    // 
    // This disables limit_fps() function when it's called by the game.
    // We will call it manually after getting current input data.
    g_game->EnableFpsLimit(false);

    g_game->EnablePauseMenu(false);

    g_during_match = true;
    g_call_ggpo_idle_manually = true;
    g_frame_base = g_game->GetState().frame;
}

void on_match_end()
{
    LIBGG_LOG() << std::endl;

    g_game->EnableFpsLimit(true);
    g_game->EnablePauseMenu(true);
    g_during_match = false;
}

void start_session()
{
    if (g_session)
        return;

    GGPOSessionCallbacks callbacks;
    callbacks.advance_frame = advance_frame;
    callbacks.begin_game = begin_game;
    callbacks.free_buffer = free_buffer;
    callbacks.load_game_state = load_game_state;
    callbacks.log_game_state = log_game_state;
    callbacks.on_event = on_event;
    callbacks.save_game_state = save_game_state;
    GGPO_CHECK(ggpo_start_synctest(&g_session, &callbacks, "GGXX", 2, 2, 8));

    GGPOPlayer player;
    player.player_num = 1;
    player.type = GGPO_PLAYERTYPE_LOCAL;
    player.size = sizeof(GGPOPlayer);
    GGPO_CHECK(ggpo_add_player(g_session, &player, &g_player_handles[0]));
    player.player_num++;
    GGPO_CHECK(ggpo_add_player(g_session, &player, &g_player_handles[1]));

    // ggpo_start_synctest doesn't suppot delay
    //GGPO_CHECK(ggpo_set_frame_delay(g_session, g_player_handles[0], 6));
    //GGPO_CHECK(ggpo_set_frame_delay(g_session, g_player_handles[1], 6));

    //ggpo_set_disconnect_notify_start(g_session, 1000);
    //ggpo_set_disconnect_timeout(g_session, 10000);

    g_during_match = false;
    g_waiting_for_first_match = true;

    LIBGG_LOG() << "end" << std::endl;
}

void close_session()
{
    if (g_session)
    {
        GGPO_CHECK(ggpo_close_session(g_session));
        g_session = nullptr;
    }

    LIBGG_LOG() << "end" << std::endl;
}

bool input_data_hook(IGame* game)
{
    if (!g_session || !g_during_match)
        return true;

    auto input = game->GetInput();

    LIBGG_LOG() << std::endl;

    if (!g_replaying_input)
    {
        game->EnableFpsLimit(true);
        game->LimitFps();
        game->EnableFpsLimit(false);
    }

    if (g_call_ggpo_idle_manually)
        GGPO_CHECK(ggpo_idle(g_session, 1));

    g_call_ggpo_idle_manually = true;

    const auto frame = g_game->GetState().frame - g_frame_base;
    const auto found = g_saved_state_map.find(frame);
    if (found == g_saved_state_map.end())
    {
        g_saved_state_map[frame] = std::make_shared<game_state>(game->GetState());
    }
    const auto& controller_configs = game->GetGameConfig().player_controller_config;
    input.keys[0] = game->RemapButtons(
        input.keys[0], controller_configs[0], game_config::default_controller_config
    );
    input.keys[1] = game->RemapButtons(
        input.keys[1], controller_configs[1], game_config::default_controller_config
    );

    GGPO_CHECK(ggpo_add_local_input(g_session, g_player_handles[0], (void*)&input.keys[0], 2));
    GGPO_CHECK(ggpo_add_local_input(g_session, g_player_handles[1], (void*)&input.keys[1], 2));
    int disconnected = 0;
    input = input_data();
    GGPO_CHECK(ggpo_synchronize_input(g_session, (void*)input.keys, 4, &disconnected));

    input.keys[0] = game->RemapButtons(
        input.keys[0], game_config::default_controller_config, controller_configs[0]
    );
    input.keys[1] = game->RemapButtons(
        input.keys[1], game_config::default_controller_config, controller_configs[1]
    );

    game->SetInput(input);

    return true;
}

bool game_tick_end_hook(IGame* game)
{
    if (!g_is_active)
        return true;

    if (game->InMatch())
    {
        g_waiting_for_first_match = false;
        if (!g_during_match)
        {
            start_session();
            LIBGG_LOG() << "on_match_start" <<  std::endl;
            on_match_start();
        }
        else
        {
            LIBGG_LOG() << "advance_frame" <<  std::endl;
            GGPO_CHECK(ggpo_advance_frame(g_session));
        }
    }
    else
    {
        if (!g_replaying_input)
        {
            if(g_during_match)
            {
                LIBGG_LOG() << "on_match_end" <<  std::endl;
                on_match_end();
                close_session();
            }

            // Exited from VS 2P
            if (!g_waiting_for_first_match && game->FindFiberByName("OPTION"))
            {
                LIBGG_LOG() << "close_session" <<  std::endl;
                on_match_end();
                close_session();
                g_is_active = false;
            }
        }
    }

    return true;
}

bool sleep_hook(IGame* game)
{
    if (!g_session || !g_during_match)
        return true;

    LIBGG_LOG() << std::endl;

    assert(!g_replaying_input);

    const auto ms = game->GetSleepTime();
    using std::chrono::steady_clock;
    const auto idle_begin = steady_clock::now();
    GGPO_CHECK(ggpo_idle(g_session, ms));
    const auto idle_end = steady_clock::now();

    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    const auto idle_ms = duration_cast<milliseconds>(idle_end - idle_begin).count();
    if (ms > idle_ms)
        ::Sleep(static_cast<DWORD>(ms - idle_ms));

    g_call_ggpo_idle_manually = false;

    return true;
}

}

void Initialize(IGame* game)
{
    game->RegisterCallback(IGame::Event::AfterGetInput, input_data_hook);
    game->RegisterCallback(IGame::Event::BeforeSleep, sleep_hook);
    game->RegisterCallback(IGame::Event::AfterGameTick, game_tick_end_hook);
    apply_patches(game->GetImageBase());
    g_game = game;
}

}