# jpeg-decoder

This is a simple JPEG decoder implementation in C++.

## Functionalities

This decoder decodes a baseline JPEG image (no interlacing) and store it in a PPM image.

## Usage

```bash
./jpeg-decoder <input_file> <output_file>
```

## Build From Source

1. Clone the repository and `cd` into it:
   
   ```bash
   $ git clone https://github.com/seanmamasde/jpeg-decoder.git

   $ cd jpeg-decoder
   ```

2. Build the project:
   
   ```bash
   mkdir build && cd build

   # build the binary
   cmake .. && cmake --build .
   ```

3. Run the decoder:
   
   `./jpegDecoder <input_file> <output_file>`

   the provided input file is `../lenna.jpg`
   
4. Clean the project:
   
   `rm -rf build`

## Troubleshooting

- If the decoder fails to decode the image, it is likely that the input file is not a baseline JPEG image. To check whether you image is a baseline JPEG image, you can use `identify` from imagemagick:

    `magick identify -verbose <input_file> | grep "Interlace"`

    If the output is `Interlace: None`, then the image is a baseline JPEG image.

- To convert a interlaced JPEG image into a baseline one, you can use `convert` from imagemagick:
  
    `magick [convert] <input_file> -interlace none <output_file>`

## Implmentation

See [Implementation.md](./Implementation.md)

## Explanation

See [Explanation.md](./Explanation.md)

## Appendix

- The original lenna image is from [Wikipedia](https://en.wikipedia.org/wiki/Lenna).

- The [lenna.jpg](./lenna.jpg) is converted from the original one using `magick lenna.png -interlace none lenna.jpg`.