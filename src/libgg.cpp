#include <Windows.h>

#include <cstdint>

#include "mini_reflection.h"
//#include "binary_serializer.h"
#include "memory_dump.h"

#include <Windows.h>
#include <objbase.h>

#include <D3D9.h>

#include <array>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using mini_reflection::reflect;
using mini_reflection::member_tuple;

using memory_dump::load;
using memory_dump::dump;
using memory_dump::ptr_chain;
using memory_dump::rel_mem_ptr;
using memory_dump::memory_offset;
using memory_dump::local_memory_accessor;
using memory_dump::offset_value;

struct input_data
{
	uint16_t keys[2];
	struct joystick
	{
		uint8_t axes[4];
	} joysticks[4];
	uint32_t unknown[2];
};

char* g_image_base = 0;

void __cdecl get_raw_input_data(input_data* out);
int32_t limit_fps();

typedef struct XACT_WAVE_INSTANCE_PROPERTIES
{
} XACT_WAVE_INSTANCE_PROPERTIES, *LPXACT_WAVE_INSTANCE_PROPERTIES;

DECLARE_INTERFACE(IXACT3Wave)
{
	STDMETHOD(Destroy)(THIS) PURE;
	STDMETHOD(Play)(THIS) PURE;
	STDMETHOD(Stop)(THIS_ DWORD dwFlags) PURE;
	STDMETHOD(Pause)(THIS_ BOOL fPause) PURE;
	STDMETHOD(GetState)(THIS_ __out DWORD* pdwState) PURE;
	STDMETHOD(SetPitch)(THIS_ SHORT pitch) PURE;
	STDMETHOD(SetVolume)(THIS_ FLOAT volume) PURE;
	STDMETHOD(SetMatrixCoefficients)(THIS_ UINT32 uSrcChannelCount, UINT32 uDstChannelCount, __in float* pMatrixCoefficients) PURE;
	STDMETHOD(GetProperties)(THIS_ __out LPXACT_WAVE_INSTANCE_PROPERTIES pProperties) PURE;
};

struct IXACT3WaveBank {};
// IXACT3WaveBank_Play(__in IXACT3WaveBank* pWaveBank, XACTINDEX nWaveIndex, DWORD dwFlags, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount, __deref_out IXACT3Wave** ppWave)
int32_t __stdcall play_sound(IXACT3WaveBank*, int16_t, uint32_t, int32_t, int8_t, IXACT3Wave**);
void game_tick();
void __stdcall sleep(uint32_t ms);
const auto sleep_ptr = &sleep;
int __cdecl write_cockpit_font_internal(int x, int y, float unknown, uint8_t alpha, float scale);
// Wrapper for write_cockpit_font_internal
int write_cockpit_font(const char* buffer, int x, int y, float unknown, uint8_t alpha, float scale);
void process_input();
void process_objects();

typedef int (fiber_func_t)();

struct menu_fiber
{
	uint32_t status; // 0
	uint32_t unknown[5]; // 4
	char name[0x18]; // // 18
	fiber_func_t* func;  //30
	LPVOID fiber; // 34
}; // 38

static_assert(sizeof(menu_fiber) == 0x38);

struct gg_char_state
{
	// common state + char-specific stuff (via union)
	// Doesn't contain any pointers
	// TODO: elaborate later
	char data[0x148];
};

struct player_button_timers
{
	// Doesn't contain any pointers
	// TODO: elaborate later
	char data[100];
};

struct player_direction_timers
{
	uint32_t timers[2];
#pragma pack(push, 1)
	struct timer
	{
		uint8_t dir;
		uint8_t timer;
	} dir_timers[2][32];
#pragma pack(pop)
	uint32_t dir_timer_idx[2];
};

struct camera_state
{
	// Doesn't contain any pointers
	// TODO: elaborate later
	char data[0xAC];
};

struct extra_config
{
	// Attack/defense/tension rate etc
	// Doesn't contain any pointers
	// TODO: elaborate later
	char data[0x98];
};

struct player_controller_state
{
	// Doesn't contain any pointers
	// TODO: elaborate later
	uint32_t sideswapped;
	char data1[8];
	uint32_t pressed;
	uint32_t depressed;
	char data2[132];
};

struct controller_state
{
	uint32_t bitmask_cur;
	uint32_t bitmask_prev;
	uint32_t bitmask_pressed_prev;
	uint32_t bitmask_pressed_cur;
	uint32_t bitmask_depressed;
	uint32_t timers[16];
	char unused[64];
	bool is_active;
};

static_assert(sizeof(controller_state) == 0x98);

