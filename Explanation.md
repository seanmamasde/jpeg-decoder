# Explanation

> Note: This part was largely referenced from [here](https://github.com/MROS/jpeg_tutorial)

## Overview

### JPEG Defines a Family of Algorithms

The JPEG standard includes multiple compression algorithms and encoding methods, meaning two `.jpg` files might use different processes. The compression algorithms include:

- **Sequential**: Decodes from top to bottom.
- **Progressive**: Gradually refines clarity during decoding.
- **Hierarchical**: Encodes at different resolutions.
- **Lossless**: Retains all original data.

Except for lossless, all JPEG algorithms are lossy.

### Baseline JPEG

Baseline JPEG is the simplest and most common configuration. It uses:

- **Sequential compression**
- **Huffman encoding**
- **8-bit precision**

This format is widely used, making it an ideal starting point for learning JPEG decoding.

### JFIF Standard

Most `.jpg` files conform to JFIF, which extends JPEG with features like thumbnails and display ratios, while restricting options like color space. Our decoder will only address JFIF-compatible files, ignoring extra features like thumbnails for simplicity.

## Reading Sections

### JPEG Encoding/Decoding Process:

Encoding Process:
1. Convert RGB to YCbCr
2. Subsampling
3. DCT
4. Quantization
5. Huffman Encoding

Decoding Process:
1. Huffman Decoding
2. Dequantization
3. Inverse DCT
4. Upsampling
5. YCbCr to RGB

These are the data generated during encoding:
- **Subsampling rate**: Defines the degree of color reduction.
- **Quantization tables**: Determines the compression level.
- **Huffman tables**: Custom encoding based on image content.
- **Compressed image data**: The encoded pixel data.

### Structure of a JPEG file

1. **SOI `0xFFD8`**: Start of Image
2. **APP0**: Application 0, contains JFIF header, 0 means JFIF.
3. **SOF0 `0xFFC0`**: Start of Frame 0, 0 means baseline.
4. **DQT `0xFFDB`** : Define Quantization Table
5. **DHT `0xFFC4`** : Define Huffman Table
6. **SOS `0xFFDA`** : Start of Scan
7. **Data** : Compressed image data
8. **EOI `0xFFD9`** : End of Image

Note that 2~5 may be arranged in any order, and DQT and DHT may contain multiple.

Every segments starts with `0xFF` and followed by a byte indicating the segment type.
For `SOI` and `EOI`, they contain no information so the only have 2 bytes (marker length).
For other segments, after the marker (2 bytes), there is a 2-byte length field, followed by the actual data. In the compressed image data, if there's `0xFF` in the data, it will be followed by `0x00` to avoid confusion with markers.


### DQT

The DQT segment may contain more than 1 quantization table. Each table consists of:
| Field | Size            | Description                                                                    |
| ----- | --------------- | ------------------------------------------------------------------------------ |
| 1️⃣     | 1 byte          | High 4 bits: value size (0 = 1 byte, 1 = 2 bytes). Low 4 bits: table ID (0~3). |
| 2️⃣     | 64 or 128 bytes | 64 1 or 2 bytes quantization value, size depends on the high 4 bits of 1️⃣       |

Note that a JPEG image can contain up to 4 quantization tables.

### DHT

#### Huffman Tree

Standardized Huffman tree reorganizes nodes to be more compact:
1. Original tree: Nodes are scattered.
2. Standard tree: Nodes of the same height are aligned for efficient storage.

[Example](https://github.com/MROS/jpeg_tutorial/blob/master/doc/%E8%B7%9F%E6%88%91%E5%AF%ABjpeg%E8%A7%A3%E7%A2%BC%E5%99%A8%EF%BC%88%E4%B8%89%EF%BC%89%E8%AE%80%E5%8F%96%E9%87%8F%E5%8C%96%E8%A1%A8%E3%80%81%E9%9C%8D%E5%A4%AB%E6%9B%BC%E8%A1%A8.md#%E7%AF%84%E5%BC%8F%E9%9C%8D%E5%A4%AB%E6%9B%BC%E7%B7%A8%E7%A2%BC)

#### JPEG Huffman Table

A DHT section may contain multiple Huffman tables. Each table consists of:
| Field | Size                 | Description                                                                                |
| ----- | -------------------- | ------------------------------------------------------------------------------------------ |
| 1️⃣     | 1 byte               | High 4 bits: table class (0 = DC, 1 = AC). Low 4 bits: table ID (0 or 1).                  |
| 2️⃣     | 16 byte              | n-th bytes indicates the number of Huffman tree leaf nodes when height = n.                |
| 3️⃣     | amount of leaf nodes | Represents corresponding symbol of the each leaf nodes, from bottom to top, left to right. |

[Example](https://github.com/MROS/jpeg_tutorial/blob/master/doc/%E8%B7%9F%E6%88%91%E5%AF%ABjpeg%E8%A7%A3%E7%A2%BC%E5%99%A8%EF%BC%88%E4%B8%89%EF%BC%89%E8%AE%80%E5%8F%96%E9%87%8F%E5%8C%96%E8%A1%A8%E3%80%81%E9%9C%8D%E5%A4%AB%E6%9B%BC%E8%A1%A8.md#jpeg-%E4%B8%AD%E7%9A%84%E9%9C%8D%E5%A4%AB%E6%9B%BC%E8%A1%A8)

### MCU

The MCU (Minimum Coded Unit) is the smallest unit of a JPEG image. A compressed image is divided into MCUs, which are then processed by DCT. A MCU is not necessarily 16x16 pixels, it can also be rectangular. The MCU size depends on the subsampling rate.

Each color component basic unit is a 8x8 block. The horizontal/vertical side of MCU depends on the subsampling rate, which is (8 * highest horizontal/vertical rate).

### SOF0

SOF0 segment contains the height and width of the image, as well as the subsampling rate of each color component. It consists of:

| Field | Size    | Description                 | Comment                                |
| ----- | ------- | --------------------------- | -------------------------------------- |
| 1️⃣     | 1 byte  | Precision (8 bits)          | Baseline has fixed precision of 8 bits |
| 2️⃣     | 2 bytes | Height                      |                                        |
| 3️⃣     | 2 bytes | Width                       |                                        |
| 4️⃣     | 1 byte  | Number of color components  | JFIF standard requires 3 (Y, Cb, Cr)   |
| 5️⃣     | 3 bytes | Color component information | See next table                         |

Color component information:
| Field | Size   | Description           | Comment                                                                  |
| ----- | ------ | --------------------- | ------------------------------------------------------------------------ |
| 1️⃣     | 1 byte | Color Component ID    | Y: 1, Cb: 2, Cr: 3                                                       |
| 2️⃣     | 1 byte | Sampling rate         | high 4 bit: horizontal, low 4 bit: vertical. Sampling rate could be 1~4. |
| 3️⃣     | 1 byte | Quantization table ID | which quantization table to refer to when decoding.                      |

From the above sampling rate restrictions, we can tell that the maximum length of a side of MCU is:
8 pixels tall in color * 4 max sampling rate = 32 pixels.

### SOS

SOS segment contains the Huffman table IDs for each color component. It consists of:

| Field | Size        | Description                                          | Comment                                    |
| ----- | ----------- | ---------------------------------------------------- | ------------------------------------------ |
| 1️⃣     | 1 byte      | Number of components                                 | JFIF standard requires 3 (Y, Cb, Cr)       |
| 2️⃣     | 2 * 3 bytes | Corresponding Huffman table for each color component | 2 bytes for each component, see next table |
| 3️⃣     | 3 bytes     | Unused in baseline                                   | Baseline JPEG has fixed 0x00 0x3F 0x00.    |

Huffman table information:
| Field | Size   | Description        | Comment                                |
| ----- | ------ | ------------------ | -------------------------------------- |
| 1️⃣     | 1 byte | Color Component ID | Y: 1, Cb: 2, Cr: 3                     |
| 2️⃣     | 1 byte | Huffman table ID   | high 4 bits for DC, low 4 bits for AC. |

### Reading a Block

The MCU data is in a sequence, arranged from left to right, and then top to bottom. The color components blocks are in the same arrangement.

If the image dimension is not a multiple of the MCU size, add MCUs to cover the entire image. For example:
- Image width: 40px, height: 60px
- MCU size: 16x16
- MCU grid: 3 (horizontal) x 4 (vertical)

Each block (8x8 pixels) contains:
- DC Coefficient (top left value):
  - Deocded using DC Huffman table.
  - Represents the difference from the previous block's DC value (`DC[n] = DC[n-1] + diff`).
  - For the first block, use the absolute value.
- AC Coefficients (remaining 63 values):
  - Decoded using AC Huffman table.
  - High 4 bits: Number of consecutive zeros.
  - Low 4 bits: Bit length of the next non-zero value.
  - Special Codes:
    - `0x00`: All remaining values are zero.
    - `0xF0`: Next 16 values are zero.

### Dequantization

Just multiply the quantization table value with the corresponding DCT coefficient.

### Undo Zigzag

Reverse the Zigzag scan to get the 8x8 block.

### Inverse DCT

$$
\text{result}[i][j] = \frac{1}{4} \sum_{x=0}^7 \sum_{y=0}^7 C_x C_y \cos\left(\frac{(2i + 1)x\pi}{16}\right) \cos\left(\frac{(2j + 1)y\pi}{16}\right) \text{block}[x][y]
$$

The $C_i$ is as follows:
$$
C_0 = \frac{1}{\sqrt{2}}, C_i = 1, \forall i > 0
$$

### Upsampling

Reverse the process where you mapped MCU to each of the color components, and you can get the Y, Cb, Cr that each pixel corresponds to.

### YCbCr to RGB

$$
\begin{align*}
R &= Y + 1.402 \cdot (Cr - 128) \\
G &= Y - 0.344136 \cdot (Cb - 128) - 0.714136 \cdot (Cr - 128) \\
B &= Y + 1.772 \cdot (Cb - 128)
\end{align*}
$$
