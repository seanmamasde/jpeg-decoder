#include "jpegDecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>

class JPEGDecoder {
 public:
  JPEGDecoder(const char* _inputFilename, const char* _outputFilename)
      : inputFilename(_inputFilename), outputFilename(_outputFilename) {
    readStream();
  }
  ~JPEGDecoder() = default;

  friend class MCU;

  class MCU {
   private:
    JPEGDecoder& decoder;

    const std::array<double, 200> cosVal = [] {
      std::array<double, 200> ret;
      for (int i = 0; i < 200; ++i) ret[i] = std::cos(i * M_PI / 16.0);
      return ret;
    }();

   public:
    // Each color component [1..3], subdivided by (vertical sampling) x
    // (horizontal sampling) So mcu[id][h][w] is an 8x8 block.
    BLOCK mcu[4][2][2];

    explicit MCU(JPEGDecoder& _decoder) : decoder(_decoder) {}
    ~MCU() = default;

#if DEBUG
    void show() {
      printf("*************** MCU content (debug) ***********************\n");
      for (int id = 1; id <= 3; ++id) {
        for (int h = 0; h < decoder.subVector[id].height; ++h) {
          for (int w = 0; w < decoder.subVector[id].width; ++w) {
            printf("mcu id: %d, block(h=%d, w=%d)\n", id, h, w);
            for (int i = 0; i < 8; ++i) {
              for (int j = 0; j < 8; ++j) {
                printf("%lf ", mcu[id][h][w][i][j]);
              }
              printf("\n");
            }
          }
        }
      }
    }
#endif

    void quantify() {
      for (int id = 1; id <= 3; ++id)
        for (int h = 0; h < decoder.subVector[id].height; ++h)
          for (int w = 0; w < decoder.subVector[id].width; ++w)
            for (int i = 0; i < 8; ++i)
              for (int j = 0; j < 8; ++j)
                mcu[id][h][w][i][j] *=
                    decoder.quantTable[decoder.subVector[id].quant][i * 8 + j];
    }

    void zigzag() {
      static constexpr int zigzagIndex[8][8] = {
          {0, 1, 5, 6, 14, 15, 27, 28},     {2, 4, 7, 13, 16, 26, 29, 42},
          {3, 8, 12, 17, 25, 30, 41, 43},   {9, 11, 18, 24, 31, 40, 44, 53},
          {10, 19, 23, 32, 39, 45, 52, 54}, {20, 22, 33, 38, 46, 51, 55, 60},
          {21, 34, 37, 47, 50, 56, 59, 61}, {35, 36, 48, 49, 57, 58, 62, 63}};

      for (int id = 1; id <= 3; ++id) {
        for (int h = 0; h < decoder.subVector[id].height; ++h) {
          for (int w = 0; w < decoder.subVector[id].width; ++w) {
            // temp 8x8 block to store re-mapped values
            double tmp[8][8];
            for (int i = 0; i < 8; ++i) {
              for (int j = 0; j < 8; ++j) {
                // zigzagIndex[i][j] gives index from 0..63
                int idx = zigzagIndex[i][j];
                int row = idx / 8;
                int col = idx % 8;
                tmp[i][j] = mcu[id][h][w][row][col];
              }
            }
            // copy back
            for (int i = 0; i < 8; ++i)
              for (int j = 0; j < 8; ++j) mcu[id][h][w][i][j] = tmp[i][j];
          }
        }
      }
    }

    void idct() {
      for (int id = 1; id <= 3; ++id) {
        for (int h = 0; h < decoder.subVector[id].height; ++h) {
          for (int w = 0; w < decoder.subVector[id].width; ++w) {
            double tmp[8][8]{}, s[8][8]{};

            for (int j = 0; j < 8; ++j) {
              for (int x = 0; x < 8; ++x) {
                for (int y = 0; y < 8; ++y)
                  s[j][x] +=
                      c(y) * mcu[id][h][w][x][y] * cosVal[(j + j + 1) * y];
                s[j][x] /= 2.0;
              }
            }

            for (int i = 0; i < 8; ++i) {
              for (int j = 0; j < 8; ++j) {
                for (int x = 0; x < 8; ++x)
                  tmp[i][j] += c(x) * s[j][x] * cosVal[(i + i + 1) * x];
                tmp[i][j] /= 2.0;
              }
            }

            // copy back
            for (int i = 0; i < 8; ++i)
              for (int j = 0; j < 8; ++j) mcu[id][h][w][i][j] = tmp[i][j];
          }
        }
      }
    }