struct mersenne_twister
{
	uint32_t index;
	uint32_t unknown;
	uint64_t data[624];
	bool twist_if_false;
};

static_assert(sizeof(mersenne_twister) == 5008);

struct sprite
{
};

struct hitbox
{
};


#pragma pack(push, 1)
template<size_t Size>
struct data_size
{
	uint8_t data[Size];
};
#pragma pack(pop)

struct active_object_state;
typedef int (palette_reset_func_t)(const active_object_state*);

#pragma pack(push, 1)
struct active_object_state
{
	uint16_t id; // 0
	uint8_t facing; // 2
	uint8_t side; // 3
	active_object_state* prev; // 4
	active_object_state* next; // 8
	uint32_t status_bitmask; // C
	uint32_t unknown_bitmask; // 10
	uint16_t active_move_prev; // 14
	uint16_t pad1; // 16
	uint16_t active_move; // 18
	uint16_t active_move_followup; // 1a
	uint16_t active_move_frame; // 1c
	uint16_t health; // 1e
	active_object_state* other_; // 20
	uint16_t last_jump_dir; // 24
	uint8_t unknown3; // 26
	uint8_t owner_id; // 27
	uint16_t unknown4; // 28
	uint16_t guard_status; // 2A
	gg_char_state* char_state_ptr; // 2C
	void* unknown_callback; // 30
	uint32_t unknown5[2]; // 34
	sprite* sprite_array_a; // 3C
	uint32_t sprite_array_b_idx; // 40
	uint32_t sprite_array_a_idx; // 44
	sprite* sprite_array_b; // 48
	uint32_t unknown6[2]; // 4C
	hitbox* hitbox_array; // 54
	uint32_t unknown7[3]; // 58
	active_object_state* other1; // 64
	uint32_t unknown8; // 68
	active_object_state* other2; // 6C
	uint8_t unknown9[0x14]; // 70
	uint8_t hitbox_count; // 84
	uint8_t unknown99[3]; // 85
	ptr_chain<data_size<0x5c>, 0, 0> data_5c1; // 88
	//data_5c* data_5c1; // 88
	ptr_chain<data_size<0x5c>, 0, 0> data_5c2; // 8C
	//data_5c* data_5c2; // 8C
	void* hit_block_callback; // 90
	palette_reset_func_t* reset_palette_callback; // 94
	uint8_t unknown10[0x18]; // 98
	int32_t pos_x; // b0
	int32_t pos_y; // b4
	int32_t velocity_x; // b8
	int32_t velocity_y; // bc
	uint32_t unknown11[8]; // c0
	void* unknown_ptr1; // e0
	uint32_t unknown12[2]; // e4
	void* unknown_ptr2; // ec
	void* unknown_ptr3; // f0
	uint32_t unknown13; // f4
	uint16_t unknown14; // f8
	uint16_t unknown15; // fa
	uint8_t unknown16; // fc
	uint8_t hitstop_countdown; // fd
	uint8_t unknown17[0x32]; // fe - 130
};
#pragma pack(pop)

template<>
struct reflect<active_object_state>
{
	constexpr static auto members = member_tuple(
		&active_object_state::id,
		&active_object_state::facing,
		&active_object_state::side,
		&active_object_state::prev,
		&active_object_state::next,
		&active_object_state::status_bitmask,
		&active_object_state::unknown_bitmask,
		&active_object_state::active_move_prev,
		&active_object_state::pad1,
		&active_object_state::active_move,
		&active_object_state::active_move_followup,
		&active_object_state::active_move_frame,
		&active_object_state::health,
		&active_object_state::other_,
		&active_object_state::last_jump_dir,
		&active_object_state::unknown3,
		&active_object_state::owner_id,
		&active_object_state::unknown4,
		&active_object_state::guard_status,
		&active_object_state::char_state_ptr,
		&active_object_state::unknown_callback,
		&active_object_state::unknown5,
		&active_object_state::sprite_array_a,
		&active_object_state::sprite_array_b_idx,
		&active_object_state::sprite_array_a_idx,
		&active_object_state::sprite_array_b,
		&active_object_state::unknown6,
		&active_object_state::hitbox_array,
		&active_object_state::unknown7,
		&active_object_state::other1,
		&active_object_state::unknown8,
		&active_object_state::other2,
		&active_object_state::unknown9,
		&active_object_state::hitbox_count,
		&active_object_state::unknown99,
		&active_object_state::data_5c1,
		&active_object_state::data_5c2,
		&active_object_state::hit_block_callback,
		&active_object_state::reset_palette_callback,
		&active_object_state::unknown10,
		&active_object_state::pos_x,
		&active_object_state::pos_y,
		&active_object_state::velocity_x,
		&active_object_state::velocity_y,
		&active_object_state::unknown11,
		&active_object_state::unknown_ptr1,
		&active_object_state::unknown12,
		&active_object_state::unknown_ptr2,
		&active_object_state::unknown_ptr3,
		&active_object_state::unknown13,
		&active_object_state::unknown14,
		&active_object_state::unknown15,
		&active_object_state::unknown16,
		&active_object_state::hitstop_countdown,
		&active_object_state::unknown17
	);
};

