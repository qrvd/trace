#include <cstdlib>
#include <SDL/SDL.h>
#include <cmath>
#include <ctime>

int max(int x, int y) {
	return x > y ? x : y;
}

int min(int x, int y) {
	return x < y ? x : y;
}

struct Cell {
	struct {
		struct {
			bool on;
			Uint32 color;
		} left, right, up, down;
	} conn;
	bool filled;
	bool debug;
	Uint32 color;
};

struct Cursor {
	int x, y;
	Uint32 color;
};

bool exists(char *filename) {
	FILE *f = fopen(filename, "r");
	if (f) {
		fclose(f);
		return true;
	}
	// Presumably not.
	return false;
}

// Constants
const int GRID_W = 80;
const int GRID_H = 80;
const int NUM_CURSORS = GRID_H/2;

const int WINDOW_FLAGS = SDL_DOUBLEBUF|SDL_HWSURFACE|SDL_RESIZABLE;
const int WINDOW_DEFAULT_W = 640;
const int WINDOW_DEFAULT_H = 640;

// Variables
Uint16 TILE_W = WINDOW_DEFAULT_W / GRID_W;
Uint16 TILE_H = WINDOW_DEFAULT_H / GRID_H;
int FILL_W = TILE_W/2;
int FILL_H = TILE_H/2;
int NUM_COLORS = 4;

Cell grid[GRID_W * GRID_H];
Cursor cursors[NUM_CURSORS];
Uint32 colors[NUM_CURSORS]; //< Have at most NUM_CURSOR colors, but only use NUM_COLORS colors.

int bound(int x, int low, int high) {
	if (x < low) return low;
	if (x > high) return high;
	return x;
}

int wrap(int x, int low, int high) {
	if (x < low) return high;
	if (x > high) return low;
	return x;
}

bool filled(int tx, int ty) {
	return grid[tx + ty * GRID_W].filled;
}

bool emptied(int tx, int ty) {
	return not filled(tx, ty);
}

bool filledOrFalse(int tx, int ty) {
	if (tx < 0 || ty < 0 || tx >= GRID_W || ty >= GRID_H)
		return true;
	return filled(tx, ty);
}

bool validAndFilled(int tx, int ty) {
	if (tx < 0 || ty < 0 || tx >= GRID_W || ty >= GRID_H)
		return false;
	return filled(tx, ty);
}

bool surrounded(int tx, int ty) {
	return (
		filledOrFalse(tx-1, ty) && filledOrFalse(tx+1, ty)
		&& filledOrFalse(tx, ty-1) && filledOrFalse(tx, ty+1)
	);
}

void fillAt(int cx, int cy) {
	grid[cx+cy*GRID_W].filled = true;
}

bool move(int old_cx, int old_cy, int cx, int cy) {
	if (old_cx == cx && old_cy == cy) {
		return false;
	}
	Cell &c = grid[cx+cy*GRID_W];
	if (c.filled) {
		return false;
	}
	c.filled = true;
	Cell &oc = grid[old_cx+old_cy*GRID_W];
	if (cx < old_cx) {
		oc.conn.left.color = oc.color; 
		oc.conn.left.on = true;
	}
	else if (cx > old_cx) {
		oc.conn.right.color = oc.color; 
		oc.conn.right.on = true;
	}
	else if (cy < old_cy) {
		oc.conn.up.color = oc.color;
		oc.conn.up.on = true;
	}
	else if (cy > old_cy) {
		oc.conn.down.color = oc.color;
		oc.conn.down.on = true;
	}
	return true;
}

void fixConnections(int tx, int ty) {
	if (filledOrFalse(tx, ty)) {
		Cell &c = grid[tx + ty * GRID_W];
		if (c.conn.left.on && filledOrFalse(tx-1, ty)) c.conn.left.on = (grid[(tx-1)+ty*GRID_W].color == c.color);
		if (c.conn.right.on && filledOrFalse(tx+1, ty)) c.conn.right.on = (grid[(tx+1)+ty*GRID_W].color == c.color);
		if (c.conn.up.on && filledOrFalse(tx, ty-1)) c.conn.up.on = (grid[tx+(ty-1)*GRID_W].color == c.color);
		if (c.conn.down.on && filledOrFalse(tx, ty+1)) c.conn.down.on = (grid[tx+(ty+1)*GRID_W].color == c.color);
	}
}

