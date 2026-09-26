#include "audio_stream_taud.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <limits>

#include "taud_dispatcher.h"

namespace godot {

/* ════════════════════════════════════════════════════════════════════════════
 *  AudioStreamTaud
 * ════════════════════════════════════════════════════════════════════════════ */

static TaudPlayer *open_player(const PackedByteArray &p_data, const TaudConfig *p_cfg, TaudError *r_err)
{
	*r_err = TAUD_ERR_FORMAT;
	if (p_data.is_empty()) return nullptr;
	return taud_open(p_data.ptr(), (size_t)p_data.size(), p_cfg, r_err);
}

Ref<AudioStreamTaud> AudioStreamTaud::load_from_buffer(const PackedByteArray &p_data)
{
	TaudError err;
	TaudPlayer *p = open_player(p_data, nullptr, &err);
	ERR_FAIL_NULL_V_MSG(p, Ref<AudioStreamTaud>(), String("Cannot load Taud song: ") + taud_error_string(err) + ".");
	taud_close(p);
	Ref<AudioStreamTaud> stream;
	stream.instantiate();
	stream->set_data(p_data);
	return stream;
}

Ref<AudioStreamTaud> AudioStreamTaud::load_from_file(const String &p_path)
{
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(bytes.is_empty(), Ref<AudioStreamTaud>(), "Cannot open Taud song '" + p_path + "'.");
	return load_from_buffer(bytes);
}

void AudioStreamTaud::_update_info() const
{
	if (info_valid) return;
	info_valid = true;
	title = String();
	songs.clear();
	TaudError err;
	TaudPlayer *p = open_player(data, nullptr, &err);
	if (p == nullptr) return;
	const char *t = taud_title(p);
	if (t != nullptr) title = String::utf8(t);
	for (int i = 0; i < taud_song_count(p); i++) {
		TaudSongInfo si;
		if (!taud_song_info(p, i, &si)) continue;
		Dictionary d;
		d["index"] = si.index;
		d["name"] = String::utf8(si.name);
		d["patterns"] = si.patterns;
		d["bpm"] = si.bpm;
		d["channels"] = si.channels;
		d["surround"] = si.surround != 0;
		songs.push_back(d);
	}
	taud_close(p);
}

void AudioStreamTaud::set_data(const PackedByteArray &p_data)
{
	data = p_data;
	info_valid = false;
	emit_changed();
}

PackedByteArray AudioStreamTaud::get_data() const { return data; }

void AudioStreamTaud::set_song(int p_song)
{
	song = p_song < 0 ? 0 : p_song;
	emit_changed();
}

int AudioStreamTaud::get_song() const { return song; }
void AudioStreamTaud::set_loop(bool p_loop) { loop = p_loop; }
bool AudioStreamTaud::has_loop() const { return loop; }
void AudioStreamTaud::set_binaural(bool p_binaural) { binaural = p_binaural; }
bool AudioStreamTaud::is_binaural() const { return binaural; }
void AudioStreamTaud::set_random_seed(int64_t p_seed) { random_seed = p_seed; }
int64_t AudioStreamTaud::get_random_seed() const { return random_seed; }

String AudioStreamTaud::get_title() const
{
	_update_info();
	return title;
}

int AudioStreamTaud::get_song_count() const
{
	_update_info();
	return songs.size();
}

Array AudioStreamTaud::get_songs() const
{
	_update_info();
	return songs.duplicate(true);
}

Ref<AudioStreamPlayback> AudioStreamTaud::_instantiate_playback() const
{
	TaudConfig cfg;
	taud_config_init(&cfg);
	/* The engine renders natively at any rate, so it renders at the mixer's
	 * and Godot's resampler only ever steps through it one frame at a time. */
	cfg.sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	cfg.song_index = song;
	cfg.binaural = binaural;
	cfg.random_seed = (uint32_t)random_seed;

	TaudError err;
	TaudPlayer *p = open_player(data, &cfg, &err);
	ERR_FAIL_NULL_V_MSG(p, Ref<AudioStreamPlayback>(), String("Cannot play Taud song: ") + taud_error_string(err) + ".");

	Ref<AudioStreamPlaybackTaud> pb;
	pb.instantiate();
	pb->player = p;
	pb->rate = taud_sample_rate(p);
	pb->loop = loop;
	for (int n = 0; n < TAUD_NUM_INTERRUPTS; n++) {
		taud_set_interrupt(p, n, &AudioStreamPlaybackTaud::_on_interrupt, pb.ptr());
	}
	if (TaudDispatcher::get_singleton() != nullptr) TaudDispatcher::get_singleton()->watch(pb);
	return pb;
}

String AudioStreamTaud::_get_stream_name() const
{
	String t = get_title();
	return t.is_empty() ? String("Taud") : t;
}

/* A tracker song's length is only known by playing it through (and a song
 * that jumps back never ends), so it reports none. */
double AudioStreamTaud::_get_length() const { return 0; }

bool AudioStreamTaud::_is_monophonic() const { return false; }

double AudioStreamTaud::_get_bpm() const
{
	_update_info();
	if (song >= songs.size()) return 0;
	return (int)Dictionary(songs[song])["bpm"];
}

void AudioStreamTaud::_bind_methods()
{
	ClassDB::bind_static_method("AudioStreamTaud", D_METHOD("load_from_buffer", "data"), &AudioStreamTaud::load_from_buffer);
	ClassDB::bind_static_method("AudioStreamTaud", D_METHOD("load_from_file", "path"), &AudioStreamTaud::load_from_file);

	ClassDB::bind_method(D_METHOD("set_data", "data"), &AudioStreamTaud::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &AudioStreamTaud::get_data);
	ClassDB::bind_method(D_METHOD("set_song", "song"), &AudioStreamTaud::set_song);
	ClassDB::bind_method(D_METHOD("get_song"), &AudioStreamTaud::get_song);
	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &AudioStreamTaud::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamTaud::has_loop);
	ClassDB::bind_method(D_METHOD("set_binaural", "enable"), &AudioStreamTaud::set_binaural);
	ClassDB::bind_method(D_METHOD("is_binaural"), &AudioStreamTaud::is_binaural);
	ClassDB::bind_method(D_METHOD("set_random_seed", "seed"), &AudioStreamTaud::set_random_seed);
	ClassDB::bind_method(D_METHOD("get_random_seed"), &AudioStreamTaud::get_random_seed);