//static_assert(sizeof(active_object_state) == 0x130);

struct projectiles
{
	active_object_state objects[98];
};

template<>
struct reflect<projectiles>
{
	constexpr static auto members = member_tuple(
		&projectiles::objects
	);
};

struct match_state
{
	memory_offset<uint8_t, 0x50f7ec> p1_rounds_won;
	memory_offset<uint8_t, 0x50f7ed> p2_rounds_won;
	memory_offset<uint32_t, 0x50f7fc> round_end_bitmask;
	memory_offset<uint16_t, 0x50F800> match_countdown;
	memory_offset<uint8_t, 0x5113C0> round_state;
	memory_offset<gg_char_state[2], 0x51A038> character_state;
	memory_offset<camera_state, 0x51B0D4> camera_state;
	memory_offset<player_button_timers, 0x516010> player_button_timers;
	memory_offset<player_direction_timers, 0x5161f8> player_direction_timers;
	memory_offset<player_controller_state[2], 0x516294> player_controller_state;
	memory_offset<uint8_t, 0x51B814> p1_selected_palette;
	memory_offset<uint8_t, 0x51B816> p2_selected_palette;
	// TODO: ac / gg / ggx
	memory_offset<bool, 0x51B8F8> p1_ex_enabled;
	memory_offset<bool, 0x51B8FC> p2_ex_enabled;
	// There are actually 4 controllers, but 3-4 are copies of 2
	memory_offset<controller_state[2], 0x51EDC8> controller_state;
	// TODO: double check array length
	memory_offset<active_object_state*, 0x519E5C> extra_object_ptr1;
	memory_offset<active_object_state*, 0x519E60> extra_object_ptr2;
	//memory_offset<active_object_state*, 0x519E60> extra_objects2;
	memory_offset<ptr_chain<active_object_state, 0, 0>, 0x516778> p1_character;
	memory_offset<ptr_chain<active_object_state, 0, 0>, 0x51A07C> p2_character;
	memory_offset<ptr_chain<projectiles, 0, 0>, 0x51677C> projectiles;

	memory_offset<uint8_t[0x18B78], 0x4FDC00> training_mode_history;

	memory_offset<uint32_t, 0x555FEC> pause_state;

	memory_offset<uint32_t, 0x3E37FC> a1;
	memory_offset<uint32_t, 0x4F80E4> a2;
	memory_offset<uint32_t, 0x50F800> a3;
	memory_offset<uint32_t, 0x5113B4> a4;
	memory_offset<active_object_state, 0x517BA8> a5;
	memory_offset<uint32_t, 0x51B798> a6;
	memory_offset<uint32_t, 0x51B7A4> a7;
	memory_offset<uint32_t, 0x51B914> a8;
	memory_offset<uint32_t, 0x51B9DC> a9;
	memory_offset<uint32_t, 0x51EDD4> a10;
	memory_offset<uint32_t, 0x51EE6C> a11;
	memory_offset<uint32_t, 0x51EF04> a12;
	memory_offset<uint32_t, 0x51EF9C> a13;
	memory_offset<uint32_t, 0x555D28> a14;
	memory_offset<uint32_t, 0x555FEC> a15;
	memory_offset<uint32_t, 0x555FF4> a16;
	memory_offset<uint32_t, 0x55602C> a17;
	memory_offset<uint32_t, 0x5561A8> a18;
	memory_offset<active_object_state, 0x517A78> a19;
	memory_offset<uint8_t[0x2800], 0x5489F8> a20;
	memory_offset<uint8_t[0x2800], 0x54B198> a21;
	memory_offset<uint8_t[0x54], 0x506690> a22;
	memory_offset<ptr_chain<data_size<0x1CFF0>, 0, 0>, 0x50669C> a23;
	memory_offset<uint8_t*, 0x5066A0> memory_begin;
	memory_offset<ptr_chain<data_size<0x1CFF0>, 0, 0>, 0x5066A4> a25;
	memory_offset<ptr_chain<data_size<0x1CFF0>, 0, 0>, 0x5066A8> a26;
	//memory_offset<data_size<0x400>, 0x5481E3> palette_p1;
	//memory_offset<data_size<0x400>, 0x5485E3> palette_p2;
};

