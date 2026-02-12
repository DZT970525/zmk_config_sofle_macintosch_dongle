typedef struct {
	int note;     /* hz */
	int duration; /* msec */
} Sound;

void stop_pwm(void);
void play_song(Sound sounds[], int notes_count);
void play_sound_with_vibrato(Sound sound, float vibrato_depth_hz, float vibrato_rate_hz);
void play_tremolo(int freq, int duration_ms, float rate_hz);
void play_trill(int note1, int note2, int duration_ms, int rate_hz);
void play_glissando(int start_note, int end_note, int duration_ms);
void play_bitcrushed_ramp(int start_note, int end_note, int duration_ms);
void play_slide_exponential(float start_freq, float end_freq, uint32_t duration_ms);
void play_slide(int start_freq, int end_freq, int duration_ms);
void play_slide_with_vibrato(int start_freq, int end_freq, int duration_ms, float vibrato_depth_hz, float vibrato_rate_hz);
void play_dual_note_simulated(int freq1, int freq2, uint32_t duration_ms);

#define PI 3.14159265

// Intervals
#define SIXTY_FOURTH 4
#define THIRTY_SECOND 19
#define SIXTEENTH 38
#define EIGTH	  75
#define QUARTER	  150
#define HALF	  300
#define WHOLE	  600
#define DOTTED_EIGTH (SIXTEENTH + EIGTH)

#define REST 1

// Octave 1
#define C1   33
#define Db1  35
#define D1   37
#define Eb1  39
#define E1   41
#define F1   44
#define Gb1  46
#define G1   49
#define Ab1  52
#define A1   55
#define Bb1  58
#define B1   62

// Octave 2
#define C2   65
#define Db2  69
#define D2   73
#define Eb2  78
#define E2   82
#define F2   87
#define Gb2  93
#define G2   98
#define Ab2  104
#define A2   110
#define Bb2  117
#define B2   123

// Octave 3
#define C3   131
#define Db3  139
#define D3   147
#define Eb3  156
#define E3   165
#define F3   175
#define Gb3  185
#define G3   196
#define Ab3  208
#define A3   220
#define Bb3  233
#define B3   247

// Octave 4 (middle C = C4)
#define C4  262
#define Db4 277
#define D4  294
#define Eb4 311
#define E4  330
#define F4  349
#define Gb4 370
#define G4  392
#define Ab4 415
#define A4  440
#define Bb4 466
#define B4  494

// Octave 5
#define C5  523
#define Db5 554
#define D5  587
#define Eb5 622
#define E5  659
#define F5  698
#define Gb5 740
#define G5  784
#define Ab5 831
#define A5  880
#define Bb5 932
#define B5  988

// Octave 6
#define C6  1046
#define Db6 1109
#define D6  1175
#define Eb6 1245
#define E6  1319
#define F6  1397
#define Gb6 1480
#define G6  1568
#define Ab6 1661
#define A6  1760
#define Bb6 1865
#define B6  1976

// Octave 7
#define C7   2093
#define Db7  2217
#define D7   2349
#define Eb7  2489
#define E7   2637
#define F7   2794
#define Gb7  2960
#define G7   3136
#define Ab7  3322
#define A7   3520
#define Bb7  3729
#define B7   3951

// Octave 8
#define C8   4186
#define Db8  4435
#define D8   4699
#define Eb8  4978
#define E8   5274
#define F8   5588
#define Gb8  5920
#define G8   6272
#define Ab8  6645
#define A8   7040
#define Bb8  7459
#define B8   7902