void moveCursor(Cursor old, Cursor nu) {
	if (move(old.x, old.y, nu.x, nu.y)) {
		grid[nu.x + nu.y*GRID_W].color = old.color;
	}
}

void tick(bool retrace, bool weave, bool jumpToOwn, bool moveBoth, bool gravity) {
	for (int i = 0; i < NUM_CURSORS; i++) {
		Cursor &c = cursors[i];
		Cursor old = c;

		bool validMove = false;
		int attempt = 4;
		while (!validMove && attempt > 0) {

			int vel_x = 0;
			int vel_y = 0;
			if (moveBoth) {
				if (rand() % 2 == 0) {
					vel_x = rand() % 2 == 0 ? 1 : -1;
				}	
				if (rand() % 2 == 0) {
					vel_y = rand() % 2 == 0 ? 1 : -1;
				}
			} else {
				if (rand() % 2 == 0) {
					vel_x = (rand() % 2 == 0 ? 1 : -1);
				} else {
					vel_y = (rand() % 2 == 0 ? 1 : -1);
				}
			}
			if (gravity) {
				vel_y = 1;
			}

			c.x = bound(c.x + vel_x, 0, GRID_W-1);
			c.y = bound(c.y + vel_y, 0, GRID_H-1);

			validMove = emptied(c.x, c.y);
			bool retraced = false;
			if (not validMove && (c.x >= 0 && c.x < GRID_W && c.y >= 0 && c.y < GRID_H)) {
				Cell &t = grid[c.x+c.y*GRID_W];
				if (retrace) {
					// Go to our own color, as long as it was previously connected in that direction
					if (vel_x < 0) { retraced = true; validMove = t.conn.right.on && t.color == c.color;}
					if (vel_x > 0) { retraced = true; validMove = t.conn.left.on && t.color == c.color;}
					if (vel_y < 0) { retraced = true; validMove = t.conn.down.on && t.color == c.color;}
					if (vel_y > 0) { retraced = true; validMove = t.conn.up.on && t.color == c.color;}
				}
			}

			if (retraced) {
				attempt++;
			}

			if (validMove) {
				moveCursor(old, c);
			} else {
				attempt--;
				c = old;
				if (surrounded(c.x, c.y)) {
					if (jumpToOwn) {
						int x = rand() % GRID_W; int y = rand()%GRID_H;
						int attempt = GRID_W*GRID_H;
						while (attempt >= 0 && !(filled(x, y) && !surrounded(x,y) && grid[x+y*GRID_W].color==c.color)) {
							x = rand() % GRID_W, y = rand()%GRID_H;
							attempt--;
						}
						if (x == -1 && y == -1) {
							bool done = false;
							for (int y = 0; !done && y < GRID_H; y++) {
								for (int x = 0; !done && x < GRID_W; x++) {
									if (not filled(x, y)) {
										Cursor nc;
										nc.x = x;
										nc.y = y;
										nc.color = c.color;
										moveCursor(c, nc);
										c = nc;
										done = true;
									}
								}
							}
						} else {
							Cursor nc;
							nc.x = x;
							nc.y = y;
							nc.color = c.color;
							moveCursor(c, nc);
							c = nc;
						}
					} else {
						Cursor nc;
						nc.x = rand() % GRID_W;
						nc.y = rand() % GRID_H;
						nc.color = c.color;
						moveCursor(c, nc);
						c = nc;
					}
				}
			}
		}
	}

	for (int y = 0; y < GRID_H; y++)
		for (int x = 0; x < GRID_W; x++)
			fixConnections(x, y);
}

void drawRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {
	SDL_Rect dstrect = {x,y,w,h};
	SDL_FillRect(SDL_GetVideoSurface(),
		&dstrect, SDL_MapRGB(
			SDL_GetVideoSurface()->format, 255, 255, 255));
}