static_assert(sizeof(player_controller_state) == 152);
static_assert(sizeof(match_state().player_direction_timers.get()) == 144);

template<>
struct reflect<match_state>
{
	constexpr static auto members = member_tuple(
		&match_state::p1_rounds_won,
		&match_state::p2_rounds_won,
		&match_state::round_end_bitmask,
		&match_state::match_countdown,
		&match_state::round_state,
		&match_state::character_state,
		&match_state::camera_state,
		&match_state::player_button_timers,
		&match_state::player_direction_timers,
		&match_state::p1_selected_palette,
		&match_state::p2_selected_palette,
		&match_state::p1_ex_enabled,
		&match_state::p2_ex_enabled,
		&match_state::controller_state,
		/*&match_state::p1_character,
		&match_state::p2_character,
		&match_state::projectiles,
		&match_state::projectile_ptrs,*/
		&match_state::extra_object_ptr1,
		&match_state::extra_object_ptr2,
		&match_state::p1_character,
		&match_state::p2_character,
		&match_state::projectiles,
		&match_state::training_mode_history,
		&match_state::pause_state,
		&match_state::a1,
		&match_state::a2,
		&match_state::a3,
		&match_state::a4,
		&match_state::a5,
		&match_state::a6,
		&match_state::a7,
		&match_state::a8,
		&match_state::a9,
		&match_state::a10,
		&match_state::a11,
		&match_state::a12,
		&match_state::a13,
		&match_state::a14,
		&match_state::a15,
		&match_state::a16,
		&match_state::a17,
		&match_state::a18,
		&match_state::a19,
		&match_state::a20,
		&match_state::a21,
		&match_state::a22,
		&match_state::a23,
		&match_state::memory_begin,
		&match_state::a25,
		&match_state::a26/*,
		&match_state::palette_p1,
		&match_state::palette_p2*/
	);
};

#pragma pack(push, 1)
struct game_config
{
	struct controller_config
	{
		struct bitmask
		{
			uint16_t bit;
			uint16_t pad;
		};
		static constexpr bitmask up = {0x10};
		static constexpr bitmask down = {0x40};
		static constexpr bitmask left = {0x80};
		static constexpr bitmask right = {0x20};
		bitmask p; // 0
		bitmask k; // 4
		bitmask s; // 8
		bitmask hs; // c
		bitmask d; // 10
		bitmask taunt; // 14
		bitmask reset; // 18
		bitmask pause; // 1c
		bitmask rec_player; // 20
		bitmask rec_enemy; // 24
		bitmask play_memory; // 28
		bitmask switch_control; // 2c
		bitmask enemy_walk; //30
		bitmask enemy_jump; // 34
		bitmask pk; // 38
		bitmask pd; // 3c
		bitmask pks; // 40
		bitmask pksh; // 44
		// 1-4, 0=controller, 5=custom
		uint32_t preset_id; // 48
		uint32_t unknown2; // 4c
	};
	controller_config player_controller_config[2];
	controller_config controller_config_presets[4];

	uint8_t unknown[0x2948];
};
#pragma pack(pop)

static_assert(sizeof(game_config::controller_config) == 0x50);
static_assert(sizeof(game_config) == 0x2b28);

uint16_t reverse_bytes(uint16_t value)
{
	return value / 256 + (value & 0xff) * 256;
}

enum class fiber_id : uint32_t
{
	none1 = 0,
	none2 = 0xa,
	title = 9,
	movie = 45,
	main_menu = 0x19,
	charselect = 0xf
};

enum class main_menu_idx : uint32_t
{
	training = 7
};

