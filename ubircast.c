/* vim: set et sts=4 sw=4: */

#include <math.h>
#include <stdint.h>
#include <SDL.h>
#include <time.h>

#define WIN_W 640
#define WIN_H 480
#define BPP 32
#define RAD(a) ((a) * (M_PI / 180.f))
#define FOV RAD(60)

typedef uint8_t u8;

#include "wall.c"

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
    .w = 100,
    .h = 100,
};

#define OUTBOUNDED(x, y) (x >= map.w || x < 0 || y >= map.h || y < 0)
#define MAP(x, y)        map.data[(int)(y) * map.w + (int)(x)]

static void map_dig(int x, int y)
{
    u8 tested = 0;

    MAP(x, y) = 0;
    while (tested != (1<<0 | 1<<1 | 1<<2 | 1<<3)) {
        int xinc = 0, yinc = 0, direction;

        do {
            direction = rand() % 4;
            switch (direction) {
            case 0: yinc =  1; break;
            case 1: xinc =  1; break;
            case 2: yinc = -1; break;
            case 3: xinc = -1; break;
            }
        } while (tested & (1<<direction));

        tested |= 1<<direction;

        if (OUTBOUNDED(x + xinc  , y + yinc  ) || !MAP(x + xinc  , y + yinc  ) ||
            OUTBOUNDED(x + xinc*2, y + yinc*2) || !MAP(x + xinc*2, y + yinc*2))
            continue;
        map_dig(x + xinc, y + yinc);
    }
}

static void init_maze(void)
{
    size_t sz = map.h * map.w * sizeof(*map.data);
    map.data = malloc(sz);
    memset(map.data, '#', sz);
    srand(time(NULL));
    map_dig((int)pl.x, (int)pl.y);
}

static float sub_angle(float a, float v)
{
    a -= v;
    return a < 0 ? a + 2 * M_PI : a;
}

static float add_angle(float a, float v)
{
    a += v;
    return a > 2 * M_PI ? a - 2 * M_PI : a;
}

static void mv_pl(float x, float y)
{
    x = pl.x + x;
    y = pl.y + y;
    if (!MAP(x, pl.y))
        pl.x = x;
    if (!MAP(pl.x, y))
        pl.y = y;
}

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
        case SDLK_LEFT:     pl.angle = add_angle(pl.angle, .1f);                break;
        case SDLK_RIGHT:    pl.angle = sub_angle(pl.angle, .1f);                break;
        case SDLK_w:
        case SDLK_UP:       mv_pl( .2f * cos(pl.angle), .2f * sin(pl.angle));   break;
        case SDLK_s:
        case SDLK_DOWN:     mv_pl(-.2f * cos(pl.angle),-.2f * sin(pl.angle));   break;
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
    for (int y = 0; y < WIN_H / 2; y++) {
        u8 c[4] = {y*0xff/WIN_H, y*0x7f/WIN_H, y*0x7f/WIN_H, 0};
        for (int x = 0; x < WIN_W; x++)
            memcpy(&sky[(y * WIN_W + x) * (BPP / 8)], c, 4);
    }
}

struct coords { float x, y; };

struct inter {
    struct coords pos;
    float dist;
    int wall_h;
    int xy;
};

static int draw_wall(u8 *data, struct inter i)
{
    int j, y;
    int wall_h = i.wall_h;

    float yy = 0.f, yinc = 1.f / wall_h;
    if (wall_h > WIN_H) {
        yy = yinc * (wall_h - WIN_H) / 2.f;
        wall_h = WIN_H;
    }
    y = WIN_H / 2 - wall_h / 2;
    data += y * WIN_W * (BPP / 8);
    float xx = i.xy < 0 ? i.pos.y - (int)i.pos.y : i.pos.x - (int)i.pos.x;
    for (j = 0; j < wall_h; j++, yy += yinc) {
        int xi = xx * WALL_TEX_WIDTH;
        int yi = yy * WALL_TEX_HEIGHT;
        u8 *c = &WALL_TEX_pixel_data[(yi * WALL_TEX_WIDTH + xi) * (BPP / 8)];
        memcpy(data, c, 4);
        data += WIN_W * (BPP / 8);
    }
    return y + j;
}

static void draw_floor(u8 *data, int y, int wall_h)
{
    data += y * WIN_W * (BPP / 8);
    int i = 0;
    while (y++ < WIN_H) {
        u8 tint;
        if (wall_h > 0) {
            tint = 0x10;
        } else {
            int v = i++ * .1f;
            tint = v > 0x10 ? 0 : 0x10 - v;
        }
        wall_h--;
        u8 c[4] = {tint, tint, tint, 0xff};
        memcpy(data, c, 4);
        data += WIN_W * (BPP / 8);
    }
}

#define HIT_WALL(i, xb, yb) (OUTBOUNDED(i.x, i.y) || MAP(xb, yb))
#define DIST(p1, p2)        ((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y))

static struct inter get_wall_inter(float angle)
{
    float slope = tan(angle);
    int xinc = angle > M_PI/2.f && angle < 3.f*M_PI/2.f ? -1 : 1;
    int yinc = angle > M_PI                             ? -1 : 1;
    struct coords ix = {.x = (int)pl.x + (xinc > 0)};
    struct coords iy = {.y = (int)pl.y + (yinc > 0)};

    // inter with X constant
    for (;;) {
        ix.y = (ix.x - pl.x) * slope + pl.y;
        if (HIT_WALL(ix, ix.x - (xinc < 0), ix.y))
            break;
        ix.x += xinc;
    }

    // inter with Y constant
    for (;;) {
        iy.x = (iy.y - pl.y) / slope + pl.x;
        if (HIT_WALL(iy, iy.x, iy.y - (yinc < 0)))
            break;
        iy.y += yinc;
    }

    float dist2_p1 = DIST(pl, ix);
    float dist2_p2 = DIST(pl, iy);
    struct inter i;

    if (dist2_p1 < dist2_p2) {
        i.pos  = ix;
        i.dist = dist2_p1;
        i.xy   = -1;
    } else {
        i.pos  = iy;
        i.dist = dist2_p2;
        i.xy   = 1;
    }
    i.dist   = sqrtf(i.dist) * cos(angle - pl.angle);
    i.wall_h = WIN_H / i.dist;
    return i;
}

static void update_frame(u8 *data)
{
    memcpy(data, sky, sizeof(sky));
    float angle = add_angle(pl.angle, FOV / 2.f);
    for (int x = 0; x < WIN_W; x++) {
        struct inter i = get_wall_inter(angle);
        int y = draw_wall(data, i);

        draw_floor(data, y, i.wall_h);
        angle = sub_angle(angle, FOV / WIN_W);
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

    init_maze();
    init_sky();

    do {
        SDL_LockSurface(s);
        update_frame((u8*)s->pixels);
        SDL_UnlockSurface(s);
        SDL_Flip(s);

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