	ClassDB::bind_method(D_METHOD("get_title"), &AudioStreamTaud::get_title);
	ClassDB::bind_method(D_METHOD("get_song_count"), &AudioStreamTaud::get_song_count);
	ClassDB::bind_method(D_METHOD("get_songs"), &AudioStreamTaud::get_songs);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "song", PROPERTY_HINT_RANGE, "0,63,1,or_greater"), "set_song", "get_song");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "binaural"), "set_binaural", "is_binaural");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "random_seed", PROPERTY_HINT_RANGE, "0,4294967295,1"), "set_random_seed", "get_random_seed");
}

/* ════════════════════════════════════════════════════════════════════════════
 *  AudioStreamPlaybackTaud
 * ════════════════════════════════════════════════════════════════════════════ */

AudioStreamPlaybackTaud::~AudioStreamPlaybackTaud()
{
	taud_close(player);
}

/* Runs on the audio thread, from inside taud_render_chunk (so under `lock`). */
void AudioStreamPlaybackTaud::_on_interrupt(int p_n, unsigned p_arg, void *p_user)
{
	AudioStreamPlaybackTaud *self = static_cast<AudioStreamPlaybackTaud *>(p_user);
	uint32_t head = self->ev_head.load(std::memory_order_relaxed);
	if (head - self->ev_tail.load(std::memory_order_acquire) >= EVENT_RING) return; // nobody is listening
	self->events[head % EVENT_RING] = { self->block_frame, (uint16_t)p_arg, (uint8_t)p_n };
	self->ev_head.store(head + 1, std::memory_order_release);
}