void drawRectColored(Sint16 x, Sint16 y, Uint16 w, Uint16 h, Uint16 color) {
	SDL_Rect dstrect = {x,y,w,h};
	SDL_FillRect(SDL_GetVideoSurface(), &dstrect, color);
}

void drawCursor(Cursor c) {
	SDL_Rect dstrect = {
		static_cast<Sint16>(c.x * TILE_W),
		static_cast<Sint16>(c.y*TILE_H), TILE_W, TILE_H};
	SDL_FillRect(SDL_GetVideoSurface(), &dstrect, c.color);
}

void fillCell(int tx, int ty) {
	drawRectColored(
		tx * TILE_W + (TILE_W-FILL_W)/2,
		ty * TILE_H + (TILE_H-FILL_H)/2,
		FILL_W, FILL_H, grid[tx+ty*GRID_W].color
	);
}

void connectX(int start, int end, int ty, Uint32 color) {
	drawRectColored(
		start * TILE_W + (TILE_W-FILL_W)/2,
		ty * TILE_H + (TILE_H-FILL_H)/2,
		FILL_W*2, FILL_H, grid[start+ty*GRID_W].color
	);
}

void connectY(int tx, int start, int end, Uint32 color) {
	drawRectColored(
		tx * TILE_W + (TILE_W-FILL_W)/2,
		start * TILE_H + (TILE_H-FILL_H)/2,
		FILL_W, FILL_H*2, grid[tx+start*GRID_W].color
	);
}

void drawCell(Cell c, int tx, int ty) {
	if (c.filled) {
		fillCell(tx, ty);
		if (c.conn.left.on) 	connectX(tx-1, tx, ty, c.conn.left.color);
		if (c.conn.right.on) connectX(tx, tx+1, ty, c.conn.right.color);
		if (c.conn.up.on) 		connectY(tx, ty-1, ty, c.conn.up.color);
		if (c.conn.down.on) 	connectY(tx, ty, ty+1, c.conn.down.color);
	}
}

void drawCellGapless(Cell c, int tx, int ty) {
	if (c.filled) {
		drawRectColored(
			tx * TILE_W + (TILE_W-FILL_W)/2,
			ty * TILE_H + (TILE_H-FILL_H)/2,
			TILE_W, TILE_H, grid[tx+ty*GRID_W].color
		);
	}
}

void resetMaze() {
	SDL_WM_SetCaption("trace", nullptr);
	SDL_Surface *wd = SDL_GetVideoSurface();

	// Create colors
	for (int i = 0; i < NUM_CURSORS; i++) {
		colors[i] = SDL_MapRGB(wd->format, rand() % 255, rand() % 255, rand() % 255);
	}

	// Create cursors
	for (int i = 0; i < NUM_CURSORS; i++) {
		Cursor &c = cursors[i];
		c.x = rand() % GRID_W;
		c.y = rand() % GRID_H;
		c.color = colors[rand() % NUM_COLORS];
		fillAt(c.x, c.y);
	}

	// Fill to white by default
	Cell newCell;
	newCell.debug = false;
	newCell.filled = false;
	newCell.color = SDL_MapRGB(wd->format, 255, 255, 255);
	newCell.conn.left.on = false;
	newCell.conn.left.color = 0;
	newCell.conn.right.on = false;
	newCell.conn.right.color = 0;
	newCell.conn.up.on = false;
	newCell.conn.up.color = 0;
	newCell.conn.down.on = false;
	newCell.conn.down.color = 0;
	for (int i = 0; i < GRID_W*GRID_H; i++) {
		grid[i] = newCell;
	}
}

bool completed() {
	for (int i = 0; i < GRID_W*GRID_H; i++)
		if (grid[i].filled == false)
			return false;
	return true;
}