    RGB** toRGB() {
      // RGB is a 2D array
      RGB** ret = (RGB**)malloc(sizeof(RGB*) * (decoder.maxHeight * 8));
      for (int i = 0; i < decoder.maxHeight * 8; i++)
        ret[i] = (RGB*)malloc(sizeof(RGB) * (decoder.maxWidth * 8));

      for (int i = 0; i < decoder.maxHeight * 8; i++) {
        for (int j = 0; j < decoder.maxWidth * 8; j++) {
          double Y = subsample(1, i, j);
          double Cb = subsample(2, i, j);
          double Cr = subsample(3, i, j);

          uc R = (uc)std::clamp(Y + 1.402 * Cr + 128, 0.0, 255.0);
          uc G =
              (uc)std::clamp(Y - 0.34414 * Cb - 0.71414 * Cr + 128, 0.0, 255.0);
          uc B = (uc)std::clamp(Y + 1.772 * Cb + 128, 0.0, 255.0);

          ret[i][j].R = R;
          ret[i][j].G = G;
          ret[i][j].B = B;
        }
      }
      return ret;
    }

   private:
    double c(int i) { return (i == 0) ? 1.0 / sqrt(2.0) : 1.0; }

    double subsample(int id, int h, int w) {
      // due to subsampling, we scale h/w to the correct block
      int vh = h * decoder.subVector[id].height / decoder.maxHeight;
      int vw = w * decoder.subVector[id].width / decoder.maxWidth;
      return mcu[id][vh / 8][vw / 8][vh % 8][vw % 8];
    }
  };

  ui enterSection(FILE* f, const char* s) {
    // read section length
    uc c;
    ui length;
    fread(&c, 1, 1, f);
    length = c;
    fread(&c, 1, 1, f);
    length = length * 256 + c;

    debug("==================== %s ====================\n", s);
    debug("Section Length: %d\n", length);
    return length;
  }

  void readCOM(FILE* f) {
    ui len = enterSection(f, "COM");
    uc c;
    for (int i = 0; i < (int)(len - 2); i++) {
      fread(&c, 1, 1, f);
      debug("%c", c);
    }
    debug("\n");
  }

  void readAPP0(FILE* f) {
    // JFIF info

    ui len = enterSection(f, "APP0");
    char m[5];
    fread(m, 1, 5, f);
    debug("Info Type: %s\n", m);
    uc v[2];
    fread(v, 1, 2, f);
    debug("Version: %d.%d\n", v[0], v[1]);
    fseek(f, 1, SEEK_CUR);
    fread(v, 1, 2, f);
    debug("X-Direction Pixel Density: %d\n", v[0] * 16 + v[1]);
    fread(v, 1, 2, f);
    debug("Y-Direction Pixel Density: %d\n", v[0] * 16 + v[1]);
    fseek(f, len - 14, SEEK_CUR);
  }

  void readDQT(FILE* f) {
    ui len = enterSection(f, "DQT");
    len -= 2;  // 2 bytes of length itself
    while (len > 0) {
      uc c;
      fread(&c, 1, 1, f);
      len--;
      ui precision = ((c >> 4) == 0 ? 8 : 16);
      debug("Precision: %d\n", precision);
      precision /= 8;  // 8 bits => 1 byte, 16 bits => 2 bytes
      uc id = c & 0x0F;
      debug("Quantization Table ID: %d", id);
      for (int i = 0; i < 64; i++) {
        uc t = 0;
        for (int p = 0; p < (int)precision; p++) {
          uc s;
          fread(&s, 1, 1, f);
          t = (t << 8) + s;
        }
        quantTable[id][i] = t;
      }
      len -= (precision * 64);

#if DEBUG
      for (int i = 0; i < 64; i++) {
        if (i % 8 == 0) printf("\n");
        printf("%2d ", quantTable[id][i]);
      }
      printf("\n");
#endif
    }
  }

