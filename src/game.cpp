#include "game.h"

#include "memory_dump.h"
#include "util.h"
#include "xact_audio.h"

#include <unordered_map>
#include <vector>


using memory_dump::load;
using memory_dump::dump;
using memory_dump::local_memory_accessor;

// hooks for game functions
namespace
{
IGame* g_game = nullptr;
gg_state g_global_data_orig;
bool g_enable_fps_limit = true;
size_t g_image_base = 0;
std::unordered_map<IGame::Event, std::vector<IGame::CallbackFuncType>> g_callbacks;
game_state g_state{};
input_data g_input{};
std::pair<IXACT3WaveBank*, int16_t> g_sound = { nullptr, static_cast<int16_t>(0) };
uint32_t g_sleep_time = 0;
bool g_is_ready = false;
bool g_is_loading = false;

void get_raw_input_data(input_data* out)
{
    const auto f = *g_global_data_orig.get_raw_input_data.get().get();

    input_data input;
    f(&input);
    g_input = input;

    for (const auto& func : g_callbacks[IGame::Event::AfterGetInput])
    {
        if (!func(g_game))
            return;
    }

    *out = g_input;
}

void process_input()
{
    const auto f = *g_global_data_orig.process_input.get().get();
    f();

    // TODO: update state?
}

void process_objects()
{
    const auto f = *g_global_data_orig.process_objects.get().get();
    f();

    save_current_state(g_image_base, g_state);

    for (const auto& func : g_callbacks[IGame::Event::AfterProcessObjects])
    {
        if (!func(g_game))
            return;
    }
}

int32_t limit_fps()
{
    const auto f = *g_global_data_orig.limit_fps.get().get();
    return g_enable_fps_limit ? f() : 0;
}

void __stdcall sleep(uint32_t ms)
{
    g_sleep_time = ms;
    for (const auto& func : g_callbacks[IGame::Event::BeforeSleep])
    {
        if (!func(g_game))
            return;
    }

    const auto f = **g_global_data_orig.sleep_ptr.get();
    f(ms);
}
auto g_sleep_ptr = &sleep;

XACT3Wave w;
int32_t __stdcall play_sound(IXACT3WaveBank* a1, int16_t a2, uint32_t a3, int32_t a4, int8_t a5, IXACT3Wave** a6)
{
    g_sound = { a1, a2 };

    for (const auto& func : g_callbacks[IGame::Event::BeforePlaySound])
    {
        if (!func(g_game))
        {
            *a6 = &w;
            return 0;
        }
    }

    const auto f = *g_global_data_orig.play_sound.get().ptr;
    return f(a1, a2, a3, a4, a5, a6);
}

void game_tick()
{
    if (!g_is_ready)
    {
        decltype(match_state_2::menu_fibers) fibers;
        load(g_image_base, fibers);
        const auto& fiber_data = fibers.get()[0];
        const auto is_loading = fiber_data.name == std::string("ATLD");
        if (g_is_loading && !is_loading)
        {
            // Loading has ended
            g_is_loading = false;

            init_fiber_mgmt();

            gg_state global_data;
            load_global_data(g_image_base, global_data);
            g_global_data_orig.play_sound = global_data.play_sound;

            global_data.get_raw_input_data.get().set(get_raw_input_data);
            global_data.limit_fps.get().set(limit_fps);
            global_data.process_input.get().set(process_input);
            global_data.process_objects.get().set(process_objects);
            global_data.sleep_ptr = &g_sleep_ptr;
            global_data.play_sound.get().ptr = play_sound;
            dump_global_data(g_image_base, global_data);

            g_is_ready = true;

            save_current_state(g_image_base, g_state);
        }
        else
        {
            g_is_loading = is_loading;
            const auto f = *g_global_data_orig.game_tick.get().get();
            f();
            return;
        }
    }

    g_input = input_data();

    for (const auto& func : g_callbacks[IGame::Event::BeforeGameTick])
    {
        if (!func(g_game))
            return;
    }

    const auto f = *g_global_data_orig.game_tick.get().get();
    f();

    ++g_state.frame;

    for (const auto& func : g_callbacks[IGame::Event::AfterGameTick])
    {
        if (!func(g_game))
            return;
    }
}

uint32_t button_bitmask_to_icon_bitmask(uint32_t input, const active_object_state* obj)
{
    auto f = g_global_data_orig.button_bitmask_to_icon_bitmask;
    __asm
    {
        mov ecx, input
        mov edx, obj
        push 1
        call f
        add esp, 4
    }
}

uint32_t direction_bitmask_to_icon_id(uint32_t input)
{
    uint32_t input_ = 0;
    // additional preprocessing is required
    if (input & 0x10)
        input_ |= 4;
    if (input & 0x20)
        input_ |= 2;
    if (input & 0x40)
        input_ |= 8;
    if (input & 0x80)
        input_ |= 1;

    auto f = g_global_data_orig.direction_bitmask_to_icon_id;
    __asm
    {
        mov ecx, input_
        push 0
        push input_
        call f
        add esp, 4*2
    }
}

}

