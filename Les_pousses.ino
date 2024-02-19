#include <tinyNeoPixel_Static.h>

const uint8_t PIN_NEO = 3;

const uint8_t NUM_NEO = 64;

const uint8_t GRID_SIZE = 8;
const uint8_t GRID_SPACE = 1;

const uint16_t PERLIN_SIZE = 8192;
const uint8_t PERLIN_GRID_SIZE = 9;
const uint16_t PERLIN_TABLE_SIZE = (uint16_t)PERLIN_GRID_SIZE * PERLIN_GRID_SIZE;

const uint8_t UPDATE_RATE = 50;
const uint8_t UPDATE_DELAY = 1000 / UPDATE_RATE;

uint8_t pixels[NUM_NEO * 3];

uint8_t reds[PERLIN_TABLE_SIZE];
uint8_t greens[PERLIN_TABLE_SIZE];
uint8_t blues[PERLIN_TABLE_SIZE];

tinyNeoPixel leds = tinyNeoPixel(NUM_NEO, PIN_NEO, NEO_RGB, pixels);

const uint16_t SRAM_START = 0x0000;		// SRAM start is 0x3800, but registers can be used as well, some have undecided reset value.
const uint16_t SRAM_END = 0x3F00;		// 

uint32_t seed;

uint32_t last;

int16_t drx, dry, dgx, dgy, dbx, dby;
int16_t rx, ry, gx, gy, bx, by;

// get the index in table from the x;y position
uint16_t coordToIndex(uint8_t x, uint8_t y, uint8_t size){
	return x + y * size;
}

// read the unitialized ram for seeding random.
void makeSeed(){
	uint8_t *p;

	for(uint16_t i = SRAM_START ; i < SRAM_END; ++i){
		p = (uint8_t*)i;
		seed ^= (uint32_t)(*p) << ((i % 4) * 8);
	}
}

// Alternative to the Arduino random() function.
uint32_t xorshift(uint32_t max = 0){
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;

	if(max != 0) return (uint64_t)seed * max / ((uint32_t)(-1));
	else return seed;
}

// Get a new random delta
int8_t newDelta(){
	return 16 - xorshift(32);
}

// Init the perlin noise field that we will use.
void initPerlin(){
	for(uint16_t i = 0; i < PERLIN_TABLE_SIZE; ++i){
		reds[i] = xorshift(0xff);
		greens[i] = xorshift(0xff);
		blues[i] = xorshift(0xff);

//		Serial.printf("index %i : %i, %i, %i\n", i, reds[i], greens[i], blues[i]);
	}
}

// Interpolate between to values, given a ratio.
uint8_t interpolate(uint8_t v1, uint8_t v2, uint8_t ratio){
	uint16_t val = v1 * ratio + v2 * (0xff - ratio);
	val /= 0xff;
	return val;
}

// get the perlin value at coordinate x, y, in the given table.
uint8_t perlin(uint8_t* table, uint16_t x, uint16_t y){
	uint8_t x1, x2, y1, y2;
//	float posx = x * PERLIN_GRID_SIZE / PERLIN_SIZE;
//	float posy = y * PERLIN_GRID_SIZE / PERLIN_SIZE;
/*
	x1 = floor(posx);
	x2 = x1 + 1;
	y1 = floor(posy);
	y2 = y1 + 1;
*/
	if(x >= PERLIN_SIZE) x = PERLIN_SIZE - 1;
	if(y >= PERLIN_SIZE) y = PERLIN_SIZE - 1;

	x1 = x * PERLIN_GRID_SIZE / PERLIN_SIZE;
	y1 = y * PERLIN_GRID_SIZE / PERLIN_SIZE;

	uint8_t rX = x * PERLIN_GRID_SIZE % PERLIN_SIZE;
	uint8_t rY = y * PERLIN_GRID_SIZE % PERLIN_SIZE;

	x2 = x1 + 1;
	y2 = y1 + 1;

	uint8_t v1 = interpolate(table[coordToIndex(x1, y1, PERLIN_GRID_SIZE)], table[coordToIndex(x2, y1, PERLIN_GRID_SIZE)], rX);
	uint8_t v2 = interpolate(table[coordToIndex(x1, y2, PERLIN_GRID_SIZE)], table[coordToIndex(x2, y2, PERLIN_GRID_SIZE)], rX);

	return interpolate(v1, v2, rY);
}

// "move" the actual "position".
void move(int16_t* pos, int16_t* delta){
	*pos += *delta;

	if(*pos < 0){
		*pos = 0;
		*delta = newDelta();
		if(*delta < 0) *delta *= -1;
	} else if ((*pos + GRID_SIZE * GRID_SPACE) > PERLIN_SIZE){
		*pos = PERLIN_SIZE - GRID_SIZE * GRID_SPACE;
		*delta = newDelta();
		if(*delta > 0) *delta *= -1;
	}
}

void updateLeds(){
	move(&rx, &drx);
	move(&ry, &dry);
	move(&gx, &dgx);
	move(&gy, &dgy);
	move(&bx, &dbx);
	move(&by, &dby);

	for(uint8_t y = 0; y < GRID_SIZE; ++y){
		for(uint8_t x = 0; x < GRID_SIZE; ++x){
			uint8_t r, g, b;
			r = perlin(reds, rx + x * GRID_SPACE, ry + y * GRID_SIZE);
			g = perlin(greens, gx + x * GRID_SPACE, gy + y * GRID_SIZE);
			b = perlin(blues, bx + x * GRID_SPACE, by + y * GRID_SIZE);
//			g = 0;
//			b = 0;
/*
			Serial.printf("rx %i; ry %i : %i\n", rx + x * GRID_SPACE, ry + y * GRID_SPACE, r);
			Serial.printf("gx %i; gy %i : %i\n", gx + x * GRID_SPACE, gy + y * GRID_SPACE, g);
			Serial.printf("bx %i; by %i : %i\n", bx + x * GRID_SPACE, by + y * GRID_SPACE, b);
*/
			leds.setPixelColor(x + y * GRID_SIZE, g, r, b); 
		}
	}
}

void setup(){
	makeSeed();

	Serial.begin(115200);

	initPerlin();

	pinMode(PIN_NEO, OUTPUT);

	memset(pixels, 0, NUM_NEO*3);


	drx = newDelta();
	dry = newDelta();
	dgx = newDelta();
	dgy = newDelta();
	dbx = newDelta();
	dby = newDelta();

	rx = xorshift(PERLIN_GRID_SIZE);
	ry = xorshift(PERLIN_GRID_SIZE);
	gx = xorshift(PERLIN_GRID_SIZE);
	gy = xorshift(PERLIN_GRID_SIZE);
	bx = xorshift(PERLIN_GRID_SIZE);
	by = xorshift(PERLIN_GRID_SIZE);

	last = millis();
}

void loop(){
	uint32_t now = millis();
	if((now - last) > UPDATE_DELAY){
		last = now;
		leds.show();
		updateLeds();
//		Serial.printf("%i, %i / %i, %i / %i, %i\n", rx, ry, gx, gy, bx, by);
	}
}