/* AudioStreamTaud — a .taud song as a Godot AudioStream, and
 * AudioStreamPlaybackTaud — one running copy of it, with the taudplay surface:
 * a transport, one fader per lane, two probes per lane, and the sixteen
 * interrupts a song can fire at the game playing it.
 *
 * Copyright (C) 2026 CuriousTorvald. Licensed under the GNU Lesser General
 * Public License version 3 or later — see COPYING.LESSER.
 */
#ifndef AUDIO_STREAM_TAUD_H
#define AUDIO_STREAM_TAUD_H

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>

#include "taudplay.h"

namespace godot {

class AudioStreamTaud : public AudioStream {
	GDCLASS(AudioStreamTaud, AudioStream)

	PackedByteArray data;
	int song = 0;
	bool loop = false;
	bool binaural = false;
	int64_t random_seed = 1;

	/* What `data` holds, read on first ask (opening a player is the only way
	 * to find out, and it costs a few milliseconds). */
	mutable bool info_valid = false;
	mutable String title;
	mutable Array songs;
	void _update_info() const;

protected:
	static void _bind_methods();

public:
	static Ref<AudioStreamTaud> load_from_buffer(const PackedByteArray &p_data);
	static Ref<AudioStreamTaud> load_from_file(const String &p_path);

	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;
	void set_song(int p_song);
	int get_song() const;
	void set_loop(bool p_loop);
	bool has_loop() const;
	void set_binaural(bool p_binaural);
	bool is_binaural() const;
	void set_random_seed(int64_t p_seed);
	int64_t get_random_seed() const;

	String get_title() const;
	int get_song_count() const;
	Array get_songs() const;

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	String _get_stream_name() const override;
	double _get_length() const override;
	bool _is_monophonic() const override;
	double _get_bpm() const override;
};

class AudioStreamPlaybackTaud : public AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamPlaybackTaud, AudioStreamPlaybackResampled)
	friend class AudioStreamTaud;
	friend class TaudDispatcher;

	/* The engine is driven from the audio thread (_mix_resampled) and the
	 * knobs from the main thread; every touch of `player` holds `lock`. */
	TaudPlayer *player = nullptr;
	mutable std::mutex lock;
	int rate = TAUD_DEFAULT_RATE;
	bool loop = false;

	float block[TAUD_CHUNK * 2];
	int block_pos = 0;
	int block_len = 0;
	uint64_t next_block_frame = 0; // stream frame the next rendered block starts at
	uint64_t block_frame = 0;      // …and the one being rendered now
	bool tail = false;             // the song has ended; playing out a block of silence

	std::atomic<bool> active{ false };
	std::atomic<int> loops{ 0 };
	std::atomic<uint64_t> mixed_frames{ 0 }; // stream frames handed to Godot since _start

	/* Interrupts, stamped with the stream frame of the block they fired in.
	 * Single producer (the render, inside `lock`), single consumer (the main
	 * thread, in _dispatch), so the ring needs no lock of its own. */
	struct Event {
		uint64_t frame;
		uint16_t arg;
		uint8_t n;
	};
	static constexpr uint32_t EVENT_RING = 256;
	Event events[EVENT_RING];
	std::atomic<uint32_t> ev_head{ 0 };
	std::atomic<uint32_t> ev_tail{ 0 };
	Callable handlers[TAUD_NUM_INTERRUPTS];

	static void _on_interrupt(int p_n, unsigned p_arg, void *p_user);
	void _restart();
	void _dispatch(bool p_flush_all);

protected:
	static void _bind_methods();

public:
	~AudioStreamPlaybackTaud();

	/* Transport. The song starts from the top when the AudioStreamPlayer
	 * plays; these move it from there. */
	Error select_song(int p_index);
	int get_current_song() const;
	void seek_cue(int p_cue);
	void set_binaural(bool p_on);

	int get_cue() const;
	int get_row() const;
	int get_bpm() const;
	int get_speed() const;
	int get_channel_count() const;

	/* The knob: one fader per lane. */
	void set_voice_gain(int p_voice, double p_gain, double p_fade_seconds);
	double get_voice_gain(int p_voice) const;

	/* The probes: two per lane. */
	double get_voice_volume(int p_voice) const;
	double get_voice_pan(int p_voice) const;

	/* Interrupts: the song calling out. Also emitted as the `interrupt` signal. */
	void set_interrupt(int p_number, const Callable &p_handler);
	void clear_interrupts();

	void _start(double p_from_pos) override;
	void _stop() override;
	bool _is_playing() const override;
	int32_t _get_loop_count() const override;
	double _get_playback_position() const override;
	void _seek(double p_position) override;
	int32_t _mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) override;
	double _get_stream_sampling_rate() const override;
};

} // namespace godot

#endif // AUDIO_STREAM_TAUD_H
