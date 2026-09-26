# taudplay demo: a meter per lane (height = how loud, dot = where it sits),
# click a lane to fade it out or back in, and the song's interrupts as they
# are heard.
extends Control

@onready var music: AudioStreamPlayer = $Music
var playback: AudioStreamPlaybackTaud
var log_lines: PackedStringArray = []


func _ready() -> void:
	music.play()
	playback = music.get_stream_playback()
	playback.interrupt.connect(_on_interrupt)


func _on_interrupt(number: int, argument: int) -> void:
	log_lines.append("Int%X  arg %d  (cue %d row %d)" % [number, argument, playback.get_cue(), playback.get_row()])
	if log_lines.size() > 8:
		log_lines.remove_at(0)


func _process(_delta: float) -> void:
	queue_redraw()


# The lanes the song writes to (the engine always has 32 or 64).
func _lane_count() -> int:
	var songs := (music.stream as AudioStreamTaud).get_songs()
	return songs[playback.get_current_song()]["channels"]


func _lane_rect(v: int, lanes: int) -> Rect2:
	var area := Rect2(24, 96, size.x - 48, size.y - 240)
	var w := area.size.x / lanes
	return Rect2(area.position.x + v * w + 2, area.position.y, w - 4, area.size.y)


func _gui_input(event: InputEvent) -> void:
	if playback == null or not (event is InputEventMouseButton and event.pressed):
		return
	var lanes := _lane_count()
	for v in lanes:
		if _lane_rect(v, lanes).has_point(event.position):
			var gain := 0.0 if playback.get_voice_gain(v) > 0.5 else 1.0
			playback.set_voice_gain(v, gain, 0.5)


func _draw() -> void:
	var font := get_theme_default_font()
	var stream := music.stream as AudioStreamTaud
	draw_string(font, Vector2(24, 36), "%s — song %d of %d" % [stream.get_title(), stream.song + 1, stream.get_song_count()], HORIZONTAL_ALIGNMENT_LEFT, -1, 22)
	if playback == null or not music.playing:
		draw_string(font, Vector2(24, 66), "(stopped — press Space to play again)", HORIZONTAL_ALIGNMENT_LEFT, -1, 16)
		return
	draw_string(font, Vector2(24, 66), "cue %d  row %d  %d bpm  speed %d  —  click a lane to fade it" % [playback.get_cue(), playback.get_row(), playback.get_bpm(), playback.get_speed()], HORIZONTAL_ALIGNMENT_LEFT, -1, 16)

	var lanes := _lane_count()
	for v in lanes:
		var r := _lane_rect(v, lanes)
		var gain := playback.get_voice_gain(v)
		var vol := playback.get_voice_volume(v)
		var pan := playback.get_voice_pan(v)
		draw_rect(r, Color(1, 1, 1, 0.06))
		var h := r.size.y * vol
		draw_rect(Rect2(r.position.x, r.end.y - h, r.size.x, h), Color(1.0, 0.55, 0.8).lerp(Color(0.35, 0.35, 0.4), 1.0 - gain))
		draw_circle(Vector2(r.position.x + r.size.x * pan, r.end.y + 10), 4, Color.WHITE)
		draw_string(font, Vector2(r.position.x, r.end.y + 30), str(v + 1), HORIZONTAL_ALIGNMENT_CENTER, r.size.x, 12)

	var y := size.y - 100
	for line in log_lines:
		draw_string(font, Vector2(24, y), line, HORIZONTAL_ALIGNMENT_LEFT, -1, 14)
		y += 12


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_accept"):
		music.play()
		playback = music.get_stream_playback()
		playback.interrupt.connect(_on_interrupt)