void render(bool draw_cursors, bool debugging, bool gapless) {
	SDL_Surface *wd = SDL_GetVideoSurface();
	if (!wd) {
		printf("render: %s\n", SDL_GetError());
	}
	SDL_FillRect(wd, nullptr, 0);
	for (int y = 0; y < GRID_H; y++) {
		for (int x = 0; x < GRID_W; x++) {
			Cell &c = grid[x+y*GRID_W];
			if (c.debug && debugging) {
				drawRectColored(x * TILE_W, y * TILE_W, TILE_W, TILE_H,
					SDL_MapRGB(wd->format, 255, 255, 255));
			}
			if (gapless)
				drawCellGapless(grid[x + y * GRID_W], x, y);
			else
				drawCell(grid[x + y * GRID_W], x, y);
		}
	}
	if (draw_cursors) {
		for (Cursor &c: cursors) {
			drawCursor(c);
		}
	}
	if (SDL_Flip(wd) < 0) {
		printf("SDL_Flip: %s\n", SDL_GetError());
	}
}

void saveMaze(const char *dirname) {
	char filename[256] = { 0 };
	snprintf(filename, 256, "%s/%ld.bmp", dirname, time(nullptr));
	if (exists(filename)) {
		int append = 0;
		while (exists(filename) && append >= 0) {
			append++;
			memset(filename, 0, 256);
			snprintf(filename, 256, "%s/%ld-%d.bmp", dirname, time(nullptr), append);
		}
		if (append < 0) {
			printf("Can't come up with a non-existing filename.\n");
			return;
		}
	}
	SDL_SaveBMP(SDL_GetVideoSurface(), filename);
	printf("Screenshot saved to %s.\n", filename);
}

void onResize(int width, int height) {
	if (SDL_SetVideoMode(width, height, 0, WINDOW_FLAGS) < 0) {
		TILE_W = max(1, width / GRID_W);
		TILE_H = max(1, height / GRID_H);
		FILL_W = max(1, TILE_W / 2);
		FILL_H = max(1, TILE_H / 2);
		render(false, false, false);
	} else {
		printf("Resize error: %s\n", SDL_GetError());
	}
}

struct timer {

	unsigned long start_time;
	unsigned long interval;

	timer(unsigned long start, unsigned long interval)
		: start_time(start), interval(interval) {}

	unsigned long time(unsigned long now) {
		return now - start_time;
	}

	bool done(unsigned long now) {
		return this->time(now) >= interval;
	}

	void reset(unsigned long new_start) {
		start_time = new_start;
	}

};

struct options {
	enum {
		TRACING,
		PAUSED,
		DONE,
	} state;
	enum {
		JUMP_TO_RANDOM,
		JUMP_TO_OWN,
		JUMP_TO_RANDOM_ALWAYS,
		JUMP_TO_RANDOM_SOMETIMES
	} jump_type;
	enum {
		MOVE_RANDOM,
		MOVE_LINEAR,
		MOVE_PERLIN
	} move_type;
	bool retrace;
	bool connect;
	bool wraparound;
};