  void readSOF(FILE* f) {
    ui len = enterSection(f, "SOF");
    fseek(f, 1, SEEK_CUR);  // skip precision
    uc v[3];
    // height
    fread(v, 1, 2, f);
    image.height = v[0] * 256 + v[1];
    // width
    fread(v, 1, 2, f);
    image.width = v[0] * 256 + v[1];
    debug("Height x Width: %d x %d\n", image.height, image.width);

    fseek(f, 1, SEEK_CUR);  // color components = 3

    // Read each color component's sampling factors & quant table
    for (int i = 0; i < 3; i++) {
      fread(v, 1, 3, f);

      debug("---------------\n");
      debug("Color Component Type: %s\n", (v[0] == 0)   ? "Y"
                                          : (v[0] == 1) ? "Cb"
                                                        : "Cr");
      debug("Quantization Table Type: %s\n", (v[2] == 0) ? "DC" : "AC");
      debug("Horizontal Sampling Factor: %d\n", v[1] >> 4);
      debug("Vertical Sampling Factor: %d\n", v[1] & 0x0F);

      subVector[v[0]].id = v[0];
      subVector[v[0]].width = v[1] >> 4;
      subVector[v[0]].height = v[1] & 0x0F;
      subVector[v[0]].quant = v[2];
      if (subVector[v[0]].height > maxHeight)
        maxHeight = subVector[v[0]].height;
      if (subVector[v[0]].width > maxWidth) maxWidth = subVector[v[0]].width;
    }
  }

  std::pair<uc, ui>* createHuffCode(uc* a, ui number) {
    int pairSize = sizeof(std::pair<uc, ui>);
    auto ret = (std::pair<uc, ui>*)malloc(pairSize * number);
    int code = 0;
    int count = 0;
    for (int i = 0; i < 16; i++) {
      for (int j = 0; j < a[i]; j++) {
        ret[count++] = std::make_pair(i + 1, code);
        code += 1;
      }
      code <<= 1;  // left shift for the next bit-length
    }
    return ret;
  }

  void readDHT(FILE* f) {
    ui len = enterSection(f, "DHT");
    len -= 2;
    while (len > 0) {
      uc v[1];
      fread(v, 1, 1, f);
      uc tableType = v[0] >> 4;
      debug("Table Type: %s\n", (tableType == 0 ? "DC" : "AC"));
      uc id = v[0] & 0x0F;
      debug("ID: %d\n", id);

      uc a[16];
      fread(a, 1, 16, f);
      ui number = 0;
      for (int i = 0; i < 16; i++) {
        debug("%d ", a[i]);
        number += a[i];
      }
      debug("\n");
      auto huffCode = createHuffCode(a, number);
      for (ui i = 0; i < number; i++) {
        uc sym;
        fread(&sym, 1, 1, f);
        // store in the map: map[(codeLen, codeVal)] = symbol
        huffTable[tableType][id][huffCode[i]] = sym;
        debug("%d %d: %d\n", huffCode[i].first, huffCode[i].second, sym);
      }
      free(huffCode);

      len -= (1 + 16 + number);
    }
  }

  void readSOS(FILE* f) {
    ui len = enterSection(f, "SOS");
    fseek(f, 1, SEEK_CUR);  // Number of color components, skip (should be 3)
    for (int i = 0; i < 3; i++) {
      uc v[1];
      fread(v, 1, 1, f);
      debug("---------------\n");
      debug("Color Component Type: %s\n", (v[0] == 0)   ? "Y"
                                          : (v[0] == 1) ? "Cb"
                                                        : "Cr");
      fread(v, 1, 1, f);
      debug("DC Huffman ID: %d\n", v[0] >> 4);
      debug("AC Huffman ID: %d\n", v[0] & 0x0F);
    }
    fseek(f, 3, SEEK_CUR);
  }