struct gg_state
{
	// TODO: memory_offset => external
	// TODO: remove rel_mem_ptr altogether
	memory_offset<rel_mem_ptr<decltype(game_tick)>, 0x14756E + 1> game_tick;
	memory_offset<rel_mem_ptr<decltype(process_input)>, 0x146F95 + 1> process_input;
	memory_offset<rel_mem_ptr<decltype(get_raw_input_data)>, 0x5263d + 1> get_raw_input_data;
	memory_offset<rel_mem_ptr<decltype(process_objects)>, 0x146F9A + 1> process_objects;
	memory_offset<rel_mem_ptr<decltype(limit_fps)>, 0x14ADC2 + 1> limit_fps;
	memory_offset<decltype(&sleep_ptr), 0x14770C + 2> sleep_ptr;
	//**(*(*(:base+0x556020)+0x58)+0x34)+0x14
	memory_offset<
		ptr_chain<decltype(play_sound)*, 0, 0x58, 0x34, 0, 0x14>,
		0x556020> play_sound;
	// TODO: how to populate it automatically?
	decltype(write_cockpit_font_internal)* write_cockpit_font = nullptr;
	memory_offset<IDirect3DDevice9*, 0x555B94> direct3d9;
	memory_offset<extra_config, 0x51B180> extra_config[2];
	// TODO: at least 14! Double check
	memory_offset<menu_fiber[14], 0x54f030> menu_fibers;
	memory_offset<mersenne_twister, 0x565F20> rng;
	memory_offset<uint32_t, 0x51B8CC> game_mode;
	//memory_offset<std::array<uint8_t, 0x3614>, 0x516514> crapola;
	memory_offset<uint8_t*, 0x54EE14> memory_begin;
	memory_offset<uint8_t*, 0x54B208> memory_end;
	memory_offset<game_config, 0x5134D8> config;
	memory_offset<fiber_id, 0x555FF4> next_fiber_id;
	memory_offset<bool, 0x5136C4> skip_saving;
	memory_offset<bool, 0x50BF30> charselect_p1_enabled;
	memory_offset<bool, 0x50BF68> charselect_p2_enabled;
	memory_offset<main_menu_idx, 0x5557B0> main_menu_idx;
} g_state, g_state_orig;

template<>
struct reflect<gg_state>
{
	constexpr static auto members = member_tuple(
		&gg_state::get_raw_input_data,
		&gg_state::limit_fps,
		&gg_state::game_tick,
		&gg_state::sleep_ptr,
		&gg_state::play_sound,
		&gg_state::process_objects,
		&gg_state::process_input,
		&gg_state::direct3d9,
		&gg_state::extra_config,
		&gg_state::menu_fibers,
		&gg_state::rng,
		&gg_state::game_mode,
		&gg_state::memory_begin,
		&gg_state::memory_end,
		&gg_state::config,
		&gg_state::next_fiber_id,
		&gg_state::skip_saving,
		&gg_state::charselect_p1_enabled,
		&gg_state::charselect_p2_enabled,
		&gg_state::main_menu_idx
	);
};

void EnableDrawing(bool enable)
{
	const auto addr = (size_t)g_image_base + 0x146FFD;
	if (enable)
	{
		// 0x75 = jne
		local_memory_accessor::write((char)0x75, addr);
	}
	else
	{
		// 0xEB = jmp
		local_memory_accessor::write((char)0xEB, addr);
	}
}

void EnableRoundEndCondition(bool enable)
{
	const auto addr = (size_t)g_image_base + 0x664CB;
	if (enable)
	{
		// 0F8F = jg
		const uint8_t data[] = {0x0f, 0x8f};
		local_memory_accessor::write(data, addr);
	}
	else
	{
		// 90E9 = nop jmp
		const uint8_t data[] = {0x90, 0xe9};
		local_memory_accessor::write(data, addr);
	}
}

// untested, but should work
void EnablePauseMenu(bool enable)
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

extern "C" __declspec(dllexport) void libgg_init()
{
	static bool s_is_ready = false;
	if (!s_is_ready)
	{
		g_image_base = (char*)::GetModuleHandle(nullptr);
		load(g_image_base, g_state);
		g_state.write_cockpit_font = reinterpret_cast<decltype(write_cockpit_font_internal)*>(g_image_base + 0x10E530);
		g_state_orig = g_state;
		s_is_ready = true;
		g_state.get_raw_input_data.get().set(get_raw_input_data);
		g_state.limit_fps.get().set(limit_fps);
		g_state.game_tick.get().set(game_tick);
		g_state.process_input.get().set(process_input);
		g_state.process_objects.get().set(process_objects);
		g_state.sleep_ptr = &sleep_ptr;
		if (g_state.play_sound.get().ptr)
			g_state.play_sound.get().ptr = play_sound;
		dump(g_state, g_image_base);
	}
}

bool g_enable_fps_limit = true;


bool in_match()
{
	const auto& fibers = g_state.menu_fibers.get();
	for (const auto& f : fibers)
	{
		if (f.name == std::string("FIN ")) // TODO: find something less hacky
			continue;
		if (f.status)
			return false;
	}
	return true;
}

void queue_destroy_fibers()
{
	auto& fibers = g_state.menu_fibers.get();
	for (auto& f : fibers)
	{
		if (f.status)
			f.status = 3;
	}
	dump(g_state.menu_fibers, g_image_base);
}