int main(int argc, char **argv) {

	// Configure
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
		printf("SDL_Init: %s\n", SDL_GetError());
	SDL_WM_SetCaption("trace", nullptr);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	onResize(WINDOW_DEFAULT_W, WINDOW_DEFAULT_H);
	srand(time(nullptr));
	resetMaze();
	printf("%d cursors\n", NUM_CURSORS);

	// Main loop
	bool should_draw_cursors = false;
	bool done = false;
	bool paused = false;
	bool allow_retracing = false;
	bool weave = false;
	bool spontaneousScreenshots = false;
	bool jumpToOwn = false;
	bool debugging = false;
	bool completeFill = false;
	bool moveBoth = false;
	bool gravity = false;
	// unsigned fps = 60;
	unsigned tick_fps = 20;
	timer done_timer(SDL_GetTicks(), 10 * 1000);
	timer tick_timer(SDL_GetTicks(), 1000 / tick_fps);

	for (bool running = true; running;) {

		// Handle events
		for (SDL_Event event; SDL_PollEvent(&event);) {
			if (event.type == SDL_QUIT) {
				running = false;
			} else if (event.type == SDL_VIDEORESIZE) {
				onResize(event.resize.w, event.resize.h);
			} else if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
					case SDLK_F3: case SDLK_c:
						should_draw_cursors = !should_draw_cursors;
						printf("Cursors %s\n", should_draw_cursors ? "enabled" : "disabled");
						break;
					case SDLK_F4: case SDLK_p: case SDLK_SPACE:
						paused = !paused;
						if (paused) {
							puts("Paused");
							SDL_WM_SetCaption("trace: paused", nullptr);
						} else {
							puts("Unpaused");
							SDL_WM_SetCaption("trace", nullptr);
						}
						break;
					case SDLK_RETURN: case SDLK_F1: case SDLK_r:
						resetMaze();
						done = false;
						break;
					case SDLK_F2: {
						saveMaze("screenshot");
						break;
					}
					case SDLK_F6: case SDLK_t:
						allow_retracing = !allow_retracing;
						printf("Retracing %s\n", allow_retracing ? "enabled" : "disabled");
						break;
					case SDLK_F5: case SDLK_d:
						weave = !weave;
						printf("Weaving %s\n", weave ? "enabled" : "disabled");
						break;
					case SDLK_F7:
						jumpToOwn = !jumpToOwn;
						printf("Jump to own %s\n", jumpToOwn ? "enabled" : "disabled");
						break;
					case SDLK_F8:
						completeFill = !completeFill;
						printf("Render Fill %s\n", completeFill ? "enabled" : "disabled");
						break;
					case SDLK_F9:
						moveBoth = !moveBoth;
						printf("Move Both %s\n", moveBoth ? "enabled" : "disabled");
						break;
					case SDLK_F10:
						debugging = !debugging;
						printf("Debug %s\n", debugging ? "enabled" : "disabled");
						break;
					case SDLK_F11:
						spontaneousScreenshots = !spontaneousScreenshots;
						printf("Spontaneous Screenshots %s\n", spontaneousScreenshots ? "enabled" : "disabled");
						break;
					case SDLK_ESCAPE: case SDLK_q:
						running = false;
						break;
					case SDLK_i:
						if (NUM_COLORS == 2) {
							NUM_COLORS = min(max(NUM_COLORS-1, 1), NUM_CURSORS);
							printf("One color will be used in the next trace, and no lower!\n");
						} else if (NUM_COLORS > 2) {
							NUM_COLORS = min(max(NUM_COLORS-1, 1), NUM_CURSORS);
							printf("%d colors to be used in the next maze\n", NUM_COLORS);
						}
						break;
					case SDLK_o:
						if (NUM_COLORS < NUM_CURSORS) {
							NUM_COLORS = min(max(NUM_COLORS+1, 1), NUM_CURSORS);
							if (NUM_COLORS == NUM_CURSORS) {
								printf("%d colors to be used in the next trace, and no more!\n", NUM_COLORS);
							} else {
								printf("%d colors to be used in the next maze\n", NUM_COLORS);
							}
						}
						break;
					default:
						break;
				}
			} else if (event.type == SDL_MOUSEBUTTONDOWN) {
				switch (event.button.button) {
					case SDL_BUTTON_LEFT:
						resetMaze();
						done = false;
						break;
					case SDL_BUTTON_RIGHT: case SDL_BUTTON_MIDDLE:
						paused = !paused;
						if (paused)
							SDL_WM_SetCaption("trace: paused", nullptr);
						else
							SDL_WM_SetCaption("trace", nullptr);
						break;
				}
			}
		}

		if (paused) {
		} else if (done) {
			if (done_timer.done(SDL_GetTicks())) {
				if (spontaneousScreenshots && rand() % 100 == 0) {
					// We particularly liked this one.
					saveMaze("screenshot");
				}
				resetMaze();
				done = false;
			} else {
				SDL_WM_SetCaption("trace: done", nullptr);
			}
		} else if (tick_timer.done(SDL_GetTicks())) {
			tick_timer.reset(SDL_GetTicks());
			tick(allow_retracing, weave, jumpToOwn, moveBoth, gravity);
			done = completed();
			if (done) {
				done_timer.reset(SDL_GetTicks());
			}
		}

		render(should_draw_cursors, debugging, completeFill);
	}
	return 0;
}
