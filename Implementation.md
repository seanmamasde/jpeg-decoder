# Implementation

## **jpegDecoder.h**

- **`#define DEBUG false`**  
  Toggle for printing debug information. When `DEBUG` is true, internal logs (`debug(...)`) will be printed.

- **`using ui = unsigned int;`**  
  Typedef for an unsigned integer, used throughout the code (e.g. for Huffman code values, lengths, etc.).

- **`using uc = unsigned char;`**  
  Typedef for an unsigned char, used for many JPEG markers and small data units.

- **JPEG Marker Constants** (`SOI`, `APP0`, `DQT`, `SOF`, `DHT`, `SOS`, `EOI`, `COM`)  
  Numeric values indicating which marker we’re reading in the JPEG file.

- **Huffman Table IDs** (`DC`, `AC`)  
  Distinguishes DC (direct current) tables from AC (alternating current) tables in Huffman decoding.

- **`typedef double BLOCK[8][8];`**  
  Represents an 8×8 block of image data (for Y/Cb/Cr).

- **`typedef struct { int height, width; } Image;`**  
  Stores overall image dimensions.

- **`typedef struct { uc R, G, B; } RGB;`**  
  Stores one pixel’s RGB components.

- **`typedef struct { uc len; uc zeros; int value; } acCode;`**  
  Used when decoding AC coefficients. `len` = number of bits for the AC coefficient, `zeros` = run of zero values before the coefficient, `value` = the actual coefficient.

- **`typedef struct { uc id; uc width; uc height; uc quant; } SubVector;`**  
  Holds sampling factors and associated quantization table ID for a color component (e.g. Y, Cb, or Cr).

---

## **jpegDecoder.cpp**

### **Class `JPEGDecoder`**

#### **Constructor**
```cpp
JPEGDecoder(const char* _inputFilename, const char* _outputFilename)
```
- Stores the input and output file names.  
- Immediately calls `readStream()` to parse the JPEG file and decode its content.  

#### **Destructor**
```cpp
~JPEGDecoder() = default;
```
- A no-op destructor (does nothing special).

---

### **Nested Class `MCU`**

`MCU` represents a **Minimum Coded Unit**—the smallest group of data blocks that the JPEG file encodes at once (depending on subsampling).

```cpp
class MCU {
 private:
  JPEGDecoder& decoder;
  std::array<double, 200> cosVal;
  ...
 public:
  BLOCK mcu[4][2][2];
  ...
};
```

- **`decoder`**  
  Reference back to the main `JPEGDecoder` for accessing its tables, image info, etc.

- **`cosVal`**  
  Precomputed cosine values used for the IDCT. Indexed up to `i * M_PI / 16.0`. Storing these avoids recalculating cosines repeatedly.

- **`BLOCK mcu[4][2][2]`**  
  Each color component (1..3 for Y, Cb, Cr) can have multiple 8×8 blocks if the sampling factors are greater than 1×1. For each component, we can have up to 2×2 blocks.  
  - Index `[id]` = color component (1=Y, 2=Cb, 3=Cr).  
  - Index `[h][w]` = which sub-block in vertical/horizontal direction.  

#### **Constructor**
```cpp
explicit MCU(JPEGDecoder& _decoder) : decoder(_decoder) {}
```
- Links the MCU with its parent `JPEGDecoder`.

#### **`quantify()`**
- Performs the *inverse quantization* step.  
- Multiplies each DCT coefficient in each 8×8 block by the corresponding entry in the quantization table.  

#### **`zigzag()`**
- "Un-zigzags" the 64 coefficients from the zigzag order to the standard 8×8 matrix order.  
- Uses `zigzagIndex` to map each `(row, col)` in the final 2D block to the correct index in the original zigzag array.  

#### **`idct()`**
- Applies the 2D Inverse Discrete Cosine Transform on each 8×8 block.  
- Uses the `cosVal` array and an internal function `c(i)` to account for the “\(\frac{1}{\sqrt{2}}\)” factor when `i = 0`.  
- The result is stored back into `mcu[id][h][w][i][j]`.  

#### **`toRGB()`**
- Converts the entire MCU’s Y/Cb/Cr data to an array of RGB pixels.  
- Because of subsampling, each pixel’s final color channels might come from different block indices (handled by `subsample()`).  
- Returns a dynamically allocated 2D array of size `(decoder.maxHeight*8) x (decoder.maxWidth*8)`.  

##### Notable Detail: 
- The function adds **128** in the color conversion formula (e.g. `Y + 1.402 * Cr + 128`), which is one way to handle the YCbCr offset. Different JPEG codebases sometimes use different offsets (and scaling factors).  

#### **Private Helper: `double c(int i)`**
- Returns `1.0 / sqrt(2.0)` if `i == 0`, else 1.0.  
- This is part of the standard IDCT scaling factor for the first row/column.

#### **Private Helper: `double subsample(int id, int h, int w)`**
- Given a requested pixel `(h, w)` in the *final* image, calculates the corresponding block and coefficient in the subsampled data.  

