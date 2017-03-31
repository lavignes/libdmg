#include <stdio.h>
#include <pthread.h>
#include <SDL2/SDL.h>

#include <dmg/dmg.h>

static SDL_Renderer *renderer;
static SDL_Texture *texture;

void vblank(DMGState *state) {
    void *pixels;
    int pitch;
    // TODO: Why not pass SDL texture directly to the renderer during an hblank?
    // TODO: instead of copying it every vbblank
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, state->ppu.lcd, sizeof(uint32_t) * 23040);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int debugger(void *userdata) {
    bool exit = false;
    do {
        printf("dmg> ");
        fflush(stdout);
        char *line = NULL;
        size_t len = 0;
        ssize_t read = getdelim(&line, &len, '\n', stdin);
        if (read < 0) {
            break;
        }
        if (read > 1 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        if (strcmp(line, "quit") == 0) {
            exit = true;
        }
        if (line) {
            free(line);
        }
    } while (!exit);
    return 0;
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("DOT MATRIX GAME", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * 4, 144 * 4, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    FILE *file = fopen("drmario.gb", "rb");
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *rom = malloc(size);
    fread(rom, 1, size, file);
    fclose(file);

    DMGState *state = calloc(1, sizeof(DMGState));
    state->cpu.ime = true;
    state->rom = rom;

    SDL_Thread *debugger_thread = SDL_CreateThread(debugger, "Debugger", NULL);

    while (true) {
        SDL_Event event;
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            break;
        }
        size_t cycles = state->cycles;
        dmg_cpu_run(state, 0);
        cycles = state->cycles - cycles;
        while(cycles--) {
            dmg_ppu_run(state, vblank, 0);
        }
    }

    free(state);
    free(rom);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_WaitThread(debugger_thread, NULL);
    SDL_Quit();

    return 0;
}