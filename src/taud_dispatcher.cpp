#include "taud_dispatcher.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace godot {

TaudDispatcher *TaudDispatcher::singleton = nullptr;

/* Frames handed to Godot that have not left the mixer yet: up to one block in
 * AudioStreamPlaybackResampled's buffer (INTERNAL_BUFFER_LEN), plus the tail
 * the AudioServer holds back from every mix step (LOOKAHEAD_BUFFER_SIZE).
 * Taking the whole of both, an interrupt errs late by < 3 ms, never early. */
static constexpr int64_t UNMIXED_FRAMES = 128 + 64;

TaudDispatcher::TaudDispatcher() { singleton = this; }

TaudDispatcher::~TaudDispatcher()
{
	watched.clear();
	if (singleton == this) singleton = nullptr;
}

void TaudDispatcher::_connect()
{
	if (connected) return;
	SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	if (tree == nullptr) return; // not yet, or not a SceneTree: try again at the next watch()
	tree->connect("process_frame", callable_mp(this, &TaudDispatcher::_on_process_frame));
	connected = true;
	latency_age = 0;
}

void TaudDispatcher::watch(const Ref<AudioStreamPlaybackTaud> &p_playback)
{
	watched.push_back(p_playback);
	_connect();
}

int64_t TaudDispatcher::heard_frame(const AudioStreamPlaybackTaud *p_playback) const
{
	int64_t mixed = (int64_t)p_playback->mixed_frames.load(std::memory_order_acquire);
	double since = AudioServer::get_singleton()->get_time_since_last_mix();
	int64_t heard = mixed - UNMIXED_FRAMES + (int64_t)((since - latency) * p_playback->rate);
	return heard < mixed ? heard : mixed;
}

void TaudDispatcher::_on_process_frame()
{
	if (latency_age-- <= 0) {
		latency = AudioServer::get_singleton()->get_output_latency();
		latency_age = 60;
	}
	/* By index: a handler may start another song, which appends to the list. */
	for (int i = 0; i < watched.size();) {
		Ref<AudioStreamPlaybackTaud> pb = watched[i];
		/* Two references: this copy and the list's. Nobody else can reach the
		 * playback any more, so nothing more will be rendered or heard. */
		bool orphan = pb->get_reference_count() <= 2;
		pb->_dispatch(orphan);
		if (orphan) {
			watched.remove_at(i);
		} else {
			i++;
		}
	}
}

} // namespace godot
