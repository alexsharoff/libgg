
#include "recorder.h"

#include "util.h"

#include <deque>
#include <string>
#include <utility>


// TODO: save to file / load
// TODO: record anywhere not just during training, but in menus etc
namespace recorder
{

namespace
{
std::deque<game_state> g_state_history;
std::deque<input_data> g_input_history;
bool g_recording = false;
bool g_playing = false;
int8_t g_speed = 1;
bool g_manual_frame_advance = false;
uint16_t g_prev_bitmask[] = {0, 0};
size_t g_history_idx = 0;
uint16_t g_speed_control_counter = 0;
struct saved_state
{
    std::optional<game_state> state;
    std::optional<input_data> input;

    void reset()
    {
        state.reset();
        input.reset();
    }
};
saved_state g_saved_state;
bool g_out_of_memory = false;

using memory_dump::dump;
void apply_patches(size_t image_base)
{
    // REC PLAYER => RECORD ALL
    dump("RECORD ALL", image_base + 0x3191EC);
    // REC ENEMY = FRAME STOP
    dump("FRAME STOP", image_base + 0x3191F8);
    // ENEMY JUMP = FRAME NEXT
    dump("FRAME NEXT", image_base + 0x319224);
    // ENEMY WALK = FRAME PREV
    dump("FRAME PREV", image_base + 0x319218);
}

// rec player = rec / stop recording
// rec enemy = frame stop
// play memory = play / stop playing
// enemy walk = prev frame (frame stop)
// enemy jump = next frame (frame stop)
// reset = reset current input
// TODO: this function is kind of a mess, split/simplify
bool input_hook(IGame* game)
{
    if (game->InMatch() && game->InTrainingMode())
    {
        auto input = game->GetInput();

        if (g_saved_state.state.has_value())
            game->SetState(*g_saved_state.state);
        if (g_saved_state.input.has_value())
            game->SetInput(*g_saved_state.input);

        bool speed_control_enabled = false;
        const auto& controller_configs = game->GetGameConfig().player_controller_config;
        for (size_t i = 0; i < 2; ++i)
        {
            uint16_t bitmask = reverse_bytes(input.keys[i]);
            const auto& cfg = controller_configs[i];

            const uint16_t training_mode_buttons = reverse_bytes(
                cfg.rec_player.bit | cfg.play_memory.bit | cfg.rec_enemy.bit |
                cfg.enemy_jump.bit | cfg.enemy_walk.bit);

            input.keys[i] = input.keys[i] & ~training_mode_buttons;

            const uint16_t pause_bit = reverse_bytes(cfg.pause.bit);
            if (g_recording || g_playing || g_manual_frame_advance)
                input.keys[i] = input.keys[i] & ~pause_bit;

            if (g_manual_frame_advance)
            {
                const uint16_t reset_bit = reverse_bytes(cfg.reset.bit);
                input.keys[i] = input.keys[i] & ~reset_bit;

                if (bitmask & cfg.reset.bit)
                {
                    input.keys[i] = 0;
                }
                else
                {
                    const uint16_t proposed_keys = game->GetInput().keys[i];
                    const uint16_t ignore_bits = training_mode_buttons | pause_bit | reset_bit;
                    const uint16_t prev_keys = reverse_bytes(g_prev_bitmask[i]) & ~ignore_bits;
                    const bool is_subset = (proposed_keys & input.keys[i]) == input.keys[i];
                    if (is_subset && (prev_keys != 0 || input.keys[i] == 0))
                        input.keys[i] = proposed_keys;
                }
            }

            if ((bitmask & cfg.rec_player.bit) && !(g_prev_bitmask[i] & cfg.rec_player.bit))
            {
                g_playing = false;
                g_recording = !g_recording;
                if (g_recording)
                {
                    g_state_history.clear();
                    g_input_history.clear();
                }
                g_history_idx = 0;
                g_out_of_memory = false;
            }

            if ((bitmask & cfg.play_memory.bit) && !(g_prev_bitmask[i] & cfg.play_memory.bit))
            {
                g_recording = false;
                g_playing = !g_playing;
                g_history_idx = 0;
            }

            if ((bitmask & cfg.rec_enemy.bit) && !(g_prev_bitmask[i] & cfg.rec_enemy.bit))
            {
                g_manual_frame_advance = !g_manual_frame_advance;
                g_speed = g_manual_frame_advance ? 0 : 1;
                g_out_of_memory = false;
            }

            if (bitmask & cfg.enemy_jump.bit && g_manual_frame_advance)
            {
                speed_control_enabled = true;
                if (!(g_prev_bitmask[i] & cfg.enemy_jump.bit) || g_speed_control_counter > 60)
                    g_speed = 1;
                else
                    g_speed = 0;
                ++g_speed_control_counter;
            }

            if (bitmask & cfg.enemy_walk.bit && g_manual_frame_advance)
            {
                speed_control_enabled = true;
                if (!(g_prev_bitmask[i] & cfg.enemy_walk.bit) || g_speed_control_counter > 60)
                    g_speed = -1;
                else
                    g_speed = 0;
                ++g_speed_control_counter;
            }

            g_prev_bitmask[i] = bitmask;
        }

        if (!speed_control_enabled)
        {
            if (g_manual_frame_advance)
                g_speed = 0;
            g_speed_control_counter = 0;
        }

        if (g_playing)
        {
            if (g_history_idx < g_input_history.size())
            {
                if (g_history_idx == 0)
                    game->SetState(g_state_history[g_history_idx]);

                game->SetInput(g_input_history[g_history_idx]);

                if (g_speed < 0)
                {
                    if (g_history_idx > 0)
                        --g_history_idx;
                }
                else if (g_speed > 0)
                {
                    ++g_history_idx;
                }
            }
            else
            {
                if (!g_manual_frame_advance)
                    g_playing = false;
            }

            if (g_history_idx >= g_state_history.size() && g_manual_frame_advance)
            {
                g_history_idx = g_state_history.size() - 1;
                g_speed = 0;
            }
        }
        else
        {
            if (g_recording)
            {
                const auto& input_ = game->GetInput();
                if (g_recording && (input.keys[0] != input_.keys[0] || input.keys[1] != input_.keys[1]))
                {
                    // recorded input at frame g_history_idx changed
                    // reset all recorded state after g_history_idx (but not input)
                    g_state_history.resize(g_history_idx);
                }
            }
            game->SetInput(input);
        }

        if (g_recording && g_history_idx < 5940)
        {
            try
            {
                if (g_history_idx >= g_state_history.size())
                    g_state_history.resize(g_history_idx + 1);
                if (g_history_idx >= g_input_history.size())
                    g_input_history.resize(g_history_idx + 1);
                g_state_history[g_history_idx] = game->GetState();
                g_input_history[g_history_idx] = game->GetInput();

                if (g_speed < 0)
                {
                    if (g_history_idx > 0)
                        --g_history_idx;
                }
                else if (g_speed > 0)
                {
                    ++g_history_idx;
                }
            }
            catch(const std::bad_alloc&)
            {
                g_out_of_memory = true;
                g_recording = false;
                g_history_idx = 0;
            }
        }
        else
        {
            g_recording = false;
        }

        g_saved_state.reset();
        if (g_manual_frame_advance)
        {
            if (g_recording || g_playing)
            {
                if (g_history_idx < g_state_history.size())
                    g_saved_state.state = g_state_history[g_history_idx];
                if (g_history_idx < g_input_history.size())
                    g_saved_state.input = g_input_history[g_history_idx];
            }
            else
            {
                if (g_speed <= 0)
                {
                    g_saved_state.state = game->GetState();
                    g_saved_state.input = game->GetInput();
                }
            }
        }
    }
    else
    {
        if (!game->GetState().match.pause_state.get())
        {
            g_state_history.clear();
            g_input_history.clear();
            g_recording = false;
            g_playing = false;
            g_speed = 1;
            g_manual_frame_advance = false;
            g_speed_control_counter = 0;
            g_out_of_memory = false;
            g_saved_state.reset();
        }
    }

    return true;
}

bool process_objects_hook(IGame* game)
{
    match_state ms;
    if (g_manual_frame_advance)
    {
        const auto& controller_state = game->GetState().match.controller_state.get();
        if (controller_state[0].bitmask_cur)
        {
            const auto input = controller_state[0].bitmask_cur;
            const auto obj = (active_object_state**)(game->GetImageBase() + 0x516778);
            game->DrawPressedDirection(input, 140, 300);
            game->DrawPressedButtons(input, *obj, 170, 300);
        }
        if (controller_state[1].bitmask_cur)
        {
            const auto input = controller_state[1].bitmask_cur;
            const auto obj = (active_object_state**)(game->GetImageBase() + 0x51A07C);
            game->DrawPressedDirection(input, 460, 300);
            game->DrawPressedButtons(input, *obj, 490, 300);
        }
    }
    game->WriteCockpitFont("DEV BUILD", 50, 100, 1, 0x50, 1);
    if (g_recording)
    {
        auto str = "REC " + std::to_string(g_history_idx);
        game->WriteCockpitFont(str.c_str(), 285, 100, 1, 0xFF, 1);
    }
    if (g_playing)
    {
        auto str = "PLAY " + std::to_string(g_history_idx);
        game->WriteCockpitFont(str.c_str(), 285, 100, 1, 0xFF, 1);
    }
    if (g_manual_frame_advance)
    {
        game->WriteCockpitFont("FRAME STOP", 260, 150, 1, 0xFF, 1);
    }
    if (g_out_of_memory)
    {
        game->WriteCockpitFont("OUT OF MEMORY!", 50, 150, 1, 0xff, 1);
    }

    return true;
}

}

void Initialize(IGame* game)
{
    game->RegisterCallback(IGame::Event::AfterGetInput, input_hook);
    game->RegisterCallback(IGame::Event::AfterProcessObjects, process_objects_hook);
    apply_patches(game->GetImageBase());
}

}