/*
 *	mixer.c
 *	written by Holmes Futrell
 *	use however you want
 */

#import "SDL.h"
#import "common.h"

#define NUM_CHANNELS 8          /* max number of sounds we can play at once */
#define NUM_DRUMS 4             /* number of drums in our set */
#define MILLESECONDS_PER_FRAME 16       /* about 60 frames per second */

static struct
{
    SDL_Rect rect;              /* where the button is drawn */
    SDL_Color upColor;          /* color when button is not active */
    SDL_Color downColor;        /* color when button is active */
    int isPressed;              /* is the button being pressed ? */
    int touchIndex;             /* what mouse (touch) index pressed the button ? */
} buttons[NUM_DRUMS];

struct sound
{
    Uint8 *buffer;              /* audio buffer for sound file */
    Uint32 length;              /* length of the buffer (in bytes) */
};

/* this array holds the audio for the drum noises */
static struct sound drums[NUM_DRUMS];

/* function declarations */
void handleMouseButtonDown(SDL_Event * event);
void handleMouseButtonUp(SDL_Event * event);
int playSound(struct sound *);
void render(void);
void initializeButtons();
void audioCallback(void *userdata, Uint8 * stream, int len);
void loadSound(const char *file, struct sound *s);

struct
{
    /* channel array holds information about currently playing sounds */
    struct
    {
        Uint8 *position;        /* what is the current position in the buffer of this sound ? */
        Uint32 remaining;       /* how many bytes remaining before we're done playing the sound ? */
        Uint32 timestamp;       /* when did this sound start playing ? */
    } channels[NUM_CHANNELS];
    SDL_AudioSpec outputSpec;   /* what audio format are we using for output? */
    int numSoundsPlaying;       /* how many sounds are currently playing */
} mixer;

/* sets up the buttons (color, position, state) */
void
initializeButtons()
{

    int i;
    int spacing = 10;           /* gap between drum buttons */
    SDL_Rect buttonRect;        /* keeps track of where to position drum */
    SDL_Color upColor = { 86, 86, 140, 255 };   /* color of drum when not pressed */
    SDL_Color downColor = { 191, 191, 221, 255 };       /* color of drum when pressed */

    buttonRect.x = spacing;
    buttonRect.y = spacing;
    buttonRect.w = SCREEN_WIDTH - 2 * spacing;
    buttonRect.h = (SCREEN_HEIGHT - (NUM_DRUMS + 1) * spacing) / NUM_DRUMS;

    /* setup each button */
    for (i = 0; i < NUM_DRUMS; i++) {

        buttons[i].rect = buttonRect;
        buttons[i].isPressed = 0;
        buttons[i].upColor = upColor;
        buttons[i].downColor = downColor;

        buttonRect.y += spacing + buttonRect.h; /* setup y coordinate for next drum */

    }
}

/*
 loads a wav file (stored in 'file'), converts it to the mixer's output format,
 and stores the resulting buffer and length in the sound structure
 */
void
loadSound(const char *file, struct sound *s)
{
    SDL_AudioSpec spec;         /* the audio format of the .wav file */
    SDL_AudioCVT cvt;           /* used to convert .wav to output format when formats differ */
    int result;
    if (SDL_LoadWAV(file, &spec, &s->buffer, &s->length) == NULL) {
        fatalError("could not load .wav");
    }
    /* build the audio converter */
    result = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                               mixer.outputSpec.format,
                               mixer.outputSpec.channels,
                               mixer.outputSpec.freq);
    if (result == -1) {
        fatalError("could not build audio CVT");
    } else if (result != 0) {
        /* 
           this happens when the .wav format differs from the output format.
           we convert the .wav buffer here
         */
        cvt.buf = (Uint8 *) SDL_malloc(s->length * cvt.len_mult);       /* allocate conversion buffer */
        cvt.len = s->length;    /* set conversion buffer length */
        SDL_memcpy(cvt.buf, s->buffer, s->length);      /* copy sound to conversion buffer */
        if (SDL_ConvertAudio(&cvt) == -1) {     /* convert the sound */
            fatalError("could not convert .wav");
        }
        SDL_free(s->buffer);    /* free the original (unconverted) buffer */
        s->buffer = cvt.buf;    /* point sound buffer to converted buffer */
        s->length = cvt.len_cvt;        /* set sound buffer's new length */
    }
}

