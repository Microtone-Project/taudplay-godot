/* Delivers interrupts on the main thread, at the moment their audio is heard.
 *
 * The engine fires an interrupt while it renders, which is on the audio
 * thread and one output buffer or more ahead of the speakers. Each playback
 * queues its interrupts with the stream frame they fired at; once a frame this
 * object walks every live playback and releases the ones whose frame has
 * reached the ear (Godot's own recipe: mixed so far + time since the last mix
 * − output latency).
 *
 * It also keeps a reference to every playback until nobody else holds one, so
 * a playback is always freed here, on the main thread, and never while this
 * loop is about to touch it.
 *
 * Copyright (C) 2026 CuriousTorvald. Licensed under the GNU Lesser General
 * Public License version 3 or later — see COPYING.LESSER.
 */
#ifndef TAUD_DISPATCHER_H
#define TAUD_DISPATCHER_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/vector.hpp>

#include "audio_stream_taud.h"

namespace godot {

class TaudDispatcher : public Object {
	GDCLASS(TaudDispatcher, Object)

	static TaudDispatcher *singleton;

	Vector<Ref<AudioStreamPlaybackTaud>> watched;
	bool connected = false;
	double latency = 0;    // AudioServer.get_output_latency() is costly; cached
	int latency_age = 0;

	void _connect();
	void _on_process_frame();

protected:
	static void _bind_methods() {}

public:
	static TaudDispatcher *get_singleton() { return singleton; }

	void watch(const Ref<AudioStreamPlaybackTaud> &p_playback);
	/* The stream frame being heard now, for a playback rendering at `rate`. */
	int64_t heard_frame(const AudioStreamPlaybackTaud *p_playback) const;

	TaudDispatcher();
	~TaudDispatcher();
};

} // namespace godot

#endif // TAUD_DISPATCHER_H