using history_t = std::tuple<
	match_state,
	decltype(g_state.rng),
	std::vector<uint8_t>,
	std::vector<uint8_t>,
	input_data
>;
std::deque<history_t> g_history;
bool g_recording = false;
bool g_playing = false;
// 0: frame step
// (-)1: normal (reverse)
// (-)2: disable fps limit (reverse)
// (-)3: disable drawing (reverse)
int8_t g_speed = 1;
bool g_manual_frame_advance = false;
uint16_t g_prev_bitmask[] = {0, 0};
size_t g_playback_idx = 0;
size_t g_record_idx = 0;
uint16_t g_speed_control_counter = 0;
std::optional<history_t> g_prev_state;
history_t g_cur_state;

// rec player = rec / stop recording
// rec enemy = stop world
// play memory = play / stop playing
// enemy walk = reverse, twice = max speed reverse
// enemy jump = forward, twice = max speed forward
// reset = reset current input
void __cdecl get_raw_input_data(input_data* out)
{
	const auto f = *g_state_orig.get_raw_input_data.get().get();

	input_data input;
	f(&input);
	//std::swap(input.keys[0], input.keys[1]);

	load(g_image_base, g_state.menu_fibers);
	load(g_image_base, g_state.game_mode);

	load(g_image_base, std::get<0>(g_cur_state));

	if (in_match() && !std::get<0>(g_cur_state).pause_state.get())
	{
		if (g_speed <= 0)
		{
			g_cur_state = *g_prev_state;
			const auto& ms = std::get<0>(g_cur_state);
			const auto& rng = std::get<1>(g_cur_state);
			const auto& data1 = std::get<2>(g_cur_state);
			std::copy(data1.begin(), data1.end(), g_state.memory_begin.get());
			const auto& data2 = std::get<3>(g_cur_state);
			std::copy(data2.begin(), data2.end(), ms.memory_begin.get());
			dump(ms, g_image_base);
			dump(rng, g_image_base);
		}
		else
		{
			load(g_image_base, std::get<1>(g_cur_state));
			std::get<2>(g_cur_state).clear();
			std::copy(g_state.memory_begin.get(), g_state.memory_end.get(),
						std::back_inserter(std::get<2>(g_cur_state)));
			std::get<3>(g_cur_state).clear();
			const auto& cur_ms = std::get<0>(g_cur_state);
			std::copy(cur_ms.memory_begin.get(), cur_ms.memory_begin.get() + 0xC8FE0,
						std::back_inserter(std::get<3>(g_cur_state)));
		}

		bool speed_control_enabled = false;
		const auto& controller_configs = g_state.config.get().player_controller_config;
		for (size_t i = 0; i < 2; ++i)
		{
			uint16_t bitmask = reverse_bytes(input.keys[i]);
			const auto& cfg = controller_configs[i];

			const uint16_t training_mode_buttons = reverse_bytes(
				cfg.rec_player.bit | cfg.play_memory.bit | cfg.rec_enemy.bit |
				cfg.enemy_jump.bit | cfg.enemy_walk.bit);

			input.keys[i] = input.keys[i] & ~training_mode_buttons;
			input.keys[i] = input.keys[i] & ~training_mode_buttons;

			if (g_recording || g_playing || g_manual_frame_advance)
				input.keys[i] = input.keys[i] & ~reverse_bytes(cfg.pause.bit);

			if (g_manual_frame_advance)
			{
				input.keys[i] = input.keys[i] & ~reverse_bytes(cfg.reset.bit);

				if (bitmask & cfg.reset.bit)
				{
					input.keys[i] = 0;
				}
				else
				{
					if (input.keys[i] == (input.keys[i] & std::get<4>(*g_prev_state).keys[i]))
						input.keys[i] = std::get<4>(*g_prev_state).keys[i];
				}
			}

			if ((bitmask & cfg.rec_player.bit) && !(g_prev_bitmask[i] & cfg.rec_player.bit))
			{
				g_playing = false;
				g_recording = !g_recording;
				if (g_recording)
					g_history.clear();
				g_record_idx = 0;
			}

			if ((bitmask & cfg.play_memory.bit) && !(g_prev_bitmask[i] & cfg.play_memory.bit))
			{
				g_recording = false;
				g_playing = !g_playing;
				g_playback_idx = 0;
			}

			if ((bitmask & cfg.rec_enemy.bit) && !(g_prev_bitmask[i] & cfg.rec_enemy.bit))
			{
				g_manual_frame_advance = !g_manual_frame_advance;
				g_speed = g_manual_frame_advance ? 0 : 1;
			}

			if (bitmask & cfg.enemy_jump.bit)
			{
				speed_control_enabled = true;
				if (!g_manual_frame_advance)
				{
					g_speed = 2;
				}
				else
				{
					if (!(g_prev_bitmask[i] & cfg.enemy_jump.bit) || g_speed_control_counter > 60)
					{
						g_speed = 1;
					}
					else
					{
						g_speed = 0;
					}
				}
				++g_speed_control_counter;
			}

			if (bitmask & cfg.enemy_walk.bit)
			{
				speed_control_enabled = true;
				if (!g_manual_frame_advance)
				{
					g_speed = 0;
				}
				else
				{
					if (!(g_prev_bitmask[i] & cfg.enemy_walk.bit) || g_speed_control_counter > 60)
					{
						g_speed = -1;
					}
					else
					{
						g_speed = 0;
					}
				}
				++g_speed_control_counter;
			}

			g_prev_bitmask[i] = bitmask;
		}

		if (!speed_control_enabled)
		{
			if (g_manual_frame_advance)
				g_speed = 0;
			else
				g_speed = 1;
			g_speed_control_counter = 0;
		}

		bool drawing_enabled = true;
		bool fps_limit_enabled = true;
		if (std::abs(g_speed) == 2)
			fps_limit_enabled = false;
		if (std::abs(g_speed) == 3)
			drawing_enabled = false;

		EnableDrawing(drawing_enabled);
		g_enable_fps_limit = fps_limit_enabled;

		std::get<4>(g_cur_state) = input;

		if (g_playing)
		{
			if (g_playback_idx < g_history.size())
			{
				g_cur_state = g_history[g_playback_idx];
				const auto& ms = std::get<0>(g_cur_state);
				const auto& rng = std::get<1>(g_cur_state);
				const auto& data1 = std::get<2>(g_cur_state);
				const auto& data2 = std::get<3>(g_cur_state);
				input = std::get<4>(g_cur_state);
				std::copy(data1.begin(), data1.end(), g_state.memory_begin.get());
				std::copy(data2.begin(), data2.end(), ms.memory_begin.get());
				dump(ms, g_image_base);
				dump(rng, g_image_base);

				if (g_speed < 0)
				{
					--g_playback_idx;
					if (g_playback_idx == -1)
						g_playback_idx = 0;
				}
				else if (g_speed > 0)
				{
					++g_playback_idx;
				}
			}
			else
			{
				g_playing = false;
			}

			if (g_playback_idx > g_history.size() && g_manual_frame_advance)
				g_playback_idx = g_history.size();
		}

		if (g_recording && g_record_idx < 900)
		{
			if (g_record_idx >= g_history.size())
				g_history.resize(g_record_idx + 1);
			g_history[g_record_idx] = g_cur_state;

			if (g_speed < 0)
			{
				--g_record_idx;
				if (g_record_idx == 0)
					g_record_idx = 0;
			}
			else if (g_speed > 0)
			{
				++g_record_idx;
			}

			if (g_record_idx < g_history.size())
				g_cur_state = g_history[g_record_idx];
		}
		else
		{
			g_recording = false;
		}


		g_prev_state = g_cur_state;
	}
	else
	{
		g_history.clear();
		g_recording = false;
		g_playing = false;
		g_speed = 1;
		g_manual_frame_advance = false;
		g_speed_control_counter = 0;
	}

	EnableRoundEndCondition(!g_recording);

	*out = input;
}

