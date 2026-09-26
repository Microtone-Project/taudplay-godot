# taudplay-godot

**Play `.taud` songs in Godot 4. Drop the file in, press play.**

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html)
around [`taudplay-c`](https://github.com/Microtone-Project/taudplay-c), the
portable C replayer for songs made in the [Microtone](https://microtone.cc)
tracker. It is the same engine, note for note and **sample for sample**: what
leaves Godot's master bus is bit-identical to the C library's render, and so to
the JavaScript library's and the tracker's (see [Verification](#verification)).

A `.taud` file imports as an `AudioStreamTaud`, which plays in any
`AudioStreamPlayer`, `AudioStreamPlayer2D` or `AudioStreamPlayer3D`. The
playback it starts carries the taudplay surface, kept deliberately small:

| | |
|---|---|
| **Knob** | `set_voice_gain(v, gain, fade_seconds)` — one fader per lane |
| **Probe** | `get_voice_volume(v)` — how loud lane `v` is, 0…1 |
| **Probe** | `get_voice_pan(v)` — where it sits, 0 (left) … 0.5 … 1 (right) |

…plus the transport, and the sixteen **interrupts** a song can fire at the game
playing it (`Int0`…`IntF` in a note column, each with a 16-bit argument),
delivered on the main thread at the moment they are heard.

A tracker song is 32 or 64 lanes of music that were written together. Fading
them against each other gives you contextual scoring from a single file: duck
the lead when the player enters a cave, bring the drums up in combat.

## Install

Copy `demo/addons/taudplay` into your project's `addons/` folder (after
building, below) and restart the editor. `.taud` files then show up in the
FileSystem dock as audio streams. Their import options are `song` (which song
of the file), `loop` and `binaural`.

## Build

```
git submodule update --init
pip install scons
scons                                  # debug build for this machine
scons target=template_release          # the export build
scons platform=windows                 # …or any platform godot-cpp can target
```

The library lands in `demo/addons/taudplay/bin/`. Two things are pinned on
purpose:

- **godot-cpp is at `godot-4.2-stable`.** Godot refuses an extension built
  against a newer engine than itself, so building against 4.2.0 keeps
  everything from 4.2.0 onwards working. Move it forward only when you drop
  support for older engines.
- **taudplay-c is built with `-ffp-contract=off`** (`/fp:precise` on MSVC) and
  never with fast-math, as bit-exact output requires. The SConstruct handles
  this; keep it that way if you rework the build.

## Play something

Assign the imported song to an `AudioStreamPlayer`'s `stream` and play it. To
reach the knobs, take the playback:

```gdscript
@onready var music: AudioStreamPlayer = $Music
var song: AudioStreamPlaybackTaud

func _ready() -> void:
    music.play()
    song = music.get_stream_playback()
```

Each `play()` starts a fresh playback from the top of the song, so take it
again after every `play()`.

### Mix it

```gdscript
song.set_voice_gain(4, 0.0, 1.2)   # fade lane 5 out over 1.2 s
song.set_voice_gain(4, 1.0, 0.4)   # …and back in, faster

for v in song.get_channel_count():  # duck everything but the drums
    if v != DRUMS:
        song.set_voice_gain(v, 0.3, 0.8)
```

Fades run inside the renderer, once per 128-frame block, so a slow fade is
smooth without your game loop driving it. A fader reaches everything its lane
spawned, NNA ghosts and metainstrument layers included. Faders also carry over
`select_song()`, because the mix is the game's state, not the song's.

### Let the song call you

```gdscript
song.interrupt.connect(func(number: int, argument: int) -> void:
    if number == 0:
        flash_light(argument))

# …or one handler per interrupt, as in the C and JS libraries:
song.set_interrupt(1, func(argument: int) -> void: spawn_enemy_wave())
song.set_interrupt(1, Callable())  # unregister
song.clear_interrupts()
```

Interrupts are emitted on the main thread, once per frame. Each one waits
until the audio it fired in is actually reaching the speakers (frames mixed +
time since the last mix − output latency, the recipe from Godot's
[audio-sync guide](https://docs.godotengine.org/en/stable/tutorials/audio/sync_with_audio.html)),
so it lands on the beat you hear rather than one output buffer early. If one
interrupt fires twice inside a 128-frame block you get it once, with the later
argument.

### Watch it

```gdscript
func _process(_delta: float) -> void:
    for v in song.get_channel_count():
        draw_meter(v, song.get_voice_volume(v), song.get_voice_pan(v))
```

The probes describe the state at the end of the last rendered block.

### Load at run time

For mods, downloads, user music:

```gdscript
var stream := AudioStreamTaud.load_from_file("user://tracks/theme.taud")
# or AudioStreamTaud.load_from_buffer(bytes)
```

Either returns `null` (with an error printed) for a file that is not a playable
song.

## API

**`AudioStreamTaud`** (Resource, extends `AudioStream`)

- `load_from_file(path)`, `load_from_buffer(bytes)`: static, return a stream or `null`
- `song`, `loop`, `binaural`, `random_seed`: properties, read when playback starts
- `get_title()`, `get_song_count()`, `get_songs()`: the file's songs as dictionaries
  (`index`, `name`, `patterns`, `bpm`, `channels`, `surround`)

**`AudioStreamPlaybackTaud`** (from `get_stream_playback()`)

- **Transport:** `select_song(i)` (switches and plays from the top), `seek_cue(n)`,
  `set_binaural(on)`, `get_current_song()`, `get_cue()`, `get_row()`, `get_bpm()`,
  `get_speed()`, `get_channel_count()`
- **Knob / probes:** `set_voice_gain(v, gain, fade_seconds = 0)`, `get_voice_gain(v)`,
  `get_voice_volume(v)`, `get_voice_pan(v)`
- **Interrupts:** signal `interrupt(number, argument)`, `set_interrupt(n, callable)`,
  `clear_interrupts()`

Master volume is the player's `volume_db`, as with any stream.
`AudioStreamPlayer.get_playback_position()` is the time played since `play()`.

## How it fits into Godot

- **No resampling.** The engine renders natively at any rate, so each playback
  renders at `AudioServer.get_mix_rate()`. Godot's resampler then steps through
  it one frame at a time, which is an exact copy. `pitch_scale` still works
  (that path does resample).
- **Threads.** The audio thread renders and your scripts turn the knobs; each
  playback holds a short lock around the engine, so every method is safe to
  call from the main thread at any time.
- **No time-line.** A tracker song goes where its jumps take it, so there is no
  length (`get_length()` is 0) and no seeking by seconds: `play(from_position)`
  starts from the top and `seek()` does nothing. Use `seek_cue()`.
- **Endings.** A song that halts ends the playback (`finished` is emitted);
  with `loop` it starts again from the top instead. Most game music jumps back
  on its own and never halts.
- **Randomness.** Instrument swing and randomised effects draw from a seeded
  generator (`random_seed`, default 1, the reference's), so a song sounds the
  same every time. Change the seed for variety.
- **Cost.** A playback holds about 2 MB plus the song's sample data, and takes
  a few milliseconds to open when `play()` is called. Rendering runs at about
  50× real time on one desktop core for an ordinary 32-lane song.

## Verification

`tests/run.sh` plays songs through a real `AudioStreamPlayer` in a headless
Godot, captures the master bus with `AudioEffectCapture`, and compares it
against the `taudplay` CLI's render bit for bit. It also checks that every
interrupt the song fires is heard:

```
GODOT=/path/to/godot sh tests/run.sh song.taud other.taud -t 20
```

The comparison skips the first mix step (512 frames), where Godot fades every
new playback in. Everything after it is identical: at the time of writing,
that covers the Microtone corpus songs tried, songs from taudplay-c's
differential fuzzer (interrupts, 64 lanes, surround, a song that halts), and
both 48 and 44.1 kHz mixers.

## Licence

LGPL-3.0-or-later, see `COPYING.LESSER` (and `COPYING` for the GPL text it
builds on), the same as taudplay-c. You may ship the extension in a
proprietary game; changes *to the extension itself* must be shared.

The demo song, `16uba_baion.taud`, is one of Microtone's demo songs.

---

Songs are made with **Microtone**, a tracker for the notes a piano cannot
play, free and in your browser at [microtone.cc](https://microtone.cc).