/* Main thread. */
void AudioStreamPlaybackTaud::_dispatch(bool p_flush_all)
{
	int64_t heard = p_flush_all ? std::numeric_limits<int64_t>::max() : TaudDispatcher::get_singleton()->heard_frame(this);
	for (;;) {
		/* Re-read each time round: a handler may have restarted the stream. */
		uint32_t tail = ev_tail.load(std::memory_order_relaxed);
		if (tail == ev_head.load(std::memory_order_acquire)) break;
		Event ev = events[tail % EVENT_RING];
		if ((int64_t)ev.frame > heard) break;
		ev_tail.store(tail + 1, std::memory_order_release);
		emit_signal("interrupt", ev.n, ev.arg);
		if (handlers[ev.n].is_valid()) handlers[ev.n].call(ev.arg);
	}
}

/* Under `lock`. */
void AudioStreamPlaybackTaud::_restart()
{
	taud_play(player);
	block_pos = block_len = 0;
	tail = false;
}

void AudioStreamPlaybackTaud::_start(double p_from_pos)
{
	/* A song has no timeline to start part-way along (its jumps decide where
	 * it goes); it starts from the top, and seek_cue() moves it on. */
	{
		std::lock_guard<std::mutex> g(lock);
		_restart();
		next_block_frame = 0;
		mixed_frames.store(0);
		loops.store(0);
		active.store(true);
	}
	ev_tail.store(ev_head.load(std::memory_order_acquire), std::memory_order_release);
	begin_resample();
}

void AudioStreamPlaybackTaud::_stop()
{
	std::lock_guard<std::mutex> g(lock);
	taud_stop(player);
	active.store(false);
}

bool AudioStreamPlaybackTaud::_is_playing() const { return active.load(); }
int32_t AudioStreamPlaybackTaud::_get_loop_count() const { return loops.load(); }

double AudioStreamPlaybackTaud::_get_playback_position() const
{
	return (double)mixed_frames.load(std::memory_order_acquire) / rate;
}

void AudioStreamPlaybackTaud::_seek(double p_position) {}

double AudioStreamPlaybackTaud::_get_stream_sampling_rate() const { return rate; }

int32_t AudioStreamPlaybackTaud::_mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count)
{
	std::lock_guard<std::mutex> g(lock);
	int32_t done = 0;
	while (done < p_frame_count) {
		if (block_pos >= block_len) {
			if (!active.load(std::memory_order_relaxed)) break;
			block_frame = next_block_frame;
			const float *out = taud_render_chunk(player);
			if (out == nullptr && loop) {
				_restart();
				loops.fetch_add(1);
				out = taud_render_chunk(player);
			}
			if (out == nullptr && !tail) {
				/* One block of silence after the song ends: Godot fades a
				 * finishing stream out from a little before the last frame it
				 * reports, and that fade should land on silence, not the song. */
				tail = true;
				std::memset(block, 0, sizeof block);
			} else if (out == nullptr) {
				active.store(false);
				break;
			} else {
				std::memcpy(block, out, sizeof block);
			}
			block_pos = 0;
			block_len = TAUD_CHUNK;
			next_block_frame += TAUD_CHUNK;
		}
		int32_t take = block_len - block_pos;
		if (take > p_frame_count - done) take = p_frame_count - done;
		const float *src = block + block_pos * 2;
		for (int32_t k = 0; k < take; k++) {
			p_dst_buffer[done + k].left = src[k * 2];
			p_dst_buffer[done + k].right = src[k * 2 + 1];
		}
		block_pos += take;
		done += take;
	}
	mixed_frames.fetch_add((uint64_t)done, std::memory_order_release);
	return done;
}

/* ── transport ── */

Error AudioStreamPlaybackTaud::select_song(int p_index)
{
	std::lock_guard<std::mutex> g(lock);
	TaudError err = taud_select_song(player, p_index);
	ERR_FAIL_COND_V_MSG(err == TAUD_ERR_RANGE, ERR_PARAMETER_RANGE_ERROR, "Song index out of range.");
	ERR_FAIL_COND_V_MSG(err != TAUD_OK, ERR_OUT_OF_MEMORY, taud_error_string(err));
	_restart();
	return OK;
}

int AudioStreamPlaybackTaud::get_current_song() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_current_song(player);
}

void AudioStreamPlaybackTaud::seek_cue(int p_cue)
{
	std::lock_guard<std::mutex> g(lock);
	taud_seek_cue(player, p_cue);
	block_pos = block_len = 0;
}