int32_t limit_fps()
{
	const auto f = *g_state_orig.limit_fps.get().get();
	return g_enable_fps_limit ? f() : 0;
}

void game_tick()
{
	if (!g_state.play_sound.get().ptr)
	{
		load(g_image_base, g_state.play_sound);
		if (g_state.play_sound.get().ptr)
		{
			g_state_orig.play_sound = g_state.play_sound;
			g_state.play_sound.get().ptr = play_sound;
		}
		dump(g_state.play_sound, g_image_base);
	}

	if (g_state.memory_begin.get() == g_state.memory_end.get())
	{
		load(g_image_base, g_state.memory_begin);
		load(g_image_base, g_state.memory_end);
	}

	if (!g_state.direct3d9.get())
	{
		load(g_image_base, g_state.direct3d9);
	}

	load(g_image_base, g_state.config);

	load(g_image_base, g_state.next_fiber_id);
	if (g_state.next_fiber_id.get() == fiber_id::title)
	{
		g_state.main_menu_idx = main_menu_idx::training;
		g_state.next_fiber_id = fiber_id::main_menu;
		dump(g_state.next_fiber_id, g_image_base);
		dump(g_state.main_menu_idx, g_image_base);
	}

	const auto f = *g_state_orig.game_tick.get().get();
	f();
}