class Game : public IGame
{
public:
    Game(size_t image_base) : m_isReady(false)
    {
        g_image_base = image_base;

        gg_state global_data;
        load_global_data(image_base, global_data);
        g_global_data_orig = global_data;

        // wait until the game is loaded (ie Loading fiber has exited)
        global_data.game_tick.get().set(game_tick);
        dump_global_data(image_base, global_data);

        g_game = this;
    }

    void EnableDrawing(bool enable) final
    {
        const auto addr = (size_t)g_image_base + 0x146FFD;
        if (enable)
        {
            // 0x75 = jne
            local_memory_accessor::write(static_cast<uint8_t>(0x75), addr);
        }
        else
        {
            // 0xEB = jmp
            local_memory_accessor::write(static_cast<uint8_t>(0xEB), addr);
        }
    }

    void EnablePauseMenu(bool enable) final
    {
        const auto addr = (size_t)g_image_base + 0xEBC19;
        if (enable)
        {
            // 0F84 = je
            const uint8_t data[] = {0x0f, 0x84};
            local_memory_accessor::write(data, addr);
        }
        else
        {
            // 90E9 = nop jmp
            const uint8_t data[] = {0x90, 0xe9};
            local_memory_accessor::write(data, addr);
        }
    }

    void EnableFpsLimit(bool enable) final
    {
        g_enable_fps_limit = enable;
    }

    const game_state& GetState() const final
    {
        return g_state;
    }

    const input_data& GetInput() const final
    {
        return g_input;
    }

    void SetState(const game_state& state) final
    {
        g_state = state;
        revert_state(g_image_base, g_state);
    }

    void SetInput(const input_data& input) final
    {
        g_input = input;
    }

    void DisplayPlayerStatusTicker(const char* message, uint32_t side) final
    {
        auto f = g_global_data_orig.player_status_ticker;
        // Custom calling convention due to LTCG:
        // * Message in ESI
        // * Side (0, 1) in EAX
        __asm
        {
            mov esi, message
            mov eax, side
            call f
        }
    }

    void DrawRect(uint32_t color, uint32_t x1, uint32_t y1,
                          uint32_t x2, uint32_t y2) final
    {
        auto f = g_global_data_orig.draw_rect;
        __asm
        {
            push 4
            push y2
            push x2
            push y1
            push x1
            mov eax, color
            call f
            add esp, 4*5
        }
    }

    void WriteUtf8Font(const char* text, int x, int y,
                               float z, float opacity,
                               float scale_x = 1, float scale_y = 1) final
    {
        float scale_x_orig, scale_y_orig;
        load(g_image_base + 0x3EE774, scale_x_orig);
        load(g_image_base + 0x3EE83C, scale_y_orig);
        dump(scale_x, g_image_base + 0x3EE774);
        dump(scale_y, g_image_base + 0x3EE83C);
        auto f = g_global_data_orig.write_utf8_font;
        __asm
        {
            push 0
            push 0xFFFFFFFF
            push opacity
            push z
            push y
            push x
            mov eax, text
            call f
            add esp, 4*6
        }
        dump(scale_x_orig, g_image_base + 0x3EE774);
        dump(scale_y_orig, g_image_base + 0x3EE83C);
    }

    void WriteCockpitFont(const char* buffer, int x, int y, float z,
                                  uint8_t alpha, float scale) final
    {
        g_global_data_orig.write_cockpit_font(buffer, x, y, z, alpha, scale);
    }