/* called from main event loop */
void
handleMouseButtonDown(SDL_Event * event)
{

    int x, y, mouseIndex, i, drumIndex;

    mouseIndex = event->button.which;
    drumIndex = -1;

    SDL_SelectMouse(mouseIndex);
    SDL_GetMouseState(&x, &y);
    /* check if we hit any of the drum buttons */
    for (i = 0; i < NUM_DRUMS; i++) {
        if (x >= buttons[i].rect.x
            && x < buttons[i].rect.x + buttons[i].rect.w
            && y >= buttons[i].rect.y
            && y < buttons[i].rect.y + buttons[i].rect.h) {
            drumIndex = i;
            break;
        }
    }
    if (drumIndex != -1) {
        /* if we hit a button */
        buttons[drumIndex].touchIndex = mouseIndex;
        buttons[drumIndex].isPressed = 1;
        playSound(&drums[drumIndex]);
    }

}

/* called from main event loop */
void
handleMouseButtonUp(SDL_Event * event)
{
    int i;
    int mouseIndex = event->button.which;
    /* check if this should cause any of the buttons to become unpressed */
    for (i = 0; i < NUM_DRUMS; i++) {
        if (buttons[i].touchIndex == mouseIndex) {
            buttons[i].isPressed = 0;
        }
    }
}

/* draws buttons to screen */
void
render(void)
{
    int i;
    SDL_SetRenderDrawColor(50, 50, 50, 255);
    SDL_RenderFill(NULL);      /* draw background (gray) */
    /* draw the drum buttons */
    for (i = 0; i < NUM_DRUMS; i++) {
        SDL_Color color =
            buttons[i].isPressed ? buttons[i].downColor : buttons[i].upColor;
        SDL_SetRenderDrawColor(color.r, color.g, color.b, color.unused);
        SDL_RenderFill(&buttons[i].rect);
    }
    /* update the screen */
    SDL_RenderPresent();
}

/*
	finds a sound channel in the mixer for a sound
	and sets it up to start playing
*/
int
playSound(struct sound *s)
{
    /*
       find an empty channel to play on.
       if no channel is available, use oldest channel
     */
    int i;
    int selected_channel = -1;
    int oldest_channel = 0;

    if (mixer.numSoundsPlaying == 0) {
        /* we're playing a sound now, so start audio callback back up */
        SDL_PauseAudio(0);
    }

    /* find a sound channel to play the sound on */
    for (i = 0; i < NUM_CHANNELS; i++) {
        if (mixer.channels[i].position == NULL) {
            /* if no sound on this channel, select it */
            selected_channel = i;
            break;
        }
        /* if this channel's sound is older than the oldest so far, set it to oldest */
        if (mixer.channels[i].timestamp <
            mixer.channels[oldest_channel].timestamp)
            oldest_channel = i;
    }

    /* no empty channels, take the oldest one */
    if (selected_channel == -1)
        selected_channel = oldest_channel;
    else
        mixer.numSoundsPlaying++;

    /* point channel data to wav data */
    mixer.channels[selected_channel].position = s->buffer;
    mixer.channels[selected_channel].remaining = s->length;
    mixer.channels[selected_channel].timestamp = SDL_GetTicks();

    return selected_channel;
}