void __stdcall sleep(uint32_t ms)
{
	const auto f = **g_state_orig.sleep_ptr.get();
	f(ms);
}
// -----------------------------------------------------------------------------
// XACT State flags
// -----------------------------------------------------------------------------
static const DWORD XACT_STATE_CREATED           = 0x00000001; // Created, but nothing else
static const DWORD XACT_STATE_PREPARING         = 0x00000002; // In the middle of preparing
static const DWORD XACT_STATE_PREPARED          = 0x00000004; // Prepared, but not yet played
static const DWORD XACT_STATE_PLAYING           = 0x00000008; // Playing (though could be paused)
static const DWORD XACT_STATE_STOPPING          = 0x00000010; // Stopping
static const DWORD XACT_STATE_STOPPED           = 0x00000020; // Stopped
static const DWORD XACT_STATE_PAUSED            = 0x00000040; // Paused (Can be combined with some of the other state flags above)
static const DWORD XACT_STATE_INUSE             = 0x00000080; // Object is in use (used by wavebanks and soundbanks).
static const DWORD XACT_STATE_PREPAREFAILED     = 0x80000000; // Object preparation failed.

struct XACT3Wave : public IXACT3Wave
{
	STDMETHOD(Destroy)(THIS)
	{
		return S_OK;
	}
	STDMETHOD(Play)(THIS)
	{
		return S_OK;
	}
	STDMETHOD(Stop)(THIS_ DWORD dwFlags)
	{
		return S_OK;
	}
	STDMETHOD(Pause)(THIS_ BOOL fPause)
	{
		return S_OK;
	}
	STDMETHOD(GetState)(THIS_ __out DWORD* pdwState)
	{
		*pdwState = XACT_STATE_STOPPED;
		return S_OK;
	}
	STDMETHOD(SetPitch)(THIS_ SHORT pitch)
	{
		return S_OK;
	}
	STDMETHOD(SetVolume)(THIS_ FLOAT volume)
	{
		return S_OK;
	}
	STDMETHOD(SetMatrixCoefficients)(THIS_ UINT32 uSrcChannelCount, UINT32 uDstChannelCount, __in float* pMatrixCoefficients)
	{
		return S_OK;
	}
	STDMETHOD(GetProperties)(THIS_ __out LPXACT_WAVE_INSTANCE_PROPERTIES pProperties)
	{
		return S_OK;
	}
};

XACT3Wave w;
std::unordered_map<IXACT3WaveBank*, std::unordered_set<int16_t>> known_sounds;
int32_t __stdcall play_sound(IXACT3WaveBank* a1, int16_t a2, uint32_t a3, int32_t a4, int8_t a5, IXACT3Wave** a6)
{
	auto [_, success] = known_sounds[a1].insert(a2);
	if (success)
	{
		const auto f = *g_state_orig.play_sound.get().ptr;
		return f(a1, a2, a3, a4, a5, a6);
	}
	else
	{
		*a6 = &w;
		return 0;
	}
}

int write_cockpit_font(const char* buffer, int x, int y, float unknown, uint8_t alpha, float scale)
{
	auto f = g_state.write_cockpit_font;
	uint32_t alpha_ = alpha;
	// Custom calling convention due to LTCG:
	// * First argument in EAX
	// * Cleanup by the caller
	__asm
	{
		push scale
		push alpha_
		push unknown
		push y
		push x
		mov eax, buffer
		call f
		add esp, 4*5
	}
}

void process_input()
{
	const auto f = *g_state_orig.process_input.get().get();
	f();
}

void process_objects()
{
	const auto f = *g_state_orig.process_objects.get().get();
	f();
	write_cockpit_font("DEV BUILD", 50, 100, 1, 0x50, 1);
	if (g_recording)
	{
		auto str = "REC " + std::to_string(g_record_idx);
		write_cockpit_font(str.c_str(), 285, 100, 1, 0xFF, 1);
	}
	if (g_playing)
	{
		auto str = "PLAY " + std::to_string(g_playback_idx);
		write_cockpit_font(str.c_str(), 285, 100, 1, 0xFF, 1);
	}
	if (g_speed != 1 || g_manual_frame_advance)
	{
		auto str = "SPEED " + std::to_string(g_speed);
		write_cockpit_font(str.c_str(), 285, 150, 1, 0xFF, 1);
	}
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	};
	return TRUE;
}