  bool getBit(FILE* f) {
    static uc buf;
    static uc count = 0;

    if (count == 0) {
      fread(&buf, 1, 1, f);
      if (buf == 0xFF) {
        uc check;
        fread(&check, 1, 1, f);
        if (check != 0x00)
          fprintf(stderr, "There's non-0xFF00 data in the data section!\n");
      }
    }

    bool ret = (buf & (1 << (7 - count))) != 0;
    count = (count == 7 ? 0 : count + 1);
    return ret;
  }

  uc matchHuff(FILE* f, uc tableID, uc ACorDC) {
    ui codeVal = 0;
    // Try code lengths from 1..16
    for (int bitsRead = 1;; bitsRead++) {
      codeVal = (codeVal << 1) | (ui)getBit(f);

      auto it =
          huffTable[ACorDC][tableID].find(std::make_pair(bitsRead, codeVal));
      if (it != huffTable[ACorDC][tableID].end()) {
        uc sym = it->second;
        return sym;
      }

      if (bitsRead > 16) {
        fprintf(stderr, "Huffman key not found (exceeded 16 bits)\n");
        bitsRead = 1;
        codeVal = 0;
      }
    }
  }

  int readDC(FILE* f, uc number) {
    uc codeLen = matchHuff(f, number, DC);
    if (codeLen == 0) return 0;  // difference is 0
    // read codeLen bits
    uc first = getBit(f);
    int ret = 1;
    for (int i = 1; i < codeLen; i++) {
      uc b = getBit(f);
      ret = (ret << 1) + (first ? b : !b);
    }
    ret = (first ? ret : -ret);
    return ret;
  }

  ACCoeff readAC(FILE* f, uc number) {
    uc x = matchHuff(f, number, AC);
    uc zeros = x >> 4;
    uc codeLen = x & 0x0F;

    if (x == 0)
      return ACCoeff{0, 0, 0};  // 0 => EOB
    else if (x == 0xF0)
      return ACCoeff{0, 16, 0};  // ZRL => 16 zeros

    // decode 'codeLen' bits
    uc first = getBit(f);
    int code = 1;
    for (int i = 1; i < codeLen; i++) {
      uc b = getBit(f);
      code = (code << 1) + (first ? b : !b);
    }
    code = (first ? code : -code);

    return ACCoeff{codeLen, zeros, code};
  }

  MCU readMCU(FILE* f) {
    static int dc[4] = {0, 0, 0, 0};  // DC diff for each comparison
    MCU mcu(*this);

    for (int i = 1; i <= 3; i++) {
      for (int h = 0; h < subVector[i].height; h++) {
        for (int w = 0; w < subVector[i].width; w++) {
          // DC
          int val = readDC(f, i / 2);
          dc[i] += val;  // cumulative
          mcu.mcu[i][h][w][0][0] = dc[i];

          // AC
          ui count = 1;
          while (count < 64) {
            ACCoeff ac = readAC(f, i / 2);
            if (ac.len == 0 && ac.zeros == 16) {
              // ZRL
              for (int z = 0; z < 16; z++) {
                mcu.mcu[i][h][w][count / 8][count % 8] = 0;
                count++;
              }
            } else if (ac.len == 0) {
              // EOB
              break;
            } else {
              // 'zeros' zeroes + the actual coefficient
              for (int z = 0; z < ac.zeros; z++) {
                mcu.mcu[i][h][w][count / 8][count % 8] = 0;
                count++;
              }
              mcu.mcu[i][h][w][count / 8][count % 8] = ac.value;
              count++;
            }
          }
          // fill the remainder with zeros
          while (count < 64) {
            mcu.mcu[i][h][w][count / 8][count % 8] = 0;
            count++;
          }
        }
      }
    }

    return mcu;
  }