    void WriteSpecialFont(const char* text, float x, float y, float z,
                                  uint32_t flags, uint32_t font, float scale) final
    {
        g_global_data_orig.write_special_font(text, x, y, z, flags, font, scale);
    }

    void DrawPressedButtons(uint32_t input_bitmask, const active_object_state* player, uint32_t x, uint32_t y) final
    {
        const auto buttons = button_bitmask_to_icon_bitmask(input_bitmask, player);
        g_global_data_orig.draw_pressed_buttons(buttons, x, y, 3, 1);
    }

    void DrawPressedDirection(uint32_t input_bitmask, uint32_t x, uint32_t y) final
    {
        const auto directions = direction_bitmask_to_icon_id(input_bitmask);
        g_global_data_orig.draw_pressed_buttons(directions, x, y, 3, 1);
    }

    uint16_t RemapButtons(uint16_t input, const game_config::controller_config& from,
                          const game_config::controller_config& to) const final
    {
        input = reverse_bytes(input);

        const auto directions = from.up.bit | from.down.bit |
                                from.left.bit | from.right.bit;
        uint16_t result = input & directions;

        if (input & from.pk.bit)
            input |= from.p.bit | from.k.bit;
        if (input & from.pd.bit)
            input |= from.p.bit | from.d.bit;
        if (input & from.pks.bit)
            input |= from.p.bit | from.k.bit | from.s.bit;
        if (input & from.pksh.bit)
            input |= from.p.bit | from.k.bit | from.s.bit | from.hs.bit;

        if (input & from.p.bit)
            result |= to.p.bit;
        if (input & from.k.bit)
            result |= to.k.bit;
        if (input & from.s.bit)
            result |= to.s.bit;
        if (input & from.hs.bit)
            result |= to.hs.bit;
        if (input & from.d.bit)
            result |= to.d.bit;
        if (input & from.taunt.bit)
            result |= to.taunt.bit;
        if (input & from.reset.bit)
            result |= to.reset.bit;
        if (input & from.pause.bit)
            result |= to.pause.bit;

        // TODO: remaining buttons

        return reverse_bytes(result);
    }

    const game_config& GetGameConfig() const final
    {
        return g_state.match2.config.get();
    }

    bool InMatch() const final
    {
        const auto& fibers = g_state.match2.menu_fibers.get();
        for (const auto& f : fibers)
        {
            if (f.name == std::string("FIN ")) // TODO: find something less hacky
                continue;
            if (f.status)
                return false;
        }
        return g_state.match2.next_fiber_id == fiber_id::match;
    }

    bool InTrainingMode() const final
    {
        const auto game_mode = g_state.match2.game_mode.get();
        return game_mode == 0x101 || game_mode == 0x102;
    }

    uint32_t GetActivePlayers() const final
    {
        const auto game_mode = g_state.match2.game_mode.get();
        return game_mode & 0x3;
    }

    bool FindFiberByName(const std::string& name) const final
    {
        const auto& fibers = g_state.match2.menu_fibers.get();
        for (const auto& f : fibers)
        {
            if (f.name == name)
                return true;
        }
        return false;
    }

    const std::pair<IXACT3WaveBank*, int16_t>& GetCurrentSound() const final
    {
        return g_sound;
    }

    const uint32_t GetSleepTime() const final
    {
        return g_sleep_time;
    }

    void GameTick() const final
    {
        game_tick();
    }

    void GetInput(input_data* out) const final
    {
        get_raw_input_data(out);
    }

    void ProcessInput() const final
    {
        process_input();
    }

    void ProcessObjects() const final
    {
        process_objects();
    }

    int32_t LimitFps() const final
    {
        return limit_fps();
    }

    size_t GetImageBase() const final
    {
        return g_image_base;
    }

    void RegisterCallback(Event event, std::function<bool(IGame*)> f, CallbackPosition pos) final
    {
        if (pos == CallbackPosition::Last)
            g_callbacks[event].push_back(f);
        else
            g_callbacks[event].insert(g_callbacks[event].begin(), f);
    }

private:
    bool m_isReady;
};

std::shared_ptr<IGame> IGame::Initialize(size_t baseAddress)
{
    assert(g_image_base == 0);
    return std::make_shared<Game>(baseAddress);
}