/* 
	Called from SDL's audio system.  Supplies sound input with data by mixing together all
	currently playing sound effects.
*/
void
audioCallback(void *userdata, Uint8 * stream, int len)
{
    int i;
    int copy_amt;
    SDL_memset(stream, mixer.outputSpec.silence, len);  /* initialize buffer to silence */
    /* for each channel, mix in whatever is playing on that channel */
    for (i = 0; i < NUM_CHANNELS; i++) {
        if (mixer.channels[i].position == NULL) {
            /* if no sound is playing on this channel */
            continue;           /* nothing to do for this channel */
        }

        /* copy len bytes to the buffer, unless we have fewer than len bytes remaining */
        copy_amt =
            mixer.channels[i].remaining <
            len ? mixer.channels[i].remaining : len;

        /* mix this sound effect with the output */
        SDL_MixAudioFormat(stream, mixer.channels[i].position,
                           mixer.outputSpec.format, copy_amt, 150);

        /* update buffer position in sound effect and the number of bytes left */
        mixer.channels[i].position += copy_amt;
        mixer.channels[i].remaining -= copy_amt;

        /* did we finish playing the sound effect ? */
        if (mixer.channels[i].remaining == 0) {
            mixer.channels[i].position = NULL;  /* indicates no sound playing on channel anymore */
            mixer.numSoundsPlaying--;
            if (mixer.numSoundsPlaying == 0) {
                /* if no sounds left playing, pause audio callback */
                SDL_PauseAudio(1);
            }
        }
    }
}

int
main(int argc, char *argv[])
{

    int done;                   /* has user tried to quit ? */
    SDL_WindowID windowID;      /* our main window */
    SDL_Event event;
    Uint32 startFrame;          /* holds when frame started processing */
    Uint32 endFrame;            /* holds when frame ended processing */
    Uint32 delay;               /* calculated delay, how long should we wait before next frame? */

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fatalError("could not initialize SDL");
    }
    windowID =
        SDL_CreateWindow(NULL, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
    SDL_CreateRenderer(windowID, 0, 0);

    /* initialize the mixer */
    SDL_memset(&mixer, 0, sizeof(mixer));
    /* setup output format */
    mixer.outputSpec.freq = 44100;
    mixer.outputSpec.format = AUDIO_S16LSB;
    mixer.outputSpec.channels = 2;
    mixer.outputSpec.samples = 256;
    mixer.outputSpec.callback = audioCallback;
    mixer.outputSpec.userdata = NULL;

    /* open audio for output */
    if (SDL_OpenAudio(&mixer.outputSpec, NULL) != 0) {
        fatalError("Opening audio failed");
    }

    /* load our drum noises */
    loadSound("ds_kick_big_amb.wav", &drums[3]);
    loadSound("ds_brush_snare.wav", &drums[2]);
    loadSound("ds_loose_skin_mute.wav", &drums[1]);
    loadSound("ds_china.wav", &drums[0]);

    /* setup positions, colors, and state of buttons */
    initializeButtons();

    /* enter main loop */
    done = 0;
    while (!done) {
        startFrame = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_MOUSEBUTTONDOWN:
                handleMouseButtonDown(&event);
                break;
            case SDL_MOUSEBUTTONUP:
                handleMouseButtonUp(&event);
                break;
            case SDL_QUIT:
                done = 1;
                break;
            }
        }
        render();               /* draw buttons */
        endFrame = SDL_GetTicks();

        /* figure out how much time we have left, and then sleep */
        delay = MILLESECONDS_PER_FRAME - (endFrame - startFrame);
        if (delay < 0) {
            delay = 0;
        } else if (delay > MILLESECONDS_PER_FRAME) {
            delay = MILLESECONDS_PER_FRAME;
        }
        SDL_Delay(delay);
    }

    /* cleanup code, let's free up those sound buffers */
    int i;
    for (i = 0; i < NUM_DRUMS; i++) {
        SDL_free(drums[i].buffer);
    }
    /* let SDL do its exit code */
    SDL_Quit();

    return 0;
}
