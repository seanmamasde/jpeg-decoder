#pragma once
#define DEBUG true

#if DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

using ui = unsigned int;
using uc = unsigned char;

// Markers
static constexpr int SOI = 0xD8;
static constexpr int APP0 = 0xE0;
static constexpr int DQT = 0xDB;
static constexpr int SOF = 0xC0;
static constexpr int DHT = 0xC4;
static constexpr int SOS = 0xDA;
static constexpr int EOI = 0xD9;
static constexpr int COM = 0xFE;

static constexpr int DC = 0;
static constexpr int AC = 1;

typedef double BLOCK[8][8];

typedef struct {
  int height, width;
} Image;

typedef struct {
  uc R, G, B;
} RGB;

typedef struct {
  uc len;
  uc zeros;
  int value;
} ACCoeff;

typedef struct {
  uc id;
  uc width;
  uc height;
  uc quant;
} SubVector;
