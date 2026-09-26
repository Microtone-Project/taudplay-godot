# Plays a song through a real AudioStreamPlayer and records what leaves the
# master bus, for tests/run.sh to compare against the taudplay CLI.
#
#   godot --headless --path tests/godot -- SONG.taud OUT.f32 SECONDS [SONG_INDEX] [loop]
#
# OUT.f32 gets the bus output (interleaved stereo float, little-endian);
# OUT.f32.log gets one line per interrupt heard, as "int N ARG FRAME", where
# FRAME is how many frames the bus had mixed when the signal arrived, and a
# "finished FRAME" line if the song ended.
extends Node

var capture := AudioEffectCapture.new()
var player := AudioStreamPlayer.new()
var pcm := PackedFloat32Array()
var captured := 0
var wanted := 0
var out_path := ""
var lines := PackedStringArray()


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var stream := AudioStreamTaud.load_from_file(args[0])
	out_path = args[1]
	wanted = int(float(args[2]) * AudioServer.get_mix_rate())
	if args.size() > 3:
		stream.song = int(args[3])
	if args.size() > 4:
		stream.loop = args[4] == "loop"

	capture.buffer_length = 2.0
	AudioServer.add_bus_effect(0, capture)
	player.stream = stream
	add_child(player)
	player.finished.connect(func(): lines.append("finished %d" % _mixed()))
	player.play()
	var pb: AudioStreamPlaybackTaud = player.get_stream_playback()
	pb.interrupt.connect(func(n: int, arg: int): lines.append("int %d %d %d" % [n, arg, _mixed()]))


func _mixed() -> int:
	return captured + capture.get_frames_available()


func _process(_delta: float) -> void:
	var n := capture.get_frames_available()
	if n > 0:
		for f in capture.get_buffer(n):
			pcm.append(f.x)
			pcm.append(f.y)
		captured += n
	if captured < wanted:
		return
	var f := FileAccess.open(out_path, FileAccess.WRITE)
	f.store_buffer(pcm.to_byte_array())
	f = FileAccess.open(out_path + ".log", FileAccess.WRITE)
	for line in lines:
		f.store_line(line)
	get_tree().quit()