void AudioStreamPlaybackTaud::set_binaural(bool p_on)
{
	std::lock_guard<std::mutex> g(lock);
	taud_set_binaural(player, p_on);
}

int AudioStreamPlaybackTaud::get_cue() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_cue(player);
}

int AudioStreamPlaybackTaud::get_row() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_row(player);
}

int AudioStreamPlaybackTaud::get_bpm() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_bpm(player);
}

int AudioStreamPlaybackTaud::get_speed() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_speed(player);
}

int AudioStreamPlaybackTaud::get_channel_count() const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_channel_count(player);
}

/* ── the knob ── */

void AudioStreamPlaybackTaud::set_voice_gain(int p_voice, double p_gain, double p_fade_seconds)
{
	ERR_FAIL_INDEX(p_voice, TAUD_MAX_VOICES);
	std::lock_guard<std::mutex> g(lock);
	taud_set_voice_gain(player, p_voice, p_gain, p_fade_seconds);
}

double AudioStreamPlaybackTaud::get_voice_gain(int p_voice) const
{
	ERR_FAIL_INDEX_V(p_voice, TAUD_MAX_VOICES, 1.0);
	std::lock_guard<std::mutex> g(lock);
	return taud_get_voice_gain(player, p_voice);
}

/* ── the probes ── */

double AudioStreamPlaybackTaud::get_voice_volume(int p_voice) const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_get_voice_volume(player, p_voice);
}

double AudioStreamPlaybackTaud::get_voice_pan(int p_voice) const
{
	std::lock_guard<std::mutex> g(lock);
	return taud_get_voice_pan(player, p_voice);
}

/* ── interrupts ── */

void AudioStreamPlaybackTaud::set_interrupt(int p_number, const Callable &p_handler)
{
	ERR_FAIL_INDEX(p_number, TAUD_NUM_INTERRUPTS);
	handlers[p_number] = p_handler;
}

void AudioStreamPlaybackTaud::clear_interrupts()
{
	for (int n = 0; n < TAUD_NUM_INTERRUPTS; n++) handlers[n] = Callable();
}

void AudioStreamPlaybackTaud::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("select_song", "index"), &AudioStreamPlaybackTaud::select_song);
	ClassDB::bind_method(D_METHOD("get_current_song"), &AudioStreamPlaybackTaud::get_current_song);
	ClassDB::bind_method(D_METHOD("seek_cue", "cue"), &AudioStreamPlaybackTaud::seek_cue);
	ClassDB::bind_method(D_METHOD("set_binaural", "enable"), &AudioStreamPlaybackTaud::set_binaural);
	ClassDB::bind_method(D_METHOD("get_cue"), &AudioStreamPlaybackTaud::get_cue);
	ClassDB::bind_method(D_METHOD("get_row"), &AudioStreamPlaybackTaud::get_row);
	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamPlaybackTaud::get_bpm);
	ClassDB::bind_method(D_METHOD("get_speed"), &AudioStreamPlaybackTaud::get_speed);
	ClassDB::bind_method(D_METHOD("get_channel_count"), &AudioStreamPlaybackTaud::get_channel_count);

	ClassDB::bind_method(D_METHOD("set_voice_gain", "voice", "gain", "fade_seconds"), &AudioStreamPlaybackTaud::set_voice_gain, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("get_voice_gain", "voice"), &AudioStreamPlaybackTaud::get_voice_gain);
	ClassDB::bind_method(D_METHOD("get_voice_volume", "voice"), &AudioStreamPlaybackTaud::get_voice_volume);
	ClassDB::bind_method(D_METHOD("get_voice_pan", "voice"), &AudioStreamPlaybackTaud::get_voice_pan);

	ClassDB::bind_method(D_METHOD("set_interrupt", "number", "handler"), &AudioStreamPlaybackTaud::set_interrupt);
	ClassDB::bind_method(D_METHOD("clear_interrupts"), &AudioStreamPlaybackTaud::clear_interrupts);

	ADD_SIGNAL(MethodInfo("interrupt", PropertyInfo(Variant::INT, "number"), PropertyInfo(Variant::INT, "argument")));
}

} // namespace godot