---

### **`enterSection(FILE* f, const char* s)`**
- Reads the next 2 bytes from the file to get the marker’s length.  
- Prints (via `debug`) the section type and length.  
- Returns the length to the caller.  

### **`readCOM(FILE* f)`**
- Handles the **Comment** (COM) marker.  
- Reads the comment length and then reads/prints the comment data (if debugging is enabled).

### **`readAPP0(FILE* f)`**
- Handles **APP0** marker, typically where JFIF info is stored.  
- Reads length, JFIF signature, version, and pixel density.  
- Skips the rest of the data in this section.  

### **`readDQT(FILE* f)`**
- Handles **Define Quantization Table** (DQT) marker.  
- Reads the quant table(s) and stores them into `quantTable[id]`.  
- Each table can be 8-bit or 16-bit precision, but often only 8-bit is used.  

### **`readSOF(FILE* f)`**
- Reads the **Start of Frame** marker, which contains:  
  - Image height & width.  
  - Number of color components (usually 3 for YCbCr).  
  - Sampling factors (horizontal/vertical) for each component.  
  - Which quantization table each component uses.  
- Fills out `image.height`, `image.width`, and the `subVector` array.  
- Also updates `maxHeight` and `maxWidth` with the largest sampling factors.  

### **`createHuffCode(uc* a, ui number)`**
- Given an array `a` of 16 entries (each entry says how many symbols have length `i+1`), it creates canonical Huffman codes.  
- Returns an array of `(codeLength, codeValue)` pairs.  

### **`readDHT(FILE* f)`**
- Reads the **Define Huffman Table** (DHT) marker.  
- For each table, it reads the distribution of code lengths and then the actual symbols.  
- Builds the Huffman code map (`huffTable[tableType][id]`), which is used later for decoding.  

### **`readSOS(FILE* f)`**
- Reads the **Start of Scan** marker.  
- Retrieves which Huffman tables (DC/AC) apply to each component.  
- Then calls `readData(f)` to begin reading the compressed blocks.  

### **`getBit(FILE* f)`**
- Returns the next bit from the JPEG’s compressed data stream.  
- If a `0xFF` byte appears, it expects a `0x00` “stuffed” byte to follow (standard JPEG byte-stuffing). Otherwise, it warns if it sees something else.  

### **`matchHuff(FILE* f, uc tableID, uc ACorDC)`**
- Reads bits one by one, building up a code.  
- Checks if this code matches an entry in the Huffman table.  
- If it doesn’t match by 16 bits, it resets (though in correct JPEG files it should match before that).  

### **`readDC(FILE* f, uc number)`**
- Decodes one DC coefficient difference from the bitstream using Huffman codes.  
- Interprets the sign bit properly (positive or negative).  

### **`readAC(FILE* f, uc number)`**
- Decodes a run of zeros plus one nonzero AC coefficient (or a run-length special code).  
- Returns an `acCode` struct describing the number of leading zeros and the actual coefficient.  
- Handles special symbols `0x00` (EOB) and `0xF0` (ZRL).  

### **`readMCU(FILE* f)`**
- Reads a single MCU for **all** components (Y, Cb, Cr).  
- First decodes the DC coefficient, then AC coefficients, storing them into `mcu[id][h][w][i][j]`.  
- Maintains “dc” array to keep track of the running DC difference for each component.  

### **`readData(FILE* f)`**
- Calculates how many MCUs fit across the image.  
- For each MCU:  
  1. Reads its data (`readMCU`).  
  2. Calls `quantify()`, `zigzag()`, `idct()`.  
  3. Converts to RGB (`toRGB()`).  
  4. Places the resulting pixels into a final output buffer.  
- Finally writes the buffer to a **PPM (P6)** file named by `outputFilename`.  

### **`readStream()`**
- The main dispatcher for reading JPEG markers:  
  1. Reads 0xFF, then the next byte (marker code).  
  2. Depending on the marker, calls the corresponding function (`readAPP0`, `readDQT`, `readSOF`, `readDHT`, `readSOS`, etc.).  
  3. Once SOS is encountered, the compressed data (MCUs) are read.  
  4. Stops upon **End of Image** (EOI) marker.  
- Warns if leftover data remains after EOI.  

---

### **`int main(int argc, char* argv[])`**
- Expects exactly 2 arguments: input JPEG file and output file name.  
- Creates a `JPEGDecoder decoder(argv[1], argv[2])`.  
- The actual decoding happens in the constructor’s `readStream()` call.  

---

## **Special Notes / Implementation Details**

1. **Subsampling**:  
   - Each MCU can have multiple 8×8 blocks per component depending on horizontal/vertical sampling factors. Code uses `subVector[id].width` and `subVector[id].height` to handle this.  

2. **Color Conversion**:  
   - Uses a simple formula for YCbCr → RGB with an added 128 offset on the Cr and Cb channels. Different JPEG decoders might use slightly different constants or offsets.  
