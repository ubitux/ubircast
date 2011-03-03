#include <math.h>
#include <stdint.h>
#include <SDL.h>

#define WIN_W 640
#define WIN_H 480
#define BPP 32
#define RAD(a) ((a) * (M_PI / 180.f))
#define FOV RAD(60)

typedef uint8_t u8;

static struct {
    float x, y, angle;
} pl = {
    .x     = 1.5,
    .y     = 1.5,
    .angle = RAD(0)
};

static struct {
    int w, h;
    u8 *data;
} map = {
    .w     = 15,
    .h     = 10,
    .data  = (u8 *)
             "###############"
             "#             #"
             "#   #    ######"
             "#####      #  #"
             "#      #      #"
             "#    ###   #  #"
             "#          #  #"
             "#####         #"
             "#        #    #"
             "###############",
};

#define MAP(x, y) map.data[(int)(y) * map.w + (int)(x)]

#define MV_PL(X, Y) do { \
    float x = pl.x + (X); \
    float y = pl.y + (Y); \
    if (MAP(x, y) == ' ') { \
        pl.x = x; \
        pl.y = y; \
    } \
} while (0);

#define SUB_ANGLE(a, v) do { \
    a -= v; \
    if (a < 0) \
        a += 2 * M_PI; \
} while (0)

#define ADD_ANGLE(a, v) do { \
    a += v; \
    if (a > 2 * M_PI) \
        a -= 2 * M_PI; \
} while (0)

static int handle_events(void)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN
        && event.key.keysym.sym == SDLK_ESCAPE))
        return -1;
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_LEFT:     ADD_ANGLE(pl.angle, .1f);                           break;
        case SDLK_RIGHT:    SUB_ANGLE(pl.angle, .1f);                           break;
        case SDLK_w:
        case SDLK_UP:       MV_PL( .2f * cos(pl.angle), .2f * sin(pl.angle));   break;
        case SDLK_s:
        case SDLK_DOWN:     MV_PL(-.2f * cos(pl.angle),-.2f * sin(pl.angle));   break;
        case SDLK_a:                                                            break;
        case SDLK_d:                                                            break;
        default:                                                                break;
        }
    }
    return 0;
}

static u8 sky[WIN_H / 2 * WIN_W * (BPP / 8)];

static void init_sky(void)
{
    for (size_t i = 0; i < sizeof(sky); i += 4)
        memcpy(&sky[i], "\xAA\x30\x30\xff", 4);
}

static int draw_wall(u8 *data, int wall_h)
{
    int j, y;
    u8 *c = (u8*)"\x20\x10\x20\xff";

    if (wall_h < 0) {
        wall_h = -wall_h;
        c = (u8*)"\x50\x50\x50\xff";
    }

    if (wall_h > WIN_H)
        wall_h = WIN_H;
    y = WIN_H / 2 - wall_h / 2;
    data += y * WIN_W * (BPP / 8);
    for (j = 0; j < wall_h; j++) {
        memcpy(data, c, 4);
        data += WIN_W * (BPP / 8);
    }
    return y + j;
}

static void draw_floor(u8 *data, int y)
{
    data += y * WIN_W * (BPP / 8);
    while (y++ < WIN_H) {
        memcpy(data, "\x50\x80\x10\xff", 4);
        data += WIN_W * (BPP / 8);
    }
}

struct coords { float x, y; };

#define MAP_FREE(i, xb, yb) (i.x >= map.w || i.x < 0 || i.y >= map.h || i.y < 0 || MAP(xb, yb) != ' ')
#define DIST(p1, p2)        ((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y))

static float get_dist_wall(float angle)
{
    float slope = tan(angle);
    int xinc = angle > M_PI/2.f && angle < 3.f*M_PI/2.f ? -1 : 1;
    int yinc = angle > M_PI                             ? -1 : 1;
    struct coords ix = {.x = (int)pl.x + (xinc > 0)};
    struct coords iy = {.y = (int)pl.y + (yinc > 0)};

    // inter with X constant
    for (;;) {
        ix.y = (ix.x - pl.x) * slope + pl.y;
        if (MAP_FREE(ix, ix.x - (xinc < 0), ix.y))
            break;
        ix.x += xinc;
    }

    // inter with Y constant
    for (;;) {
        iy.x = (iy.y - pl.y) / slope + pl.x;
        if (MAP_FREE(iy, iy.x, iy.y - (yinc < 0)))
            break;
        iy.y += yinc;
    }

    float dist_p1 = DIST(pl, ix);
    float dist_p2 = DIST(pl, iy);
    return sqrt(dist_p1 < dist_p2 ? dist_p1 : dist_p2) // shorter dist
         * cos(angle - pl.angle)                       // distorsion fix
         * (dist_p1 < dist_p2 ? -1 : 1);               // difference between X and Y hit
}

static void update_frame(u8 *data)
{
    int x;
    float angle = pl.angle;

    memcpy(data, sky, sizeof(sky));
    ADD_ANGLE(angle, FOV / 2);
    for (x = 0; x < WIN_W; x++) {
        int wall_h = (int)(WIN_H / get_dist_wall(angle)); // + .5f?
        int y = draw_wall(data, wall_h);

        draw_floor(data, y);
        SUB_ANGLE(angle, FOV / WIN_W);
        data += BPP / 8;
    }
}

int main(void)
{
    SDL_Surface *s;
    int ticks, prev_ticks = 0, frames = 0;

    SDL_Init(SDL_INIT_VIDEO);
    s = SDL_SetVideoMode(WIN_W, WIN_H, BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (!s) {
        fprintf(stderr, "Unable to init video\n");
        return 1;
    }

    SDL_EnableKeyRepeat(1, SDL_DEFAULT_REPEAT_INTERVAL);

    init_sky();

    do {
        SDL_LockSurface(s);
        update_frame((u8*)s->pixels);
        SDL_UnlockSurface(s);
        SDL_Flip(s);
        SDL_Delay(1);

        frames++;
        ticks = SDL_GetTicks();

        if (ticks - prev_ticks > 1000) {
            printf("%d FPS\n", (int)(frames / (ticks / 1000.f)));
            prev_ticks = ticks;
        }
    } while (handle_events() != -1);

    SDL_FreeSurface(s);
    SDL_Quit();
    return 0;
}