  void readData(FILE* f) {
    // Each MCU covers (8 * subVector[i].width) by (8 * subVector[i].height)
    // pixels for each color But typically, maxWidth/maxHeight define the
    // largest sampling among Y/Cb/Cr We'll compute how many MCUs horizontally
    // and vertically:
    int w = (image.width - 1) / (8 * maxWidth) + 1;
    int h = (image.height - 1) / (8 * maxHeight) + 1;

    // Final output size
    int outW = maxWidth * 8 * w;
    int outH = maxHeight * 8 * h;

    // We'll accumulate the final image in a big array (3 bytes per pixel)
    uc* outData = (uc*)malloc(outW * outH * 3);
    if (!outData) {
      fprintf(stderr, "Failed to allocate output image buffer\n");
      return;
    }

    // Initialize to black
    for (int i = 0; i < outW * outH * 3; i++) outData[i] = 0;

    for (int row_mcu = 0; row_mcu < h; row_mcu++) {
      for (int col_mcu = 0; col_mcu < w; col_mcu++) {
        MCU mcu = readMCU(f);
        mcu.quantify();
        mcu.zigzag();
        mcu.idct();

        RGB** blockRGB = mcu.toRGB();

        // Place these RGB values into the correct position in outData
        // region in final image:
        //   from (row_mcu * 8*maxHeight) to (row_mcu+1)*8*maxHeight - 1
        //   from (col_mcu * 8*maxWidth ) to (col_mcu+1)*8*maxWidth  - 1
        for (int py = 0; py < (maxHeight * 8); py++) {
          for (int px = 0; px < (maxWidth * 8); px++) {
            int outY = row_mcu * maxHeight * 8 + py;
            int outX = col_mcu * maxWidth * 8 + px;
            if (outY < outH && outX < outW) {
              // write RGB to outData
              int idx = (outY * outW + outX) * 3;
              outData[idx + 0] = blockRGB[py][px].R;
              outData[idx + 1] = blockRGB[py][px].G;
              outData[idx + 2] = blockRGB[py][px].B;
            }
          }
        }

        // cleanup
        for (int i = 0; i < maxHeight * 8; i++) free(blockRGB[i]);
        free(blockRGB);
      }
    }

    // write out PPM (binary P6)
    FILE* fp = fopen(outputFilename, "wb");
    if (!fp) {
      fprintf(stderr, "Failed to open %s for writing\n", outputFilename);
      free(outData);
      return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", outW, outH);
    fwrite(outData, 1, outW * outH * 3, fp);
    fclose(fp);
    free(outData);
  }

  void readStream() {
    FILE* f = fopen(inputFilename, "rb");
    if (!f) {
      fprintf(stderr, "Failed to open file: %s\n", inputFilename);
      return;
    }

    uc c;
    fread(&c, 1, 1, f);
    while (c == 0xFF) {
      fread(&c, 1, 1, f);
      switch (c) {
        case SOI:
          debug("==================== SOI ====================\n");
          break;
        case APP0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
          readAPP0(f);
          break;
        case COM:
          readCOM(f);
          break;
        case DQT:
          readDQT(f);
          break;
        case SOF:
          readSOF(f);
          break;
        case DHT:
          readDHT(f);
          break;
        case SOS:
          readSOS(f);
          readData(f);
          break;
        case EOI:
          debug("==================== EOI ====================\n");
        default:
          break;
      }
      fread(&c, 1, 1, f);
    }
    // Check for leftover data
    uc leftover;
    if (fread(&leftover, 1, 1, f) != 0) {
      fprintf(stderr, "There's leftover data in the file after EOI.\n");
    }

    fclose(f);
  }

 private:
  const char* inputFilename;
  const char* outputFilename;

  Image image;
  SubVector subVector[4];
  uc maxWidth = 0, maxHeight = 0;
  int quantTable[4][128];
  std::map<std::pair<uc, ui>, uc> huffTable[2][2];
};

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: ./jpegDecoder <jpeg file> <output file>\n");
    return 1;
  }

  JPEGDecoder decoder(argv[1], argv[2]);
  return 0;
}
