// This library should be a single header, compilable on a default Windows
// Code::Blocks install (no linker/compiler settings). It may be impossible
// to have no linker settings on GNU/Linux, but it is lower priority.
//
// Ctrl-f PISHTOV_START

#include <vector>
int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size_t in_size) {
  // picoPNG version 20101224
  // Copyright (c) 2005-2010 Lode Vandevenne
  //
  // This software is provided 'as-is', without any express or implied
  // warranty. In no event will the authors be held liable for any damages
  // arising from the use of this software.
  //
  // Permission is granted to anyone to use this software for any purpose,
  // including commercial applications, and to alter it and redistribute it
  // freely, subject to the following restrictions:
  //
  //     1. The origin of this software must not be misrepresented; you must not
  //     claim that you wrote the original software. If you use this software
  //     in a product, an acknowledgment in the product documentation would be
  //     appreciated but is not required.
  //     2. Altered source versions must be plainly marked as such, and must not be
  //     misrepresented as being the original software.
  //     3. This notice may not be removed or altered from any source distribution.
  
  // picoPNG is a PNG decoder in one C++ function of around 500 lines. Use picoPNG for
  // programs that need only 1 .cpp file. Since it's a single function, it's very limited,
  // it can convert a PNG to raw pixel data either converted to 32-bit RGBA color or
  // with no color conversion at all. For anything more complex, another tiny library
  // is available: LodePNG (lodepng.c(pp)), which is a single source and header file.
  // Apologies for the compact code style, it's to make this tiny.
  
  static const unsigned long LENBASE[29] =  {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
  static const unsigned long LENEXTRA[29] = {0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0};
  static const unsigned long DISTBASE[30] =  {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
  static const unsigned long DISTEXTRA[30] = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13};
  static const unsigned long CLCL[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15}; //code length code lengths
  struct Zlib //nested functions for zlib decompression
  {
    static unsigned long readBitFromStream(size_t& bitp, const unsigned char* bits) { unsigned long result = (bits[bitp >> 3] >> (bitp & 0x7)) & 1; bitp++; return result;}
    static unsigned long readBitsFromStream(size_t& bitp, const unsigned char* bits, size_t nbits)
    {
      unsigned long result = 0;
      for(size_t i = 0; i < nbits; i++) result += (readBitFromStream(bitp, bits)) << i;
      return result;
    }
    struct HuffmanTree
    {
      int makeFromLengths(const std::vector<unsigned long>& bitlen, unsigned long maxbitlen)
      { //make tree given the lengths
        unsigned long numcodes = (unsigned long)(bitlen.size()), treepos = 0, nodefilled = 0;
        std::vector<unsigned long> tree1d(numcodes), blcount(maxbitlen + 1, 0), nextcode(maxbitlen + 1, 0);
        for(unsigned long bits = 0; bits < numcodes; bits++) blcount[bitlen[bits]]++; //count number of instances of each code length
        for(unsigned long bits = 1; bits <= maxbitlen; bits++) nextcode[bits] = (nextcode[bits - 1] + blcount[bits - 1]) << 1;
        for(unsigned long n = 0; n < numcodes; n++) if(bitlen[n] != 0) tree1d[n] = nextcode[bitlen[n]]++; //generate all the codes
        tree2d.clear(); tree2d.resize(numcodes * 2, 32767); //32767 here means the tree2d isn't filled there yet
        for(unsigned long n = 0; n < numcodes; n++) //the codes
        for(unsigned long i = 0; i < bitlen[n]; i++) //the bits for this code
        {
          unsigned long bit = (tree1d[n] >> (bitlen[n] - i - 1)) & 1;
          if(treepos > numcodes - 2) return 55;
          if(tree2d[2 * treepos + bit] == 32767) //not yet filled in
          {
            if(i + 1 == bitlen[n]) { tree2d[2 * treepos + bit] = n; treepos = 0; } //last bit
            else { tree2d[2 * treepos + bit] = ++nodefilled + numcodes; treepos = nodefilled; } //addresses are encoded as values > numcodes
          }
          else treepos = tree2d[2 * treepos + bit] - numcodes; //subtract numcodes from address to get address value
        }
        return 0;
      }
      int decode(bool& decoded, unsigned long& result, size_t& treepos, unsigned long bit) const
      { //Decodes a symbol from the tree
        unsigned long numcodes = (unsigned long)tree2d.size() / 2;
        if(treepos >= numcodes) return 11; //error: you appeared outside the codetree
        result = tree2d[2 * treepos + bit];
        decoded = (result < numcodes);
        treepos = decoded ? 0 : result - numcodes;
        return 0;
      }
      std::vector<unsigned long> tree2d; //2D representation of a huffman tree: The one dimension is "0" or "1", the other contains all nodes and leaves of the tree.
    };
    struct Inflator
    {
      int error;
      void inflate(std::vector<unsigned char>& out, const std::vector<unsigned char>& in, size_t inpos = 0)
      {
        size_t bp = 0, pos = 0; //bit pointer and byte pointer
        error = 0;
        unsigned long BFINAL = 0;
        while(!BFINAL && !error)
        {
          if(bp >> 3 >= in.size()) { error = 52; return; } //error, bit pointer will jump past memory
          BFINAL = readBitFromStream(bp, &in[inpos]);
          unsigned long BTYPE = readBitFromStream(bp, &in[inpos]); BTYPE += 2 * readBitFromStream(bp, &in[inpos]);
          if(BTYPE == 3) { error = 20; return; } //error: invalid BTYPE
          else if(BTYPE == 0) inflateNoCompression(out, &in[inpos], bp, pos, in.size());
          else inflateHuffmanBlock(out, &in[inpos], bp, pos, in.size(), BTYPE);
        }
        if(!error) out.resize(pos); //Only now we know the true size of out, resize it to that
      }
      void generateFixedTrees(HuffmanTree& tree, HuffmanTree& treeD) //get the tree of a deflated block with fixed tree
      {
        std::vector<unsigned long> bitlen(288, 8), bitlenD(32, 5);;
        for(size_t i = 144; i <= 255; i++) bitlen[i] = 9;
        for(size_t i = 256; i <= 279; i++) bitlen[i] = 7;
        tree.makeFromLengths(bitlen, 15);
        treeD.makeFromLengths(bitlenD, 15);
      }
      HuffmanTree codetree, codetreeD, codelengthcodetree; //the code tree for Huffman codes, dist codes, and code length codes
      unsigned long huffmanDecodeSymbol(const unsigned char* in, size_t& bp, const HuffmanTree& codetree, size_t inlength)
      { //decode a single symbol from given list of bits with given code tree. return value is the symbol
        bool decoded; unsigned long ct;
        for(size_t treepos = 0;;)
        {
          if((bp & 0x07) == 0 && (bp >> 3) > inlength) { error = 10; return 0; } //error: end reached without endcode
          error = codetree.decode(decoded, ct, treepos, readBitFromStream(bp, in)); if(error) return 0; //stop, an error happened
          if(decoded) return ct;
        }
      }
      void getTreeInflateDynamic(HuffmanTree& tree, HuffmanTree& treeD, const unsigned char* in, size_t& bp, size_t inlength)
      { //get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree
        std::vector<unsigned long> bitlen(288, 0), bitlenD(32, 0);
        if(bp >> 3 >= inlength - 2) { error = 49; return; } //the bit pointer is or will go past the memory
        size_t HLIT =  readBitsFromStream(bp, in, 5) + 257; //number of literal/length codes + 257
        size_t HDIST = readBitsFromStream(bp, in, 5) + 1; //number of dist codes + 1
        size_t HCLEN = readBitsFromStream(bp, in, 4) + 4; //number of code length codes + 4
        std::vector<unsigned long> codelengthcode(19); //lengths of tree to decode the lengths of the dynamic tree
        for(size_t i = 0; i < 19; i++) codelengthcode[CLCL[i]] = (i < HCLEN) ? readBitsFromStream(bp, in, 3) : 0;
        error = codelengthcodetree.makeFromLengths(codelengthcode, 7); if(error) return;
        size_t i = 0, replength;
        while(i < HLIT + HDIST)
        {
          unsigned long code = huffmanDecodeSymbol(in, bp, codelengthcodetree, inlength); if(error) return;
          if(code <= 15)  { if(i < HLIT) bitlen[i++] = code; else bitlenD[i++ - HLIT] = code; } //a length code
          else if(code == 16) //repeat previous
          {
            if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
            replength = 3 + readBitsFromStream(bp, in, 2);
            unsigned long value; //set value to the previous code
            if((i - 1) < HLIT) value = bitlen[i - 1];
            else value = bitlenD[i - HLIT - 1];
            for(size_t n = 0; n < replength; n++) //repeat this value in the next lengths
            {
              if(i >= HLIT + HDIST) { error = 13; return; } //error: i is larger than the amount of codes
              if(i < HLIT) bitlen[i++] = value; else bitlenD[i++ - HLIT] = value;
            }
          }
          else if(code == 17) //repeat "0" 3-10 times
          {
            if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
            replength = 3 + readBitsFromStream(bp, in, 3);
            for(size_t n = 0; n < replength; n++) //repeat this value in the next lengths
            {
              if(i >= HLIT + HDIST) { error = 14; return; } //error: i is larger than the amount of codes
              if(i < HLIT) bitlen[i++] = 0; else bitlenD[i++ - HLIT] = 0;
            }
          }
          else if(code == 18) //repeat "0" 11-138 times
          {
            if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
            replength = 11 + readBitsFromStream(bp, in, 7);
            for(size_t n = 0; n < replength; n++) //repeat this value in the next lengths
            {
              if(i >= HLIT + HDIST) { error = 15; return; } //error: i is larger than the amount of codes
              if(i < HLIT) bitlen[i++] = 0; else bitlenD[i++ - HLIT] = 0;
            }
          }
          else { error = 16; return; } //error: somehow an unexisting code appeared. This can never happen.
        }
        if(bitlen[256] == 0) { error = 64; return; } //the length of the end code 256 must be larger than 0
        error = tree.makeFromLengths(bitlen, 15); if(error) return; //now we've finally got HLIT and HDIST, so generate the code trees, and the function is done
        error = treeD.makeFromLengths(bitlenD, 15); if(error) return;
      }
      void inflateHuffmanBlock(std::vector<unsigned char>& out, const unsigned char* in, size_t& bp, size_t& pos, size_t inlength, unsigned long btype) 
      {
        if(btype == 1) { generateFixedTrees(codetree, codetreeD); }
        else if(btype == 2) { getTreeInflateDynamic(codetree, codetreeD, in, bp, inlength); if(error) return; }
        for(;;)
        {
          unsigned long code = huffmanDecodeSymbol(in, bp, codetree, inlength); if(error) return;
          if(code == 256) return; //end code
          else if(code <= 255) //literal symbol
          {
            if(pos >= out.size()) out.resize((pos + 1) * 2); //reserve more room
            out[pos++] = (unsigned char)(code);
          }
          else if(code >= 257 && code <= 285) //length code
          {
            size_t length = LENBASE[code - 257], numextrabits = LENEXTRA[code - 257];
            if((bp >> 3) >= inlength) { error = 51; return; } //error, bit pointer will jump past memory
            length += readBitsFromStream(bp, in, numextrabits);
            unsigned long codeD = huffmanDecodeSymbol(in, bp, codetreeD, inlength); if(error) return;
            if(codeD > 29) { error = 18; return; } //error: invalid dist code (30-31 are never used)
            unsigned long dist = DISTBASE[codeD], numextrabitsD = DISTEXTRA[codeD];
            if((bp >> 3) >= inlength) { error = 51; return; } //error, bit pointer will jump past memory
            dist += readBitsFromStream(bp, in, numextrabitsD);
            size_t start = pos, back = start - dist; //backwards
            if(pos + length >= out.size()) out.resize((pos + length) * 2); //reserve more room
            for(size_t i = 0; i < length; i++) { out[pos++] = out[back++]; if(back >= start) back = start - dist; }
          }
        }
      }
      void inflateNoCompression(std::vector<unsigned char>& out, const unsigned char* in, size_t& bp, size_t& pos, size_t inlength)
      {
        while((bp & 0x7) != 0) bp++; //go to first boundary of byte
        size_t p = bp / 8;
        if(p >= inlength - 4) { error = 52; return; } //error, bit pointer will jump past memory
        unsigned long LEN = in[p] + 256 * in[p + 1], NLEN = in[p + 2] + 256 * in[p + 3]; p += 4;
        if(LEN + NLEN != 65535) { error = 21; return; } //error: NLEN is not one's complement of LEN
        if(pos + LEN >= out.size()) out.resize(pos + LEN);
        if(p + LEN > inlength) { error = 23; return; } //error: reading outside of in buffer
        for(unsigned long n = 0; n < LEN; n++) out[pos++] = in[p++]; //read LEN bytes of literal data
        bp = p * 8;
      }
    };
    int decompress(std::vector<unsigned char>& out, const std::vector<unsigned char>& in) //returns error value
    {
      Inflator inflator;
      if(in.size() < 2) { return 53; } //error, size of zlib data too small
      if((in[0] * 256 + in[1]) % 31 != 0) { return 24; } //error: 256 * in[0] + in[1] must be a multiple of 31, the FCHECK value is supposed to be made that way
      unsigned long CM = in[0] & 15, CINFO = (in[0] >> 4) & 15, FDICT = (in[1] >> 5) & 1;
      if(CM != 8 || CINFO > 7) { return 25; } //error: only compression method 8: inflate with sliding window of 32k is supported by the PNG spec
      if(FDICT != 0) { return 26; } //error: the specification of PNG says about the zlib stream: "The additional flags shall not specify a preset dictionary."
      inflator.inflate(out, in, 2);
      return inflator.error; //note: adler32 checksum was skipped and ignored
    }
  };
  struct PNG //nested functions for PNG decoding
  {
    struct Info
    {
      unsigned long width, height, colorType, bitDepth, compressionMethod, filterMethod, interlaceMethod, key_r, key_g, key_b;
      bool key_defined; //is a transparent color key given?
      std::vector<unsigned char> palette;
    } info;
    int error;
    void decode(std::vector<unsigned char>& out, const unsigned char* in, size_t size, bool convert_to_rgba32)
    {
      error = 0;
      if(size == 0 || in == 0) { error = 48; return; } //the given data is empty
      readPngHeader(&in[0], size); if(error) return;
      size_t pos = 33; //first byte of the first chunk after the header
      std::vector<unsigned char> idat; //the data from idat chunks
      bool IEND = false, known_type = true;
      info.key_defined = false;
      while(!IEND) //loop through the chunks, ignoring unknown chunks and stopping at IEND chunk. IDAT data is put at the start of the in buffer
      {
        if(pos + 8 >= size) { error = 30; return; } //error: size of the in buffer too small to contain next chunk
        size_t chunkLength = read32bitInt(&in[pos]); pos += 4;
        if(chunkLength > 2147483647) { error = 63; return; }
        if(pos + chunkLength >= size) { error = 35; return; } //error: size of the in buffer too small to contain next chunk
        if(in[pos + 0] == 'I' && in[pos + 1] == 'D' && in[pos + 2] == 'A' && in[pos + 3] == 'T') //IDAT chunk, containing compressed image data
        {
          idat.insert(idat.end(), &in[pos + 4], &in[pos + 4 + chunkLength]);
          pos += (4 + chunkLength);
        }
        else if(in[pos + 0] == 'I' && in[pos + 1] == 'E' && in[pos + 2] == 'N' && in[pos + 3] == 'D')  { pos += 4; IEND = true; }
        else if(in[pos + 0] == 'P' && in[pos + 1] == 'L' && in[pos + 2] == 'T' && in[pos + 3] == 'E') //palette chunk (PLTE)
        {
          pos += 4; //go after the 4 letters
          info.palette.resize(4 * (chunkLength / 3));
          if(info.palette.size() > (4 * 256)) { error = 38; return; } //error: palette too big
          for(size_t i = 0; i < info.palette.size(); i += 4)
          {
            for(size_t j = 0; j < 3; j++) info.palette[i + j] = in[pos++]; //RGB
            info.palette[i + 3] = 255; //alpha
          }
        }
        else if(in[pos + 0] == 't' && in[pos + 1] == 'R' && in[pos + 2] == 'N' && in[pos + 3] == 'S') //palette transparency chunk (tRNS)
        {
          pos += 4; //go after the 4 letters
          if(info.colorType == 3)
          {
            if(4 * chunkLength > info.palette.size()) { error = 39; return; } //error: more alpha values given than there are palette entries
            for(size_t i = 0; i < chunkLength; i++) info.palette[4 * i + 3] = in[pos++];
          }
          else if(info.colorType == 0)
          {
            if(chunkLength != 2) { error = 40; return; } //error: this chunk must be 2 bytes for greyscale image
            info.key_defined = 1; info.key_r = info.key_g = info.key_b = 256 * in[pos] + in[pos + 1]; pos += 2;
          }
          else if(info.colorType == 2)
          {
            if(chunkLength != 6) { error = 41; return; } //error: this chunk must be 6 bytes for RGB image
            info.key_defined = 1;
            info.key_r = 256 * in[pos] + in[pos + 1]; pos += 2;
            info.key_g = 256 * in[pos] + in[pos + 1]; pos += 2;
            info.key_b = 256 * in[pos] + in[pos + 1]; pos += 2;
          }
          else { error = 42; return; } //error: tRNS chunk not allowed for other color models
        }
        else //it's not an implemented chunk type, so ignore it: skip over the data
        {
          if(!(in[pos + 0] & 32)) { error = 69; return; } //error: unknown critical chunk (5th bit of first byte of chunk type is 0)
          pos += (chunkLength + 4); //skip 4 letters and uninterpreted data of unimplemented chunk
          known_type = false;
        }
        pos += 4; //step over CRC (which is ignored)
      }
      unsigned long bpp = getBpp(info);
      std::vector<unsigned char> scanlines(((info.width * (info.height * bpp + 7)) / 8) + info.height); //now the out buffer will be filled
      Zlib zlib; //decompress with the Zlib decompressor
      error = zlib.decompress(scanlines, idat); if(error) return; //stop if the zlib decompressor returned an error
      size_t bytewidth = (bpp + 7) / 8, outlength = (info.height * info.width * bpp + 7) / 8;
      out.resize(outlength); //time to fill the out buffer
      unsigned char* out_ = outlength ? &out[0] : 0; //use a regular pointer to the std::vector for faster code if compiled without optimization
      if(info.interlaceMethod == 0) //no interlace, just filter
      {
        size_t linestart = 0, linelength = (info.width * bpp + 7) / 8; //length in bytes of a scanline, excluding the filtertype byte
        if(bpp >= 8) //byte per byte
        for(unsigned long y = 0; y < info.height; y++)
        {
          unsigned long filterType = scanlines[linestart];
          const unsigned char* prevline = (y == 0) ? 0 : &out_[(y - 1) * info.width * bytewidth];
          unFilterScanline(&out_[linestart - y], &scanlines[linestart + 1], prevline, bytewidth, filterType,  linelength); if(error) return;
          linestart += (1 + linelength); //go to start of next scanline
        }
        else //less than 8 bits per pixel, so fill it up bit per bit
        {
          std::vector<unsigned char> templine((info.width * bpp + 7) >> 3); //only used if bpp < 8
          for(size_t y = 0, obp = 0; y < info.height; y++)
          {
            unsigned long filterType = scanlines[linestart];
            const unsigned char* prevline = (y == 0) ? 0 : &out_[(y - 1) * info.width * bytewidth];
            unFilterScanline(&templine[0], &scanlines[linestart + 1], prevline, bytewidth, filterType, linelength); if(error) return;
            for(size_t bp = 0; bp < info.width * bpp;) setBitOfReversedStream(obp, out_, readBitFromReversedStream(bp, &templine[0]));
            linestart += (1 + linelength); //go to start of next scanline
          }
        }
      }
      else //interlaceMethod is 1 (Adam7)
      {
        size_t passw[7] = { (info.width + 7) / 8, (info.width + 3) / 8, (info.width + 3) / 4, (info.width + 1) / 4, (info.width + 1) / 2, (info.width + 0) / 2, (info.width + 0) / 1 };
        size_t passh[7] = { (info.height + 7) / 8, (info.height + 7) / 8, (info.height + 3) / 8, (info.height + 3) / 4, (info.height + 1) / 4, (info.height + 1) / 2, (info.height + 0) / 2 };
        size_t passstart[7] = {0};
        size_t pattern[28] = {0,4,0,2,0,1,0,0,0,4,0,2,0,1,8,8,4,4,2,2,1,8,8,8,4,4,2,2}; //values for the adam7 passes
        for(int i = 0; i < 6; i++) passstart[i + 1] = passstart[i] + passh[i] * ((passw[i] ? 1 : 0) + (passw[i] * bpp + 7) / 8);
        std::vector<unsigned char> scanlineo((info.width * bpp + 7) / 8), scanlinen((info.width * bpp + 7) / 8); //"old" and "new" scanline
        for(int i = 0; i < 7; i++)
          adam7Pass(&out_[0], &scanlinen[0], &scanlineo[0], &scanlines[passstart[i]], info.width, pattern[i], pattern[i + 7], pattern[i + 14], pattern[i + 21], passw[i], passh[i], bpp);
      }
      if(convert_to_rgba32 && (info.colorType != 6 || info.bitDepth != 8)) //conversion needed
      {
        std::vector<unsigned char> data = out;
        error = convert(out, &data[0], info, info.width, info.height);
      }
    }
    void readPngHeader(const unsigned char* in, size_t inlength) //read the information from the header and store it in the Info
    {
      if(inlength < 29) { error = 27; return; } //error: the data length is smaller than the length of the header
      if(in[0] != 137 || in[1] != 80 || in[2] != 78 || in[3] != 71 || in[4] != 13 || in[5] != 10 || in[6] != 26 || in[7] != 10) { error = 28; return; } //no PNG signature
      if(in[12] != 'I' || in[13] != 'H' || in[14] != 'D' || in[15] != 'R') { error = 29; return; } //error: it doesn't start with a IHDR chunk!
      info.width = read32bitInt(&in[16]); info.height = read32bitInt(&in[20]);
      info.bitDepth = in[24]; info.colorType = in[25];
      info.compressionMethod = in[26]; if(in[26] != 0) { error = 32; return; } //error: only compression method 0 is allowed in the specification
      info.filterMethod = in[27]; if(in[27] != 0) { error = 33; return; } //error: only filter method 0 is allowed in the specification
      info.interlaceMethod = in[28]; if(in[28] > 1) { error = 34; return; } //error: only interlace methods 0 and 1 exist in the specification
      error = checkColorValidity(info.colorType, info.bitDepth);
    }
    void unFilterScanline(unsigned char* recon, const unsigned char* scanline, const unsigned char* precon, size_t bytewidth, unsigned long filterType, size_t length)
    {
      switch(filterType)
      {
        case 0: for(size_t i = 0; i < length; i++) recon[i] = scanline[i]; break;
        case 1:
          for(size_t i =         0; i < bytewidth; i++) recon[i] = scanline[i];
          for(size_t i = bytewidth; i <    length; i++) recon[i] = scanline[i] + recon[i - bytewidth];
          break;
        case 2:
          if(precon) for(size_t i = 0; i < length; i++) recon[i] = scanline[i] + precon[i];
          else       for(size_t i = 0; i < length; i++) recon[i] = scanline[i];
          break;
        case 3:
          if(precon)
          {
            for(size_t i =         0; i < bytewidth; i++) recon[i] = scanline[i] + precon[i] / 2;
            for(size_t i = bytewidth; i <    length; i++) recon[i] = scanline[i] + ((recon[i - bytewidth] + precon[i]) / 2);
          }
          else
          {
            for(size_t i =         0; i < bytewidth; i++) recon[i] = scanline[i];
            for(size_t i = bytewidth; i <    length; i++) recon[i] = scanline[i] + recon[i - bytewidth] / 2;
          }
          break;
        case 4:
          if(precon)
          {
            for(size_t i =         0; i < bytewidth; i++) recon[i] = scanline[i] + paethPredictor(0, precon[i], 0);
            for(size_t i = bytewidth; i <    length; i++) recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], precon[i], precon[i - bytewidth]);
          }
          else
          {
            for(size_t i =         0; i < bytewidth; i++) recon[i] = scanline[i];
            for(size_t i = bytewidth; i <    length; i++) recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], 0, 0);
          }
          break;
        default: error = 36; return; //error: unexisting filter type given
      }
    }
    void adam7Pass(unsigned char* out, unsigned char* linen, unsigned char* lineo, const unsigned char* in, unsigned long w, size_t passleft, size_t passtop, size_t spacex, size_t spacey, size_t passw, size_t passh, unsigned long bpp)
    { //filter and reposition the pixels into the output when the image is Adam7 interlaced. This function can only do it after the full image is already decoded. The out buffer must have the correct allocated memory size already.
      if(passw == 0) return;
      size_t bytewidth = (bpp + 7) / 8, linelength = 1 + ((bpp * passw + 7) / 8);
      for(unsigned long y = 0; y < passh; y++)
      {
        unsigned char filterType = in[y * linelength], *prevline = (y == 0) ? 0 : lineo;
        unFilterScanline(linen, &in[y * linelength + 1], prevline, bytewidth, filterType, (w * bpp + 7) / 8); if(error) return;
        if(bpp >= 8) for(size_t i = 0; i < passw; i++) for(size_t b = 0; b < bytewidth; b++) //b = current byte of this pixel
          out[bytewidth * w * (passtop + spacey * y) + bytewidth * (passleft + spacex * i) + b] = linen[bytewidth * i + b];
        else for(size_t i = 0; i < passw; i++)
        {
          size_t obp = bpp * w * (passtop + spacey * y) + bpp * (passleft + spacex * i), bp = i * bpp;
          for(size_t b = 0; b < bpp; b++) setBitOfReversedStream(obp, out, readBitFromReversedStream(bp, &linen[0]));
        }
        unsigned char* temp = linen; linen = lineo; lineo = temp; //swap the two buffer pointers "line old" and "line new"
      }
    }
    static unsigned long readBitFromReversedStream(size_t& bitp, const unsigned char* bits) { unsigned long result = (bits[bitp >> 3] >> (7 - (bitp & 0x7))) & 1; bitp++; return result;}
    static unsigned long readBitsFromReversedStream(size_t& bitp, const unsigned char* bits, unsigned long nbits)
    {
      unsigned long result = 0;
      for(size_t i = nbits - 1; i < nbits; i--) result += ((readBitFromReversedStream(bitp, bits)) << i);
      return result;
    }
    void setBitOfReversedStream(size_t& bitp, unsigned char* bits, unsigned long bit) { bits[bitp >> 3] |=  (bit << (7 - (bitp & 0x7))); bitp++; }
    unsigned long read32bitInt(const unsigned char* buffer) { return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]; }
    int checkColorValidity(unsigned long colorType, unsigned long bd) //return type is a LodePNG error code
    {
      if((colorType == 2 || colorType == 4 || colorType == 6)) { if(!(bd == 8 || bd == 16)) return 37; else return 0; }
      else if(colorType == 0) { if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8 || bd == 16)) return 37; else return 0; }
      else if(colorType == 3) { if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8            )) return 37; else return 0; }
      else return 31; //unexisting color type
    }
    unsigned long getBpp(const Info& info)
    {
      if(info.colorType == 2) return (3 * info.bitDepth);
      else if(info.colorType >= 4) return (info.colorType - 2) * info.bitDepth;
      else return info.bitDepth;
    }
    int convert(std::vector<unsigned char>& out, const unsigned char* in, Info& infoIn, unsigned long w, unsigned long h)
    { //converts from any color type to 32-bit. return value = LodePNG error code
      size_t numpixels = w * h, bp = 0;
      out.resize(numpixels * 4);
      unsigned char* out_ = out.empty() ? 0 : &out[0]; //faster if compiled without optimization
      if(infoIn.bitDepth == 8 && infoIn.colorType == 0) //greyscale
      for(size_t i = 0; i < numpixels; i++)
      {
        out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[i];
        out_[4 * i + 3] = (infoIn.key_defined && in[i] == infoIn.key_r) ? 0 : 255;
      }
      else if(infoIn.bitDepth == 8 && infoIn.colorType == 2) //RGB color
      for(size_t i = 0; i < numpixels; i++)
      {
        for(size_t c = 0; c < 3; c++) out_[4 * i + c] = in[3 * i + c];
        out_[4 * i + 3] = (infoIn.key_defined == 1 && in[3 * i + 0] == infoIn.key_r && in[3 * i + 1] == infoIn.key_g && in[3 * i + 2] == infoIn.key_b) ? 0 : 255;
      }
      else if(infoIn.bitDepth == 8 && infoIn.colorType == 3) //indexed color (palette)
      for(size_t i = 0; i < numpixels; i++)
      {
        if(4U * in[i] >= infoIn.palette.size()) return 46;
        for(size_t c = 0; c < 4; c++) out_[4 * i + c] = infoIn.palette[4 * in[i] + c]; //get rgb colors from the palette
      }
      else if(infoIn.bitDepth == 8 && infoIn.colorType == 4) //greyscale with alpha
      for(size_t i = 0; i < numpixels; i++)
      {
        out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[2 * i + 0];
        out_[4 * i + 3] = in[2 * i + 1];
      }
      else if(infoIn.bitDepth == 8 && infoIn.colorType == 6) for(size_t i = 0; i < numpixels; i++) for(size_t c = 0; c < 4; c++) out_[4 * i + c] = in[4 * i + c]; //RGB with alpha
      else if(infoIn.bitDepth == 16 && infoIn.colorType == 0) //greyscale
      for(size_t i = 0; i < numpixels; i++)
      {
        out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[2 * i];
        out_[4 * i + 3] = (infoIn.key_defined && 256U * in[i] + in[i + 1] == infoIn.key_r) ? 0 : 255;
      }
      else if(infoIn.bitDepth == 16 && infoIn.colorType == 2) //RGB color
      for(size_t i = 0; i < numpixels; i++)
      {
        for(size_t c = 0; c < 3; c++) out_[4 * i + c] = in[6 * i + 2 * c];
        out_[4 * i + 3] = (infoIn.key_defined && 256U*in[6*i+0]+in[6*i+1] == infoIn.key_r && 256U*in[6*i+2]+in[6*i+3] == infoIn.key_g && 256U*in[6*i+4]+in[6*i+5] == infoIn.key_b) ? 0 : 255;
      }
      else if(infoIn.bitDepth == 16 && infoIn.colorType == 4) //greyscale with alpha
      for(size_t i = 0; i < numpixels; i++)
      {
        out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[4 * i]; //most significant byte
        out_[4 * i + 3] = in[4 * i + 2];
      }
      else if(infoIn.bitDepth == 16 && infoIn.colorType == 6) for(size_t i = 0; i < numpixels; i++) for(size_t c = 0; c < 4; c++) out_[4 * i + c] = in[8 * i + 2 * c]; //RGB with alpha
      else if(infoIn.bitDepth < 8 && infoIn.colorType == 0) //greyscale
      for(size_t i = 0; i < numpixels; i++)
      {
        unsigned long value = (readBitsFromReversedStream(bp, in, infoIn.bitDepth) * 255) / ((1 << infoIn.bitDepth) - 1); //scale value from 0 to 255
        out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = (unsigned char)(value);
        out_[4 * i + 3] = (infoIn.key_defined && value && ((1U << infoIn.bitDepth) - 1U) == infoIn.key_r && ((1U << infoIn.bitDepth) - 1U)) ? 0 : 255;
      }
      else if(infoIn.bitDepth < 8 && infoIn.colorType == 3) //palette
      for(size_t i = 0; i < numpixels; i++)
      {
        unsigned long value = readBitsFromReversedStream(bp, in, infoIn.bitDepth);
        if(4 * value >= infoIn.palette.size()) return 47;
        for(size_t c = 0; c < 4; c++) out_[4 * i + c] = infoIn.palette[4 * value + c]; //get rgb colors from the palette
      }
      return 0;
    }
    unsigned char paethPredictor(short a, short b, short c) //Paeth predicter, used by PNG filter type 4
    {
      short p = a + b - c, pa = p > a ? (p - a) : (a - p), pb = p > b ? (p - b) : (b - p), pc = p > c ? (p - c) : (c - p);
      return (unsigned char)((pa <= pb && pa <= pc) ? a : pb <= pc ? b : c);
    }
  };
  PNG decoder; decoder.decode(out_image, in_png, in_size, true);
  image_width = decoder.info.width; image_height = decoder.info.height;
  return decoder.error;
}

#ifdef __cplusplus
extern "C" {
#endif
/* Copyright 2010-2020 <>< Charles Lohr and other various authors as attributed.
	Licensed under the MIT/x11 or NewBSD License you choose.
	CN Foundational Graphics Main Header File.  This is the main header you
	should include.  See README.md for more details.
*/
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#define CNFGOGL_NEED_EXTENSION
#ifndef _CNFG_H
#define _CNFG_H
/* Rawdraw flags:
	CNFG3D -> Enable the weird 3D functionality that rawdraw has to allow you to
		write apps which emit basic rawdraw primitives but look 3D!
		CNFG_USE_DOUBLE_FUNCTIONS -> Use double-precision floating point for CNFG3D.
	CNFGOGL -> Use an OpenGL Backend for all rawdraw functionality.
		->Caveat->If using CNFG_HAS_XSHAPE, then, we do something realy wacky.
	CNFGRASTERIZER -> Software-rasterize the rawdraw calls, and, use
		CNFGUpdateScreenWithBitmap to send video to webpage.
	CNFGCONTEXTONLY -> Don't add any drawing functions, only opening a window to
		get an OpenGL context.
		
Usually tested combinations:
 * TCC On Windows and X11 (Linux) with:
    - CNFGOGL on or CNFGOGL off.  If CNFGOGL is off you can use
			CNFG_WINDOWS_DISABLE_BATCH to disable all batching.
		-or-
	- CNFGRASTERIZER
	NOTE: Sometimes you can also use CNFGOGL + CNFGRASTERIZER
 * WASM driver supports both: CNFGRASTERIZER and without CNFGRASTERIZER (Recommended turn rasterizer off)
 * ANDROID (But this automatically sets CNFGRASTERIZER OFF and CNFGOGL ON)
*/
#include <stdint.h>
//Some per-platform logic.
#if defined( ANDROID ) || defined( __android__ )
	#define CNFGOGL
#endif
#if ( defined( CNFGOGL ) || defined( __wasm__ ) ) && !defined(CNFG_HAS_XSHAPE)
	#define CNFG_BATCH 8192 //131,072 bytes.
	#if defined( ANDROID ) || defined( __android__ ) || defined( __wasm__ ) || defined( EGL_LEAN_AND_MEAN )
		#define CNFGEWGL //EGL or WebGL
	#else
		#define CNFGDESKTOPGL
	#endif
#endif
typedef struct {
    short x, y; 
} RDPoint; 
extern int CNFGPenX, CNFGPenY;
extern uint32_t CNFGBGColor;
extern uint32_t CNFGLastColor;
void CNFGDrawText( const char * text, short scale );
void CNFGGetTextExtents( const char * text, int * w, int * h, int textsize  );
//To be provided by driver. Rawdraw uses colors in the format 0xRRGGBBAA
//Note that some backends do not support alpha of any kind.
//Some platforms also support alpha blending.  So, be sure to set alpha to 0xFF
uint32_t CNFGColor( uint32_t RGBA );
//This both updates the screen, and flips, all as a single operation.
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h );
//This is only supported on a FEW architectures, but allows arbitrary
//image blitting.  Note that the alpha channel behavior is different
//on different systems.
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h );
void CNFGTackPixel( short x1, short y1 );
void CNFGTackSegment( short x1, short y1, short x2, short y2 );
void CNFGTackRectangle( short x1, short y1, short x2, short y2 );
void CNFGTackPoly( RDPoint * points, int verts );
void CNFGClearFrame();
void CNFGSwapBuffers();
void CNFGGetDimensions( short * x, short * y );
int CNFGSetup( const char * WindowName, int w, int h ); //return 0 if ok.
void CNFGSetupFullscreen( const char * WindowName, int screen_number );
void CNFGHandleInput();
//You must provide:
void HandleKey( int keycode, int bDown );
void HandleButton( int x, int y, int button, int bDown );
void HandleMotion( int x, int y, int mask );
void HandleDestroy();
//Internal function for resizing rasterizer for rasterizer-mode.
void CNFGInternalResize( short x, short y ); //don't call this.
//Not available on all systems.  Use The OGL portion with care.
#ifdef CNFGOGL
void   CNFGSetVSync( int vson );
void * CNFGGetExtension( const char * extname );
#endif
//Also not available on all systems.  Transparency.
void	CNFGPrepareForTransparency();
void	CNFGDrawToTransparencyMode( int transp );
void	CNFGClearTransparencyLevel();
//Only available on systems that support it.
void	CNFGSetLineWidth( short width );
void	CNFGChangeWindowTitle( const char * windowtitle );
void	CNFGSetWindowIconData( int w, int h, uint32_t * data );
int 	CNFGSetupWMClass( const char * WindowName, int w, int h , char * wm_res_name_ , char * wm_res_class_ );
//If you're using a batching renderer, for instance on Android or an OpenGL
//You will need to call this function inbetewen swtiching properties of drawing.  This is usually
//only needed if you calling OpenGL / OGLES functions directly and outside of CNFG.
//
//Note that these are the functions that are used on the backends which support this
//sort of thing.
#ifdef CNFG_BATCH
//If you are not using the CNFGOGL driver, you will need to define these in your driver.
void	CNFGEmitBackendTriangles( const float * vertices, const uint32_t * colors, int num_vertices );
void	CNFGBlitImage( uint32_t * data, int x, int y, int w, int h );
//These need to be defined for the specific driver.  
void 	CNFGClearFrame();
void 	CNFGSwapBuffers();
void 	CNFGFlushRender(); //Emit any geometry (lines, squares, polys) which are slated to be rendered.
void	CNFGInternalResize( short x, short y ); //Driver calls this after resize happens.
void	CNFGSetupBatchInternal(); //Driver calls this after setup is complete.
//Useful function for emitting a non-axis-aligned quad.
void 	CNFGEmitQuad( float cx0, float cy0, float cx1, float cy1, float cx2, float cy2, float cx3, float cy3 );
extern int 	CNFGVertPlace;
extern float CNFGVertDataV[CNFG_BATCH*3];
extern uint32_t CNFGVertDataC[CNFG_BATCH];
#endif
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#define CNFG_KEY_SHIFT 0x10
#define CNFG_KEY_BACKSPACE 0x08
#define CNFG_KEY_DELETE 0x2E
#define CNFG_KEY_LEFT_ARROW 0x25
#define CNFG_KEY_RIGHT_ARROW 0x27
#define CNFG_KEY_TOP_ARROW 0x26
#define CNFG_KEY_BOTTOM_ARROW 0x28
#define CNFG_KEY_ESCAPE 0x1B
#define CNFG_KEY_ENTER 0x0D
#elif defined( EGL_LEAN_AND_MEAN ) // doesn't have any keys
#elif defined( __android__ ) || defined( ANDROID ) // ^
#elif defined( __wasm__ )
#define CNFG_KEY_SHIFT 16
#define CNFG_KEY_BACKSPACE 8
#define CNFG_KEY_DELETE 46
#define CNFG_KEY_LEFT_ARROW 37
#define CNFG_KEY_RIGHT_ARROW 39
#define CNFG_KEY_TOP_ARROW 38
#define CNFG_KEY_BOTTOM_ARROW 40
#define CNFG_KEY_ESCAPE 27
#define CNFG_KEY_ENTER 13
#else // most likely x11
#define CNFG_KEY_SHIFT 65505
#define CNFG_KEY_BACKSPACE 65288
#define CNFG_KEY_DELETE 65535
#define CNFG_KEY_LEFT_ARROW 65361
#define CNFG_KEY_RIGHT_ARROW 65363
#define CNFG_KEY_TOP_ARROW 65362
#define CNFG_KEY_BOTTOM_ARROW 65364
#define CNFG_KEY_ESCAPE 65307
#define CNFG_KEY_ENTER 65293
#endif
#ifdef CNFG3D
#ifndef __wasm__
#include <math.h>
#endif
#ifdef CNFG_USE_DOUBLE_FUNCTIONS
#define tdCOS cos
#define tdSIN sin
#define tdTAN tan
#define tdSQRT sqrt
#else
#define tdCOS cosf
#define tdSIN sinf
#define tdTAN tanf
#define tdSQRT sqrtf
#endif
#ifdef __wasm__
void tdMATCOPY( float * x, const float * y ); //Copy y into x
#else
#define tdMATCOPY(x,y) memcpy( x, y, 16*sizeof(float))
#endif
#define tdQ_PI 3.141592653589
#define tdDEGRAD (tdQ_PI/180.)
#define tdRADDEG (180./tdQ_PI)
//General Matrix Functions
void tdIdentity( float * f );
void tdZero( float * f );
void tdTranslate( float * f, float x, float y, float z );		//Operates ON f
void tdScale( float * f, float x, float y, float z );			//Operates ON f
void tdRotateAA( float * f, float angle, float x, float y, float z ); 	//Operates ON f
void tdRotateQuat( float * f, float qw, float qx, float qy, float qz ); 	//Operates ON f
void tdRotateEA( float * f, float x, float y, float z );		//Operates ON f
void tdMultiply( float * fin1, float * fin2, float * fout );		//Operates ON f
void tdPrint( const float * f );
void tdTransposeSelf( float * f );
//Specialty Matrix Functions
void tdPerspective( float fovy, float aspect, float zNear, float zFar, float * out ); //Sets, NOT OPERATES. (FOVX=degrees)
void tdLookAt( float * m, float * eye, float * at, float * up );	//Operates ON m
//General point functions
#define tdPSet( f, x, y, z ) { f[0] = x; f[1] = y; f[2] = z; }
void tdPTransform( const float * pin, float * f, float * pout );
void tdVTransform( const float * vin, float * f, float * vout );
void td4Transform( float * kin, float * f, float * kout );
void td4RTransform( float * kin, float * f, float * kout );
void tdNormalizeSelf( float * vin );
void tdCross( float * va, float * vb, float * vout );
float tdDistance( float * va, float * vb );
float tdDot( float * va, float * vb );
#define tdPSub( x, y, z ) { (z)[0] = (x)[0] - (y)[0]; (z)[1] = (x)[1] - (y)[1]; (z)[2] = (x)[2] - (y)[2]; }
#define tdPAdd( x, y, z ) { (z)[0] = (x)[0] + (y)[0]; (z)[1] = (x)[1] + (y)[1]; (z)[2] = (x)[2] + (y)[2]; }
//Stack Functionality
#define tdMATRIXMAXDEPTH 32
extern float * gSMatrix;
void tdPush();
void tdPop();
void tdMode( int mode );
#define tdMODELVIEW 0
#define tdPROJECTION 1
//Final stage tools
void tdSetViewport( float leftx, float topy, float rightx, float bottomy, float pixx, float pixy );
void tdFinalPoint( float * pin, float * pout );
float tdNoiseAt( int x, int y );
float tdFLerp( float a, float b, float t );
float tdPerlin2D( float x, float y );
#endif
extern const unsigned char RawdrawFontCharData[1405];
extern const unsigned short RawdrawFontCharMap[256];
#ifdef CNFG_IMPLEMENTATION
//Include this file to get all of rawdraw.  You usually will not
//want to include this in your build, but instead, #include "CNFG.h"
//after #define CNFG_IMPLEMENTATION in one of your C files.
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
//Copyright (c) 2011-2019 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
//Portion from: http://en.wikibooks.org/wiki/Windows_Programming/Window_Creation
#ifndef _CNFGWINDRIVER_C
#define _CNFGWINDRIVER_C
#include <windows.h>
#include <stdlib.h>
#include <malloc.h> //for alloca
#include <ctype.h>
static HBITMAP lsBitmap;
static HWND lsHWND;
static HDC lsWindowHDC;
static HDC lsHDC;
static HDC lsHDCBlit;
//Queue up lines and points for a faster render.
#ifndef CNFG_WINDOWS_DISABLE_BATCH
#define BATCH_ELEMENTS
#endif
#define COLORSWAPS( RGB ) \
		((((RGB )& 0xFF000000)>>24) | ( ((RGB )& 0xFF0000 ) >> 8 ) | ( ((RGB )& 0xFF00 )<<8 ))
void CNFGChangeWindowTitle( const char * windowtitle )
{
	SetWindowTextA( lsHWND, windowtitle );
}
#ifdef CNFGRASTERIZER
//Don't call this file yourself.  It is intended to be included in any drivers which want to support the rasterizer plugin.
#ifdef CNFGRASTERIZER
//#include <stdlib.h>
#include <stdint.h>
static uint32_t * buffer = 0;
static short bufferx;
static short buffery;
#ifdef CNFGOGL
void CNFGFlushRender()
{
}
#endif
void CNFGInternalResize( short x, short y )
{
	bufferx = x;
	buffery = y;
	if( buffer ) free( buffer );
	buffer = malloc( bufferx * buffery * 4 );
#ifdef CNFGOGL
	void CNFGInternalResizeOGLBACKEND( short w, short h );
	CNFGInternalResizeOGLBACKEND( x, y );
#endif
}
#ifdef __wasm__
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<24;
	r>>=8;
	ret |= (r&0xff)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff)<<0;
	return ret;
}
#elif !defined(CNFGOGL)
#define SWAPS(x) (x>>8)
#else
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff);
	r>>=8;
	ret |= (r&0xff)<<24;
	return ret;
}
#endif
uint32_t CNFGColor( uint32_t RGB )
{
	CNFGLastColor = SWAPS(RGB);
	return CNFGLastColor;
}
void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	short tx, ty;
	float slope, lp;
	short dx = x2 - x1;
	short dy = y2 - y1;
	if( !buffer ) return;
	if( dx < 0 ) dx = -dx;
	if( dy < 0 ) dy = -dy;
	if( dx > dy )
	{
		short minx = (x1 < x2)?x1:x2;
		short maxx = (x1 < x2)?x2:x1;
		short miny = (x1 < x2)?y1:y2;
		short maxy = (x1 < x2)?y2:y1;
		float thisy = miny;
		slope = (float)(maxy-miny) / (float)(maxx-minx);
		for( tx = minx; tx <= maxx; tx++ )
		{
			ty = thisy;
			if( tx < 0 || ty < 0 || ty >= buffery ) continue;
			if( tx >= bufferx ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisy += slope;
		}
	}
	else
	{
		short minx = (y1 < y2)?x1:x2;
		short maxx = (y1 < y2)?x2:x1;
		short miny = (y1 < y2)?y1:y2;
		short maxy = (y1 < y2)?y2:y1;
		float thisx = minx;
		slope = (float)(maxx-minx) / (float)(maxy-miny);
		for( ty = miny; ty <= maxy; ty++ )
		{
			tx = thisx;
			if( ty < 0 || tx < 0 || tx >= bufferx ) continue;
			if( ty >= buffery ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisx += slope;
		}
	}
}
void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	short minx = (x1<x2)?x1:x2;
	short miny = (y1<y2)?y1:y2;
	short maxx = (x1>=x2)?x1:x2;
	short maxy = (y1>=y2)?y1:y2;
	short x, y;
	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx >= bufferx ) maxx = bufferx-1;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		uint32_t * bufferstart = &buffer[y * bufferx + minx];
		for( x = minx; x <= maxx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
}
void CNFGTackPoly( RDPoint * points, int verts )
{
	short minx = 10000, miny = 10000;
	short maxx =-10000, maxy =-10000;
	short i, x, y;
	//Just in case...
	if( verts > 32767 ) return;
	for( i = 0; i < verts; i++ )
	{
		RDPoint * p = &points[i];
		if( p->x < minx ) minx = p->x;
		if( p->y < miny ) miny = p->y;
		if( p->x > maxx ) maxx = p->x;
		if( p->y > maxy ) maxy = p->y;
	}
	if( miny < 0 ) miny = 0;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		short startfillx = maxx;
		short endfillx = minx;
		//Figure out what line segments intersect this line.
		for( i = 0; i < verts; i++ )
		{
			short pl = i + 1;
			if( pl == verts ) pl = 0;
			RDPoint ptop;
			RDPoint pbot;
			ptop.x = points[i].x;
			ptop.y = points[i].y;
			pbot.x = points[pl].x;
			pbot.y = points[pl].y;
//printf( "Poly: %d %d\n", pbot.y, ptop.y );
			if( pbot.y < ptop.y )
			{
				RDPoint ptmp;
				ptmp.x = pbot.x;
				ptmp.y = pbot.y;
				pbot.x = ptop.x;
				pbot.y = ptop.y;
				ptop.x = ptmp.x;
				ptop.y = ptmp.y;
			}
			//Make sure this line segment is within our range.
//printf( "PT: %d %d %d\n", y, ptop.y, pbot.y );
			if( ptop.y <= y && pbot.y >= y )
			{
				short diffy = pbot.y - ptop.y;
				uint32_t placey = (uint32_t)(y - ptop.y)<<16;  //Scale by 16 so we can do integer math.
				short diffx = pbot.x - ptop.x;
				short isectx;
				if( diffy == 0 )
				{
					if( pbot.x < ptop.x )
					{
						if( startfillx > pbot.x ) startfillx = pbot.x;
						if( endfillx < ptop.x ) endfillx = ptop.x;
					}
					else
					{
						if( startfillx > ptop.x ) startfillx = ptop.x;
						if( endfillx < pbot.x ) endfillx = pbot.x;
					}
				}
				else
				{
					//Inner part is scaled by 65536, outer part must be scaled back.
					isectx = (( (placey / diffy) * diffx + 32768 )>>16) + ptop.x;
					if( isectx < startfillx ) startfillx = isectx;
					if( isectx > endfillx ) endfillx = isectx;
				}
//printf( "R: %d %d %d\n", pbot.x, ptop.x, isectx );
			}
		}
//printf( "%d %d %d\n", y, startfillx, endfillx );
		if( endfillx >= bufferx ) endfillx = bufferx - 1;
		if( endfillx >= bufferx ) endfillx = buffery - 1;
		if( startfillx < 0 ) startfillx = 0;
		if( startfillx < 0 ) startfillx = 0;
		unsigned int * bufferstart = &buffer[y * bufferx + startfillx];
		for( x = startfillx; x <= endfillx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
//exit(1);
}
void CNFGClearFrame()
{
	int i, m;
	uint32_t col = 0;
	short x, y;
	CNFGGetDimensions( &x, &y );
	if( x != bufferx || y != buffery || !buffer )
	{
		bufferx = x;
		buffery = y;
		buffer = malloc( x * y * 8 );
	}
	m = x * y;
	col = CNFGColor( CNFGBGColor );
	for( i = 0; i < m; i++ )
	{
//printf( "Got: %d %p %d\n", m, buffer, i );
		buffer[i] = col;
	}
}
void CNFGTackPixel( short x, short y )
{
	if( x < 0 || y < 0 || x >= bufferx || y >= buffery ) return;
	buffer[x+bufferx*y] = CNFGLastColor;
}
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	int ox = x;
	int stride = w;
	if( w <= 0 || h <= 0 || x >= bufferx || y >= buffery ) return;
	if( x < 0 ) { w += x; x = 0; }
	if( y < 0 ) { h += y; y = 0; }
	//Switch w,h to x2, y2
	h += y;
	w += x;
	if( w >= bufferx ) { w = bufferx; }
	if( h >= buffery ) { h = buffery; }
	for( ; y < h-1; y++ )
	{
		x = ox;
		uint32_t * indat = data;
		uint32_t * outdat = buffer + y * bufferx + x;
		for( ; x < w-1; x++ )
		{
			uint32_t newm = *(indat++);
			uint32_t oldm = *(outdat);
			if( (newm & 0xff) == 0xff )
			{
				*(outdat++) = newm;
			}
			else
			{
				//Alpha blend.
				int alfa = newm&0xff;
				int onemalfa = 255-alfa;
#ifdef __wasm__
				uint32_t newv = 255<<0; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#elif defined( ANDROID ) || defined( __android__ )
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined( CNFGOGL ) //OGL, on X11
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#else //X11
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#endif
				*(outdat++) = newv;
			}
		}
		data += stride;
	}
}
void CNFGSwapBuffers()
{
	CNFGUpdateScreenWithBitmap( (uint32_t*)buffer, bufferx, buffery );
}
#endif
void InternalHandleResize()
{
	if( lsBitmap ) LOADED_DeleteObject( lsBitmap );
	CNFGInternalResize( bufferx, buffery );
	lsBitmap = LOADED_CreateBitmap( bufferx, buffery, 1, 32, buffer );
	LOADED_SelectObject( lsHDC, lsBitmap );
	CNFGInternalResize( bufferx, buffery);
}
#else
static short bufferx, buffery;
static void InternalHandleResize();
#endif
#ifdef CNFGOGL
#include <GL/gl.h>
// gdi32.dll
HGDIOBJ (*LOADED_SelectObject)(HDC, HGDIOBJ);
LONG (*LOADED_SetBitmapBits)(HBITMAP, DWORD, const VOID*);
BOOL (*LOADED_BitBlt)(HDC, int, int, int, int, HDC, int, int, DWORD);
HDC (*LOADED_CreateCompatibleDC)(HDC);
HBITMAP (*LOADED_CreateCompatibleBitmap)(HDC, int, int);
BOOL (*LOADED_DeleteObject)(HGDIOBJ);
BOOL (*LOADED_PolyPolyline)(HDC, const POINT*, const DWORD*, DWORD);
BOOL (*LOADED_PolyPolygon)(HDC, const POINT*, const INT*, int);
COLORREF (*LOADED_SetPixel)(HDC, int, int, COLORREF);
HBRUSH (*LOADED_CreateSolidBrush)(COLORREF);
HPEN (*LOADED_CreatePen)(int, int, COLORREF);
HBITMAP (*LOADED_CreateBitmap)(int, int, UINT, UINT, const VOID*);
BOOL (*LOADED_Polygon)(HDC, const POINT*, int);
BOOL (*LOADED_SwapBuffers)( HDC Arg1);
int (*LOADED_ChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR*);
BOOL (*LOADED_SetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR *);
// opengl32.dll
HGLRC (*LOADED_wglCreateContext)(HDC);
HGLRC (*LOADED_wglGetCurrentContext)();
BOOL (*LOADED_wglMakeCurrent)(HDC, HGLRC);
PROC (*LOADED_wglGetProcAddress)(LPCSTR);
GLenum (WINAPI*LOADED_glGetError)();
void (WINAPI*LOADED_glGenTextures)(GLsizei, GLuint*);
void (WINAPI*LOADED_glDisable)(GLenum);
void (WINAPI*LOADED_glDepthMask)(GLboolean);
void (WINAPI*LOADED_glEnable)(GLenum);
void (WINAPI*LOADED_glBlendFunc)(GLenum, GLenum);
void (WINAPI*LOADED_glViewport)(GLint, GLint, GLsizei, GLsizei);
void (WINAPI*LOADED_glDrawArrays)(GLenum, GLint, GLsizei);
void (WINAPI*LOADED_glBindTexture)(GLenum, GLuint);
void (WINAPI*LOADED_glTexParameteri)(GLenum, GLenum, GLint);
void (WINAPI*LOADED_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLint, GLenum, const GLvoid*);
void (WINAPI*LOADED_glClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
void (WINAPI*LOADED_glClear)(GLbitfield);
void load_functions() {
    HMODULE lib_gdi32 = LoadLibrary("gdi32.dll");
    LOADED_SelectObject = (HGDIOBJ (*)(HDC, HGDIOBJ))GetProcAddress(lib_gdi32, "SelectObject");
    LOADED_SetBitmapBits = (LONG (*)(HBITMAP, DWORD, const VOID*))GetProcAddress(lib_gdi32, "SetBitmapBits");
    LOADED_BitBlt = (BOOL (*)(HDC, int, int, int, int, HDC, int, int, DWORD))GetProcAddress(lib_gdi32, "BitBlt");
    LOADED_CreateCompatibleDC = (HDC (*)(HDC))GetProcAddress(lib_gdi32, "CreateCompatibleDC");
    LOADED_CreateCompatibleBitmap = (HBITMAP (*)(HDC, int, int))GetProcAddress(lib_gdi32, "CreateCompatibleBitmap");
    LOADED_DeleteObject = (BOOL (*)(HGDIOBJ))GetProcAddress(lib_gdi32, "DeleteObject");
    LOADED_PolyPolyline = (BOOL (*)(HDC, const POINT*, const DWORD*, DWORD))GetProcAddress(lib_gdi32, "PolyPolyline");
    LOADED_PolyPolygon = (BOOL (*)(HDC, const POINT*, const INT*, int))GetProcAddress(lib_gdi32, "PolyPolygon");
    LOADED_SetPixel = (COLORREF (*)(HDC, int, int, COLORREF))GetProcAddress(lib_gdi32, "SetPixel");
    LOADED_CreateSolidBrush = (HBRUSH (*)(COLORREF))GetProcAddress(lib_gdi32, "CreateSolidBrush");
    LOADED_CreatePen = (HPEN (*)(int, int, COLORREF))GetProcAddress(lib_gdi32, "CreatePen");
    LOADED_CreateBitmap = (HBITMAP (*)(int, int, UINT, UINT, const VOID*))GetProcAddress(lib_gdi32, "CreateBitmap");
    LOADED_Polygon = (BOOL (*)(HDC, const POINT*, int))GetProcAddress(lib_gdi32, "Polygon");
    LOADED_SwapBuffers = (BOOL (*)( HDC Arg1))GetProcAddress(lib_gdi32, "SwapBuffers");
    LOADED_ChoosePixelFormat = (int (*)(HDC, const PIXELFORMATDESCRIPTOR*))GetProcAddress(lib_gdi32, "ChoosePixelFormat");
    LOADED_SetPixelFormat = (BOOL (*)(HDC, int, const PIXELFORMATDESCRIPTOR *))GetProcAddress(lib_gdi32, "SetPixelFormat");
    HMODULE lib_opengl32 = LoadLibrary("opengl32.dll");
    LOADED_wglCreateContext = (HGLRC (*)(HDC))GetProcAddress(lib_opengl32, "wglCreateContext");
    LOADED_wglGetCurrentContext = (HGLRC (*)())GetProcAddress(lib_opengl32, "wglGetCurrentContext");
    LOADED_wglMakeCurrent = (BOOL (*)(HDC, HGLRC))GetProcAddress(lib_opengl32, "wglMakeCurrent");
    LOADED_wglGetProcAddress = (PROC (WINAPI*)(LPCSTR))GetProcAddress(lib_opengl32, "wglGetProcAddress");
    LOADED_glGetError = (GLenum (WINAPI*)(void))GetProcAddress(lib_opengl32, "glGetError");
    LOADED_glGenTextures = (void (WINAPI*)(GLsizei, GLuint*))GetProcAddress(lib_opengl32, "glGenTextures");
    LOADED_glDisable = (void (WINAPI*)(GLenum))GetProcAddress(lib_opengl32, "glDisable");
    LOADED_glDepthMask = (void (WINAPI*)(GLboolean))GetProcAddress(lib_opengl32, "glDepthMask");
    LOADED_glEnable = (void (WINAPI*)(GLenum))GetProcAddress(lib_opengl32, "glEnable");
    LOADED_glBlendFunc = (void (WINAPI*)(GLenum, GLenum))GetProcAddress(lib_opengl32, "glBlendFunc");
    LOADED_glViewport = (void (WINAPI*)(GLint, GLint, GLsizei, GLsizei))GetProcAddress(lib_opengl32, "glViewport");
    LOADED_glDrawArrays = (void (WINAPI*)(GLenum, GLint, GLsizei))GetProcAddress(lib_opengl32, "glDrawArrays");
    LOADED_glBindTexture = (void (WINAPI*)(GLenum, GLuint))GetProcAddress(lib_opengl32, "glBindTexture");
    LOADED_glTexParameteri = (void (WINAPI*)(GLenum, GLenum, GLint))GetProcAddress(lib_opengl32, "glTexParameteri");
    LOADED_glTexImage2D = (void (WINAPI*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLint, GLenum, const GLvoid*))GetProcAddress(lib_opengl32, "glTexImage2D");
    LOADED_glClearColor = (void (WINAPI*)(GLclampf, GLclampf, GLclampf, GLclampf))GetProcAddress(lib_opengl32, "glClearColor");
    LOADED_glClear = (void (WINAPI*)(GLbitfield))GetProcAddress(lib_opengl32, "glClear");
}
static HGLRC           hRC=NULL; 
static void InternalHandleResize() { }
void CNFGSwapBuffers()
{
#ifdef CNFG_BATCH
	CNFGFlushRender();
#endif
	LOADED_SwapBuffers(lsWindowHDC);
}
#endif
void CNFGGetDimensions( short * x, short * y )
{
	static short lastx, lasty;
	RECT window;
	GetClientRect( lsHWND, &window );
	bufferx = (short)( window.right - window.left);
	buffery = (short)( window.bottom - window.top);
	if( bufferx != lastx || buffery != lasty )
	{
		lastx = bufferx;
		lasty = buffery;
		CNFGInternalResize( lastx, lasty );
		InternalHandleResize();
	}
	*x = bufferx;
	*y = buffery;
}
#ifndef CNFGOGL
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h )
{
	RECT r;
	LOADED_SelectObject( lsHDC, lsBitmap );
	LOADED_SetBitmapBits(lsBitmap,w*h*4,data);
	LOADED_BitBlt(lsWindowHDC, 0, 0, w, h, lsHDC, 0, 0, SRCCOPY);
	UpdateWindow( lsHWND );
	short thisw, thish;
	//Check to see if the window is closed.
	if( !IsWindow( lsHWND ) )
	{
		exit( 0 );
	}
	GetClientRect( lsHWND, &r );
	thisw = (short)(r.right - r.left);
	thish = (short)(r.bottom - r.top);
	if( thisw != bufferx || thish != buffery )
	{
		bufferx = thisw;
		buffery = thish;
		InternalHandleResize();
	}
}
#endif
void CNFGTearDown()
{
	PostQuitMessage(0);
#ifdef CNFGOGL
	exit(0);
#endif
}
//This was from the article
LRESULT CALLBACK MyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
#ifndef CNFGOGL
	case WM_SYSCOMMAND:  //Not sure why, if deactivated, the dc gets unassociated?
		if( wParam == SC_RESTORE || wParam == SC_MAXIMIZE || wParam == SC_SCREENSAVE )
		{
			LOADED_SelectObject( lsHDC, lsBitmap );
			LOADED_SelectObject( lsWindowHDC, lsBitmap );
		}
		break;
#endif
	case WM_DESTROY:
		HandleDestroy();
		CNFGTearDown();
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
//This was from the article, too... well, mostly.
int CNFGSetup( const char * name_of_window, int width, int height )
{
    load_functions();
	static LPSTR szClassName = "MyClass";
	RECT client, window;
	WNDCLASS wnd;
	int w, h, wd, hd;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	bufferx = (short)width;
	buffery = (short)height;
	wnd.style = CS_HREDRAW | CS_VREDRAW; //we will explain this later
	wnd.lpfnWndProc = MyWndProc;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(NULL, IDI_APPLICATION); //default icon
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);   //default arrow mouse cursor
	wnd.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wnd.lpszMenuName = NULL;                     //no menu
	wnd.lpszClassName = szClassName;
	if(!RegisterClass(&wnd))                     //register the WNDCLASS
	{
		MessageBox(NULL, "This Program Requires Windows NT", "Error", MB_OK);
	}
	lsHWND = CreateWindow(szClassName,
		name_of_window,      //name_of_window,
		WS_OVERLAPPEDWINDOW, //basic window style
		CW_USEDEFAULT,
		CW_USEDEFAULT,       //set starting point to default value
		bufferx,
		buffery,        //set all the dimensions to default value
		NULL,                //no parent window
		NULL,                //no menu
		hInstance,
		NULL);               //no parameters to pass
	lsWindowHDC = GetDC( lsHWND );
#ifdef CNFGOGL
	//From NeHe
	static  PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0, 
		0,
		0,
		0,
		0, 0, 0, 0,
		16,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};
	GLuint      PixelFormat = LOADED_ChoosePixelFormat( lsWindowHDC, &pfd );
	if( !LOADED_SetPixelFormat( lsWindowHDC, PixelFormat, &pfd ) )
	{
		MessageBox( 0, "Could not create PFD for OpenGL Context\n", 0, 0 );
		exit( -1 );
	}
	if (!(hRC=LOADED_wglCreateContext(lsWindowHDC)))                   // Are We Able To Get A Rendering Context?
	{
		MessageBox( 0, "Could not create OpenGL Context\n", 0, 0 );
		exit( -1 );
	}
	if(!LOADED_wglMakeCurrent(lsWindowHDC,hRC))                        // Try To Activate The Rendering Context
	{
		MessageBox( 0, "Could not current OpenGL Context\n", 0, 0 );
		exit( -1 );
	}
#endif
	lsHDC = LOADED_CreateCompatibleDC( lsWindowHDC );
	lsHDCBlit = LOADED_CreateCompatibleDC( lsWindowHDC );
	lsBitmap = LOADED_CreateCompatibleBitmap( lsWindowHDC, bufferx, buffery );
	LOADED_SelectObject( lsHDC, lsBitmap );
	//lsClearBrush = LOADED_CreateSolidBrush( CNFGBGColor );
	//lsHBR = LOADED_CreateSolidBrush( 0xFFFFFF );
	//lsHPEN = LOADED_CreatePen( PS_SOLID, 0, 0xFFFFFF );
	ShowWindow(lsHWND, 1);              //display the window on the screen
	//Once set up... we have to change the window's borders so we get the client size right.
	GetClientRect( lsHWND, &client );
	GetWindowRect( lsHWND, &window );
	w = ( window.right - window.left);
	h = ( window.bottom - window.top);
	wd = w - client.right;
	hd = h - client.bottom;
	MoveWindow( lsHWND, window.left, window.top, bufferx + wd, buffery + hd, 1 );
	InternalHandleResize();
#ifdef CNFG_BATCH
	CNFGSetupBatchInternal();
#endif
	return 0;
}
void CNFGHandleInput()
{
	MSG msg;
	while( PeekMessage( &msg, lsHWND, 0, 0xFFFF, 1 ) )
	{
		TranslateMessage(&msg);
		switch( msg.message )
		{
		case WM_MOUSEMOVE:
			HandleMotion( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, ( (msg.wParam & 0x01)?1:0) | ((msg.wParam & 0x02)?2:0) | ((msg.wParam & 0x10)?4:0) );
			break;
		case WM_LBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 1, 1 ); break;
		case WM_RBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 2, 1 ); break;
		case WM_MBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 3, 1 ); break;
		case WM_LBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 1, 0 ); break;
		case WM_RBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 2, 0 ); break;
		case WM_MBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 3, 0 ); break;
		case WM_KEYDOWN:
		case WM_KEYUP:
			HandleKey( tolower( (int) msg.wParam ), (msg.message==WM_KEYDOWN) );
			break;
		default:
			DispatchMessage(&msg);
			break;
		}
	}
}
#ifndef CNFGOGL
#ifndef CNFGRASTERIZER
static HBITMAP lsBackBitmap;
static HBRUSH lsHBR;
static HPEN lsHPEN;
static HBRUSH lsClearBrush;
static void InternalHandleResize()
{
	LOADED_DeleteObject( lsBackBitmap );
	lsBackBitmap = LOADED_CreateCompatibleBitmap( lsHDC, bufferx, buffery );
	LOADED_SelectObject( lsHDC, lsBackBitmap );
}
#ifdef BATCH_ELEMENTS
static int linelisthead;
static int pointlisthead;
static int polylisthead;
static int polylistindex;
static POINT linelist[4096*3];
static DWORD twoarray[4096];
static POINT pointlist[4096];
static POINT polylist[8192];
static INT   polylistcutoffs[8192];
static int last_linex;
static int last_liney;
static int possible_lastline;
void FlushTacking()
{
	int i;
	if( twoarray[0] != 2 )
		for( i = 0; i < 4096; i++ ) twoarray[i] = 2;
	if( linelisthead )
	{
		LOADED_PolyPolyline( lsHDC, linelist, twoarray, linelisthead );
		linelisthead = 0;
	}
	if( polylistindex )
	{
		LOADED_PolyPolygon( lsHDC, polylist, polylistcutoffs, polylistindex );
		polylistindex = 0;
		polylisthead = 0;
	}
	if( possible_lastline )
		CNFGTackPixel( last_linex, last_liney );
	possible_lastline = 0;
	//XXX TODO: Consider locking the bitmap, and manually drawing the pixels.
	if( pointlisthead )
	{
		for( i = 0; i < pointlisthead; i++ )
		{
			LOADED_SetPixel( lsHDC, pointlist[i].x, pointlist[i].y, CNFGLastColor );
		}
		pointlisthead = 0;
	}
}
#endif
uint32_t CNFGColor( uint32_t RGB )
{
	RGB = COLORSWAPS( RGB );
	if( CNFGLastColor == RGB ) return RGB;
#ifdef BATCH_ELEMENTS
	FlushTacking();
#endif
	CNFGLastColor = RGB;
	LOADED_DeleteObject( lsHBR );
	lsHBR = LOADED_CreateSolidBrush( RGB );
	LOADED_SelectObject( lsHDC, lsHBR );
	LOADED_DeleteObject( lsHPEN );
	lsHPEN = LOADED_CreatePen( PS_SOLID, 0, RGB );
	LOADED_SelectObject( lsHDC, lsHPEN );
	return RGB;
}
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	static int pbw, pbh;
	static HBITMAP pbb;
	if( !pbb || pbw != w || pbh !=h )
	{
		if( pbb ) LOADED_DeleteObject( pbb );
		pbb = LOADED_CreateBitmap( w, h, 1, 32, 0 );
		pbh = h;
		pbw = w;
	}
	LOADED_SetBitmapBits(pbb,w*h*4,data);
	LOADED_SelectObject( lsHDCBlit, pbb );
	LOADED_BitBlt(lsHDC, x, y, w, h, lsHDCBlit, 0, 0, SRCCOPY);
}
void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
#ifdef BATCH_ELEMENTS
	if( ( x1 != last_linex || y1 != last_liney ) && possible_lastline )
	{
		CNFGTackPixel( last_linex, last_liney );
	}
	if( x1 == x2 && y1 == y2 )
	{
		CNFGTackPixel( x1, y1 );
		possible_lastline = 0;
		return;
	}
	last_linex = x2;
	last_liney = y2;
	possible_lastline = 1;
	if( x1 != x2 || y1 != y2 )
	{
		linelist[linelisthead*2+0].x = x1;
		linelist[linelisthead*2+0].y = y1;
		linelist[linelisthead*2+1].x = x2;
		linelist[linelisthead*2+1].y = y2;
		linelisthead++;
		if( linelisthead >= 2048 ) FlushTacking();
	}
#else
	POINT pt[2] = { {x1, y1}, {x2, y2} };
	Polyline( lsHDC, pt, 2 );
	LOADED_SetPixel( lsHDC, x1, y1, CNFGLastColor );
	LOADED_SetPixel( lsHDC, x2, y2, CNFGLastColor );
#endif
}
void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
#ifdef BATCH_ELEMENTS
	FlushTacking();
#endif
	RECT r;
	if( x1 < x2 ) { r.left = x1; r.right = x2; }
	else          { r.left = x2; r.right = x1; }
	if( y1 < y2 ) { r.top = y1; r.bottom = y2; }
	else          { r.top = y2; r.bottom = y1; }
	FillRect( lsHDC, &r, lsHBR );
}
void CNFGClearFrame()
{
#ifdef BATCH_ELEMENTS
	FlushTacking();
#endif
	RECT r = { 0, 0, bufferx, buffery };
	LOADED_DeleteObject( lsClearBrush  );
	lsClearBrush = LOADED_CreateSolidBrush( COLORSWAPS(CNFGBGColor) );
	LOADED_SelectObject( lsHDC, lsClearBrush );
	FillRect( lsHDC, &r, lsClearBrush);
}
void CNFGTackPoly( RDPoint * points, int verts )
{
#ifdef BATCH_ELEMENTS
	if( verts > 8192 )
	{
		FlushTacking();
		//Fall-through
	}
	else
	{
		if( polylistindex >= 8191 || polylisthead + verts >= 8191 )
		{
			FlushTacking();
		}
		int i;
		for( i = 0; i < verts; i++ )
		{
			polylist[polylisthead].x = points[i].x;
			polylist[polylisthead].y = points[i].y;
			polylisthead++;
		}
		polylistcutoffs[polylistindex++] = verts;
		return;
	}
#endif
	{
		int i;
		POINT * t = (POINT*)alloca( sizeof( POINT ) * verts );
		for( i = 0; i < verts; i++ )
		{
			t[i].x = points[i].x;
			t[i].y = points[i].y;
		}
		LOADED_Polygon( lsHDC, t, verts );
	}
}
void CNFGTackPixel( short x1, short y1 )
{
#ifdef BATCH_ELEMENTS
	pointlist[pointlisthead+0].x = x1;
	pointlist[pointlisthead+0].y = y1;
	pointlisthead++;
	if( pointlisthead >=4096 ) FlushTacking();
#else
	LOADED_SetPixel( lsHDC, x1, y1, CNFGLastColor );
#endif
}
void CNFGSwapBuffers()
{
#ifdef BATCH_ELEMENTS
	FlushTacking();
#endif
	int thisw, thish;
	RECT r;
	LOADED_BitBlt( lsWindowHDC, 0, 0, bufferx, buffery, lsHDC, 0, 0, SRCCOPY );
	UpdateWindow( lsHWND );
	//Check to see if the window is closed.
	if( !IsWindow( lsHWND ) )
	{
		exit( 0 );
	}
	GetClientRect( lsHWND, &r );
	thisw = r.right - r.left;
	thish = r.bottom - r.top;
	if( thisw != bufferx || thish != buffery )
	{
		bufferx = (short)thisw;
		buffery = (short)thish;
		InternalHandleResize();
	}
}
void CNFGInternalResize( short bfx, short bfy ) { }
#endif
#endif
#endif // _CNFGWINDRIVER_C
#elif defined( EGL_LEAN_AND_MEAN )
//Copyright (c) 2011, 2017, 2018, 2020 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
//This driver cannot create an OpenGL Surface, but can be used for computing for background tasks.
//NOTE: This is a truly incomplete driver - if no EGL surface is available, it does not support direct buffer rendering.
//Additionally no input is connected.
#include <stdio.h>
#include <stdlib.h>
#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2ext.h>
static const EGLint configAttribs[] = {
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_NONE
};
EGLint context_attribs[] = { 
	EGL_CONTEXT_CLIENT_VERSION, 2, 
	EGL_NONE 
};
static int pbufferWidth = 0;
static int pbufferHeight = 0;
static EGLint pbufferAttribs[] = {
	EGL_WIDTH, 0,
	EGL_HEIGHT, 0,
	EGL_NONE,
};
EGLDisplay eglDpy = 0;
EGLContext eglCtx = 0;
EGLSurface eglSurf = 0;
void CNFGGetDimensions( short * x, short * y )
{
	*x = pbufferWidth;
	*y = pbufferHeight;
}
void	CNFGChangeWindowTitle( const char * WindowName )
{
}
void CNFGSetupFullscreen( const char * WindowName, int screen_no )
{
	//Fullscreen is meaningless for this driver, since it doesn't really open a window.
	CNFGSetup( WindowName, 1024, 1024 );
}
void CNFGTearDown()
{
	if( eglDpy )
	{
		eglTerminate( eglDpy );
	}
	//Unimplemented.
}
int CNFGSetup( const char * WindowName, int w, int h )
{
	eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	atexit( CNFGTearDown );
	printf( "EGL Display: %p\n", eglDpy );
	pbufferAttribs[1] = pbufferWidth = w;
	pbufferAttribs[3] = pbufferHeight = h;
	EGLint major, minor;
	eglInitialize(eglDpy, &major, &minor);
	EGLint numConfigs=0;
	EGLConfig eglCfg=NULL;
    eglChooseConfig(eglDpy, configAttribs, 0, 0, &numConfigs); //this gets number of configs
	if (numConfigs) {
		eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);
		printf( " EGL config found\n" );
	} else {
		printf( " Error could not find a valid config avail.. \n" );
	}
	printf( "EGL Major Minor: %d %d\n", major, minor );
	eglBindAPI(EGL_OPENGL_API);
	eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, context_attribs);
	int err = eglGetError(); if(err != EGL_SUCCESS) { printf("1. Error %d\n", err); }
	printf( "EGL Got context: %p\n", eglCtx );
	if( w > 0 && h > 0 )
	{
		eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);
		eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);
		printf( "EGL Current, with surface %p\n", eglSurf );
		//Actually have a surface.  Need to allocate it.
		EGLint surfwid;
		EGLint surfht;
		eglQuerySurface(eglDpy, eglSurf, EGL_WIDTH, &surfwid);
		eglQuerySurface(eglDpy, eglSurf, EGL_HEIGHT, &surfht);
		printf("Window dimensions: %d x %d\n", surfwid, surfht);
	}
	else
	{
		eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, eglCtx);
		printf( "EGL Current, no surface.\n" );
	}
	return 0;
}
void CNFGHandleInput()
{
	//Stubbed (No input)
	return;
}
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h )
{
	//Stubbed (No input)
}
void CNFGSetVSync( int vson )
{
	//No-op
}
void CNFGSwapBuffers()
{
	//No-op
}
#elif defined( __android__ ) || defined( ANDROID )
/*
 * Copyright (c) 2011-2013 Luc Verhaegen <libv@skynet.be>
 * Copyright (c) 2018-2020 <>< Charles Lohr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#if defined( __android__ ) && !defined( ANDROID )
#define ANDROID
#endif
//Note: This interface provides the following two things privately.
//you may "extern" them in your code.
#ifdef ANDROID
#ifndef _CNFG_ANDROID_H
#define _CNFG_ANDROID_H
//This file contains the additional functions that are available on the Android platform.
//In order to build rawdraw for Android, please compile CNFGEGLDriver.c with -DANDROID
extern struct android_app * gapp;
void AndroidMakeFullscreen();
int AndroidHasPermissions(const char* perm_name);
void AndroidRequestAppPermissions(const char * perm);
void AndroidDisplayKeyboard(int pShow);
int AndroidGetUnicodeChar( int keyCode, int metaState );
void AndroidSendToBack( int param );
extern int android_sdk_version; //Derived at start from property ro.build.version.sdk
extern int android_width, android_height;
extern int UpdateScreenWithBitmapOffsetX;
extern int UpdateScreenWithBitmapOffsetY;
//You must implement these.
void HandleResume();
void HandleSuspend();
//Departures:
// HandleMotion's "mask" parameter is actually just an index, not a mask
// CNFGSetup / CNFGSetupFullScreen only controls whether or not the navigation
// decoration is removed.  Fullscreen means *full screen* To choose fullscreen
// or not fullscrene, modify, in your AndroidManifest.xml file, the application
// section to either contain or not contain:
//     android:theme="@android:style/Theme.NoTitleBar.Fullscreen"
#endif
struct android_app * gapp;
static int OGLESStarted;
int android_width, android_height;
int android_sdk_version;
#include <android_native_app_glue.h>
#include <jni.h>
#include <native_activity.h>
#define ERRLOG(...) printf( __VA_ARGS__ );
#else
#define ERRLOG(...) fprintf( stderr, __VA_ARGS__ );
#endif
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <EGL/egl.h>
#ifdef ANDROID
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif
#define EGL_ZBITS 16
#define EGL_IMMEDIATE_SIZE 2048
#ifdef USE_EGL_X
	#error This feature has never been completed or tested.
	Display *XDisplay;
	Window XWindow;
#else
	typedef enum
	{
		FBDEV_PIXMAP_DEFAULT = 0,
		FBDEV_PIXMAP_SUPPORTS_UMP = (1<<0),
		FBDEV_PIXMAP_ALPHA_FORMAT_PRE = (1<<1),
		FBDEV_PIXMAP_COLORSPACE_sRGB = (1<<2),
		FBDEV_PIXMAP_EGL_MEMORY = (1<<3)        /* EGL allocates/frees this memory */
	} fbdev_pixmap_flags;
	typedef struct fbdev_window
	{
		unsigned short width;
		unsigned short height;
	} fbdev_window;
	typedef struct fbdev_pixmap
	{
		unsigned int height;
		unsigned int width;
		unsigned int bytes_per_pixel;
		unsigned char buffer_size;
		unsigned char red_size;
		unsigned char green_size;
		unsigned char blue_size;
		unsigned char alpha_size;
		unsigned char luminance_size;
		fbdev_pixmap_flags flags;
		unsigned short *data;
		unsigned int format; /* extra format information in case rgbal is not enough, especially for YUV formats */
	} fbdev_pixmap;
#if defined( ANDROID )
EGLNativeWindowType native_window;
#else
struct fbdev_window native_window;
#endif
#endif
static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,
	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, EGL_ZBITS,
	//EGL_SAMPLES, 1,
#ifdef ANDROID
#if ANDROIDVERSION >= 28
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
#else
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#endif
#else
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
#endif
	EGL_NONE
};
static EGLint window_attribute_list[] = {
	EGL_NONE
};
static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};
EGLDisplay egl_display;
EGLSurface egl_surface;
void CNFGSetVSync( int vson )
{
	eglSwapInterval(egl_display, vson);
}
static short iLastInternalW, iLastInternalH;
void CNFGSwapBuffers()
{
	CNFGFlushRender();
	eglSwapBuffers(egl_display, egl_surface);
#ifdef ANDROID
	android_width = ANativeWindow_getWidth( native_window );
	android_height = ANativeWindow_getHeight( native_window );
	LOADED_glViewport( 0, 0, android_width, android_height );
	if( iLastInternalW != android_width || iLastInternalH != android_height )
		CNFGInternalResize( iLastInternalW=android_width, iLastInternalH=android_height );
#endif
}
void CNFGGetDimensions( short * x, short * y )
{
#ifdef ANDROID
	*x = android_width;
	*y = android_height;
#else
	*x = native_window.width;
	*y = native_window.height;
#endif
	if( *x != iLastInternalW || *y != iLastInternalH )
		CNFGInternalResize( iLastInternalW=*x, iLastInternalH=*y );
}
int CNFGSetup( const char * WindowName, int w, int h )
{
	EGLint egl_major, egl_minor;
	EGLConfig config;
	EGLint num_config;
	EGLContext context;
	//This MUST be called before doing any initialization.
	int events;
	while( !OGLESStarted )
	{
		struct android_poll_source* source;
		if (ALooper_pollAll( 0, 0, &events, (void**)&source) >= 0)
		{
			if (source != NULL) source->process(gapp, source);
		}
	}
#ifdef USE_EGL_X
	XDisplay = XOpenDisplay(NULL);
	if (!XDisplay) {
		ERRLOG( "Error: failed to open X display.\n");
		return -1;
	}
	Window XRoot = DefaultRootWindow(XDisplay);
	XSetWindowAttributes XWinAttr;
	XWinAttr.event_mask  =  ExposureMask | PointerMotionMask;
	XWindow = XCreateWindow(XDisplay, XRoot, 0, 0, WIDTH, HEIGHT, 0,
				CopyFromParent, InputOutput,
				CopyFromParent, CWEventMask, &XWinAttr);
	Atom XWMDeleteMessage =
		XInternAtom(XDisplay, "WM_DELETE_WINDOW", False);
	XMapWindow(XDisplay, XWindow);
	XStoreName(XDisplay, XWindow, "Mali libs test");
	XSetWMProtocols(XDisplay, XWindow, &XWMDeleteMessage, 1);
	egl_display = eglGetDisplay((EGLNativeDisplayType) XDisplay);
#else
#ifndef ANDROID
	if( w >= 1 && h >= 1 )
	{
		native_window.width = w;
		native_window.height =h;
	}
#endif
	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
	if (egl_display == EGL_NO_DISPLAY) {
		ERRLOG( "Error: No display found!\n");
		return -1;
	}
	if (!eglInitialize(egl_display, &egl_major, &egl_minor)) {
		ERRLOG( "Error: eglInitialise failed!\n");
		return -1;
	}
	printf("EGL Version: \"%s\"\n",
	       eglQueryString(egl_display, EGL_VERSION));
	printf("EGL Vendor: \"%s\"\n",
	       eglQueryString(egl_display, EGL_VENDOR));
	printf("EGL Extensions: \"%s\"\n",
	       eglQueryString(egl_display, EGL_EXTENSIONS));
	eglChooseConfig(egl_display, config_attribute_list, &config, 1,
			&num_config);
	printf( "Config: %d\n", num_config );
	printf( "Creating Context\n" );
	context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT,
//				NULL );
				context_attribute_list);
	if (context == EGL_NO_CONTEXT) {
		ERRLOG( "Error: eglCreateContext failed: 0x%08X\n",
			eglGetError());
		return -1;
	}
	printf( "Context Created %p\n", context );
#ifdef USE_EGL_X
	egl_surface = eglCreateWindowSurface(egl_display, config, XWindow,
					     window_attribute_list);
#else
	if( native_window && !gapp->window )
	{
		printf( "WARNING: App restarted without a window.  Cannot progress.\n" );
		exit( 0 );
	}
	printf( "Getting Surface %p\n", native_window = gapp->window );
	if( !native_window )
	{
		printf( "FAULT: Cannot get window\n" );
		return -5;
	}
	android_width = ANativeWindow_getWidth( native_window );
	android_height = ANativeWindow_getHeight( native_window );
	printf( "Width/Height: %dx%d\n", android_width, android_height );
	egl_surface = eglCreateWindowSurface(egl_display, config,
#ifdef ANDROID
			     gapp->window,
#else
			     (EGLNativeWindowType)&native_window,
#endif
			     window_attribute_list);
#endif
	printf( "Got Surface: %p\n", egl_surface );
	if (egl_surface == EGL_NO_SURFACE) {
		ERRLOG( "Error: eglCreateWindowSurface failed: "
			"0x%08X\n", eglGetError());
		return -1;
	}
#ifndef ANDROID
	int width, height;
	if (!eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &width) ||
	    !eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &height)) {
		ERRLOG( "Error: eglQuerySurface failed: 0x%08X\n",
			eglGetError());
		return -1;
	}
	printf("Surface size: %dx%d\n", width, height);
	native_window.width = width;
	native_window.height = height;
#endif
	if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, context)) {
		ERRLOG( "Error: eglMakeCurrent() failed: 0x%08X\n",
			eglGetError());
		return -1;
	}
	printf("GL Vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("GL Renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("GL Version: \"%s\"\n", glGetString(GL_VERSION));
	printf("GL Extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));
	CNFGSetupBatchInternal();
	{
		short dummyx, dummyy;
		CNFGGetDimensions( &dummyx, &dummyy );
	}
	return 0;
}
void CNFGSetupFullscreen( const char * WindowName, int screen_number )
{
	//Removes decoration, must be called before setup.
	AndroidMakeFullscreen();
	CNFGSetup( WindowName, -1, -1 );
}
int debuga, debugb, debugc;
int32_t handle_input(struct android_app* app, AInputEvent* event)
{
#ifdef ANDROID
	//Potentially do other things here.
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
	{
		static uint64_t downmask;
		int action = AMotionEvent_getAction( event );
		int whichsource = action >> 8;
		action &= AMOTION_EVENT_ACTION_MASK;
		size_t pointerCount = AMotionEvent_getPointerCount(event);
		for (size_t i = 0; i < pointerCount; ++i)
		{
			int x, y, index;
			x = AMotionEvent_getX(event, i);
			y = AMotionEvent_getY(event, i);
			index = AMotionEvent_getPointerId( event, i );
			if( action == AMOTION_EVENT_ACTION_POINTER_DOWN || action == AMOTION_EVENT_ACTION_DOWN )
			{
				int id = index;
				if( action == AMOTION_EVENT_ACTION_POINTER_DOWN && id != whichsource ) continue;
				HandleButton( x, y, id, 1 );
				downmask    |= 1<<id;
				ANativeActivity_showSoftInput( gapp->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED );
			}
			else if( action == AMOTION_EVENT_ACTION_POINTER_UP || action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL )
			{
				int id = index;
				if( action == AMOTION_EVENT_ACTION_POINTER_UP && id != whichsource ) continue;
				HandleButton( x, y, id, 0 );
				downmask    &= ~(1<<id);
			}
			else if( action == AMOTION_EVENT_ACTION_MOVE )
			{
				HandleMotion( x, y, index );
			}
		}
		return 1;
	}
	else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
	{
		int code = AKeyEvent_getKeyCode(event);
#ifdef ANDROID_USE_SCANCODES
		HandleKey( code, AKeyEvent_getAction(event) );
#else
		int unicode = AndroidGetUnicodeChar( code, AMotionEvent_getMetaState( event ) );
		if( unicode )
			HandleKey( unicode, AKeyEvent_getAction(event) );
		else
		{
			HandleKey( code, !AKeyEvent_getAction(event) );
			return (code == 4)?1:0; //don't override functionality.
		}
#endif
		return 1;
	}
#endif
	return 0;
}
void CNFGHandleInput()
{
#ifdef ANDROID
	int events;
	struct android_poll_source* source;
	while( ALooper_pollAll( 0, 0, &events, (void**)&source) >= 0 )
	{
		if (source != NULL)
		{
			source->process(gapp, source);
		}
	}
#endif
#ifdef USE_EGL_X
	while (1) {
		XEvent event;
		XNextEvent(XDisplay, &event);
		if ((event.type == MotionNotify) ||
		    (event.type == Expose))
			Redraw(width, height);
		else if (event.type == ClientMessage) {
			if (event.xclient.data.l[0] == XWMDeleteMessage)
				break;
		}
	}
	XSetWMProtocols(XDisplay, XWindow, &XWMDeleteMessage, 0);
#endif
}
#ifdef ANDROID
void handle_cmd(struct android_app* app, int32_t cmd)
{
	switch (cmd)
	{
	case APP_CMD_DESTROY:
		//This gets called initially after back.
		HandleDestroy();
		ANativeActivity_finish( gapp->activity );
		break;
	case APP_CMD_INIT_WINDOW:
		//When returning from a back button suspension, this isn't called.
		if( !OGLESStarted )
		{
			OGLESStarted = 1;
			printf( "Got start event\n" );
		}
		else
		{
			CNFGSetup( "", -1, -1 );
			HandleResume();
		}
		break;
	//case APP_CMD_TERM_WINDOW:
		//This gets called initially when you click "back"
		//This also gets called when you are brought into standby.
		//Not sure why - callbacks here seem to break stuff.
	//	break;
	default:
		printf( "event not handled: %d", cmd);
	}
}
int __system_property_get(const char* name, char* value);
void android_main(struct android_app* app)
{
	int main( int argc, char ** argv );
	char * argv[] = { "main", 0 };
	{
		char sdk_ver_str[92];
		int len = __system_property_get("ro.build.version.sdk", sdk_ver_str);
		if( len <= 0 ) 
			android_sdk_version = 0;
		else
			android_sdk_version = atoi(sdk_ver_str);
	}
	gapp = app;
	app->onAppCmd = handle_cmd;
	app->onInputEvent = handle_input;
	printf( "Starting with Android SDK Version: %d", android_sdk_version );
	printf( "Starting Main\n" );
	main( 1, argv );
	printf( "Main Complete\n" );
}
void AndroidMakeFullscreen()
{
	//Partially based on https://stackoverflow.com/questions/47507714/how-do-i-enable-full-screen-immersive-mode-for-a-native-activity-ndk-app
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	//Get android.app.NativeActivity, then get getWindow method handle, returns view.Window type
	jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");
	jmethodID getWindow = env->GetMethodID( envptr, activityClass, "getWindow", "()Landroid/view/Window;");
	jobject window = env->CallObjectMethod( envptr, gapp->activity->clazz, getWindow);
	//Get android.view.Window class, then get getDecorView method handle, returns view.View type
	jclass windowClass = env->FindClass( envptr, "android/view/Window");
	jmethodID getDecorView = env->GetMethodID( envptr, windowClass, "getDecorView", "()Landroid/view/View;");
	jobject decorView = env->CallObjectMethod( envptr, window, getDecorView);
	//Get the flag values associated with systemuivisibility
	jclass viewClass = env->FindClass( envptr, "android/view/View");
	const int flagLayoutHideNavigation = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I"));
	const int flagLayoutFullscreen = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I"));
	const int flagLowProfile = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LOW_PROFILE", "I"));
	const int flagHideNavigation = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I"));
	const int flagFullscreen = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I"));
	const int flagImmersiveSticky = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I"));
	jmethodID setSystemUiVisibility = env->GetMethodID( envptr, viewClass, "setSystemUiVisibility", "(I)V");
	//Call the decorView.setSystemUiVisibility(FLAGS)
	env->CallVoidMethod( envptr, decorView, setSystemUiVisibility,
		        (flagLayoutHideNavigation | flagLayoutFullscreen | flagLowProfile | flagHideNavigation | flagFullscreen | flagImmersiveSticky));
	//now set some more flags associated with layoutmanager -- note the $ in the class path
	//search for api-versions.xml
	//https://android.googlesource.com/platform/development/+/refs/tags/android-9.0.0_r48/sdk/api-versions.xml
	jclass layoutManagerClass = env->FindClass( envptr, "android/view/WindowManager$LayoutParams");
	const int flag_WinMan_Fullscreen = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_FULLSCREEN", "I") ));
	const int flag_WinMan_KeepScreenOn = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_KEEP_SCREEN_ON", "I") ));
	const int flag_WinMan_hw_acc = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_HARDWARE_ACCELERATED", "I") ));
	//    const int flag_WinMan_flag_not_fullscreen = env->GetStaticIntField(layoutManagerClass, (env->GetStaticFieldID(layoutManagerClass, "FLAG_FORCE_NOT_FULLSCREEN", "I") ));
	//call window.addFlags(FLAGS)
	env->CallVoidMethod( envptr, window, (env->GetMethodID (envptr, windowClass, "addFlags" , "(I)V")), (flag_WinMan_Fullscreen | flag_WinMan_KeepScreenOn | flag_WinMan_hw_acc));
	jnii->DetachCurrentThread( jniiptr );
}
void AndroidDisplayKeyboard(int pShow)
{
	//Based on https://stackoverflow.com/questions/5864790/how-to-show-the-soft-keyboard-on-native-activity
	jint lFlags = 0;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");
	// Retrieves NativeActivity.
	jobject lNativeActivity = gapp->activity->clazz;
	// Retrieves Context.INPUT_METHOD_SERVICE.
	jclass ClassContext = env->FindClass( envptr, "android/content/Context");
	jfieldID FieldINPUT_METHOD_SERVICE = env->GetStaticFieldID( envptr, ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;" );
	jobject INPUT_METHOD_SERVICE = env->GetStaticObjectField( envptr, ClassContext, FieldINPUT_METHOD_SERVICE );
	// Runs getSystemService(Context.INPUT_METHOD_SERVICE).
	jclass ClassInputMethodManager = env->FindClass( envptr, "android/view/inputmethod/InputMethodManager" );
	jmethodID MethodGetSystemService = env->GetMethodID( envptr, activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
	jobject lInputMethodManager = env->CallObjectMethod( envptr, lNativeActivity, MethodGetSystemService, INPUT_METHOD_SERVICE);
	// Runs getWindow().getDecorView().
	jmethodID MethodGetWindow = env->GetMethodID( envptr, activityClass, "getWindow", "()Landroid/view/Window;");
	jobject lWindow = env->CallObjectMethod( envptr, lNativeActivity, MethodGetWindow);
	jclass ClassWindow = env->FindClass( envptr, "android/view/Window");
	jmethodID MethodGetDecorView = env->GetMethodID( envptr, ClassWindow, "getDecorView", "()Landroid/view/View;");
	jobject lDecorView = env->CallObjectMethod( envptr, lWindow, MethodGetDecorView);
	if (pShow) {
		// Runs lInputMethodManager.showSoftInput(...).
		jmethodID MethodShowSoftInput = env->GetMethodID( envptr, ClassInputMethodManager, "showSoftInput", "(Landroid/view/View;I)Z");
		/*jboolean lResult = */env->CallBooleanMethod( envptr, lInputMethodManager, MethodShowSoftInput, lDecorView, lFlags);
	} else {
		// Runs lWindow.getViewToken()
		jclass ClassView = env->FindClass( envptr, "android/view/View");
		jmethodID MethodGetWindowToken = env->GetMethodID( envptr, ClassView, "getWindowToken", "()Landroid/os/IBinder;");
		jobject lBinder = env->CallObjectMethod( envptr, lDecorView, MethodGetWindowToken);
		// lInputMethodManager.hideSoftInput(...).
		jmethodID MethodHideSoftInput = env->GetMethodID( envptr, ClassInputMethodManager, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
		/*jboolean lRes = */env->CallBooleanMethod( envptr, lInputMethodManager, MethodHideSoftInput, lBinder, lFlags);
	}
	// Finished with the JVM.
	jnii->DetachCurrentThread( jniiptr );
}
int AndroidGetUnicodeChar( int keyCode, int metaState )
{
	//https://stackoverflow.com/questions/21124051/receive-complete-android-unicode-input-in-c-c/43871301
	int eventType = AKEY_EVENT_ACTION_DOWN;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	//jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");
	// Retrieves NativeActivity.
	//jobject lNativeActivity = gapp->activity->clazz;
	jclass class_key_event = env->FindClass( envptr, "android/view/KeyEvent");
	int unicodeKey;
	jmethodID method_get_unicode_char = env->GetMethodID( envptr, class_key_event, "getUnicodeChar", "(I)I");
	jmethodID eventConstructor = env->GetMethodID( envptr, class_key_event, "<init>", "(II)V");
	jobject eventObj = env->NewObject( envptr, class_key_event, eventConstructor, eventType, keyCode);
	unicodeKey = env->CallIntMethod( envptr, eventObj, method_get_unicode_char, metaState );
	// Finished with the JVM.
	jnii->DetachCurrentThread( jniiptr );
	//printf("Unicode key is: %d", unicodeKey);
	return unicodeKey;
}
//Based on: https://stackoverflow.com/questions/41820039/jstringjni-to-stdstringc-with-utf8-characters
jstring android_permission_name(const struct JNINativeInterface ** envptr, const char* perm_name) {
    // nested class permission in class android.Manifest,
    // hence android 'slash' Manifest 'dollar' permission
	const struct JNINativeInterface * env = *envptr;
    jclass ClassManifestpermission = env->FindClass( envptr, "android/Manifest$permission");
    jfieldID lid_PERM = env->GetStaticFieldID( envptr, ClassManifestpermission, perm_name, "Ljava/lang/String;" );
    jstring ls_PERM = (jstring)(env->GetStaticObjectField( envptr, ClassManifestpermission, lid_PERM )); 
    return ls_PERM;
}
/**
 * \brief Tests whether a permission is granted.
 * \param[in] app a pointer to the android app.
 * \param[in] perm_name the name of the permission, e.g.,
 *   "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE".
 * \retval true if the permission is granted.
 * \retval false otherwise.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
int AndroidHasPermissions( const char* perm_name)
{
	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	if( android_sdk_version < 23 )
	{
		printf( "Android SDK version %d does not support AndroidHasPermissions\n", android_sdk_version );
		return 1;
	}
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	int result = 0;
	jstring ls_PERM = android_permission_name( envptr, perm_name);
	jint PERMISSION_GRANTED = (-1);
	{
		jclass ClassPackageManager = env->FindClass( envptr, "android/content/pm/PackageManager" );
		jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID( envptr, ClassPackageManager, "PERMISSION_GRANTED", "I" );
		PERMISSION_GRANTED = env->GetStaticIntField( envptr, ClassPackageManager, lid_PERMISSION_GRANTED );
	}
	{
		jobject activity = app->activity->clazz;
		jclass ClassContext = env->FindClass( envptr, "android/content/Context" );
		jmethodID MethodcheckSelfPermission = env->GetMethodID( envptr, ClassContext, "checkSelfPermission", "(Ljava/lang/String;)I" );
		jint int_result = env->CallIntMethod( envptr, activity, MethodcheckSelfPermission, ls_PERM );
		result = (int_result == PERMISSION_GRANTED);
	}
	jnii->DetachCurrentThread( jniiptr );
	return result;
}
/**
 * \brief Query file permissions.
 * \details This opens the system dialog that lets the user
 *  grant (or deny) the permission.
 * \param[in] app a pointer to the android app.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
void AndroidRequestAppPermissions(const char * perm)
{
	if( android_sdk_version < 23 )
	{
		printf( "Android SDK version %d does not support AndroidRequestAppPermissions\n",android_sdk_version );
		return;
	}
	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jobject activity = app->activity->clazz;
	jobjectArray perm_array = env->NewObjectArray( envptr, 1, env->FindClass( envptr, "java/lang/String"), env->NewStringUTF( envptr, "" ) );
	env->SetObjectArrayElement( envptr, perm_array, 0, android_permission_name( envptr, perm ) );
	jclass ClassActivity = env->FindClass( envptr, "android/app/Activity" );
	jmethodID MethodrequestPermissions = env->GetMethodID( envptr, ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V" );
	// Last arg (0) is just for the callback (that I do not use)
	env->CallVoidMethod( envptr, activity, MethodrequestPermissions, perm_array, 0 );
	jnii->DetachCurrentThread( jniiptr );
}
/* Example:
	int hasperm = android_has_permission( "RECORD_AUDIO" );
	if( !hasperm )
	{
		android_request_app_permissions( "RECORD_AUDIO" );
	}
*/
void AndroidSendToBack( int param )
{
	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jobject activity = app->activity->clazz;
	//_glfmCallJavaMethodWithArgs(jni, gapp->activity->clazz, "moveTaskToBack", "(Z)Z", Boolean, false);
	jclass ClassActivity = env->FindClass( envptr, "android/app/Activity" );
	jmethodID MethodmoveTaskToBack = env->GetMethodID( envptr, ClassActivity, "moveTaskToBack", "(Z)Z" );
	env->CallBooleanMethod( envptr, activity, MethodmoveTaskToBack, param );
	jnii->DetachCurrentThread( jniiptr );
}
#endif
#elif defined( __wasm__ )
//Right now, designed for use with https://github.com/cnlohr/rawdrawwasm/
// #include <CNFG.h>
#include <stdint.h>
extern void __attribute__((import_module("bynsyncify"))) CNFGSwapBuffersInternal();
void CNFGBlitImageInternal( uint32_t * data, int x, int y, int w, int h );
void print( double idebug );
void prints( const char* sdebug );
//Forward declarations that we get from either WASM or our javascript code.
void CNFGClearFrameInternal( uint32_t bgcolor );
//The WASM driver handles internal resizing automatically.
#ifndef CNFGRASTERIZER
void	CNFGInternalResize( short x, short y )
{
}
void CNFGFlushRender()
{
	if( !CNFGVertPlace ) return;
	CNFGEmitBackendTriangles( CNFGVertDataV, CNFGVertDataC, CNFGVertPlace );
	CNFGVertPlace = 0;
}
void CNFGClearFrame()
{
	CNFGFlushRender();
	CNFGClearFrameInternal( CNFGBGColor );
}
void CNFGSwapBuffers()
{
	CNFGFlushRender();
	CNFGSwapBuffersInternal( );
}
void CNFGHandleInput()
{
	//Do nothing.
	//Input is handled on swap frame.
}
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	CNFGBlitImageInternal( data, x, y, w, h );
}
#else
	
//Rasterizer - if you want to do this, you will need to enable blitting in the javascript.
//XXX TODO: NEED MEMORY ALLOCATOR
extern unsigned char __heap_base;
unsigned int bump_pointer = (unsigned int)&__heap_base;
void* malloc(unsigned long size) {
	unsigned int ptr = bump_pointer;
	bump_pointer += size;
	return (void *)ptr;
}
void free(void* ptr) {  }
//Don't call this file yourself.  It is intended to be included in any drivers which want to support the rasterizer plugin.
#ifdef CNFGRASTERIZER
//#include <stdlib.h>
#include <stdint.h>
static uint32_t * buffer = 0;
static short bufferx;
static short buffery;
#ifdef CNFGOGL
void CNFGFlushRender()
{
}
#endif
void CNFGInternalResize( short x, short y )
{
	bufferx = x;
	buffery = y;
	if( buffer ) free( buffer );
	buffer = malloc( bufferx * buffery * 4 );
#ifdef CNFGOGL
	void CNFGInternalResizeOGLBACKEND( short w, short h );
	CNFGInternalResizeOGLBACKEND( x, y );
#endif
}
#ifdef __wasm__
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<24;
	r>>=8;
	ret |= (r&0xff)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff)<<0;
	return ret;
}
#elif !defined(CNFGOGL)
#define SWAPS(x) (x>>8)
#else
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff);
	r>>=8;
	ret |= (r&0xff)<<24;
	return ret;
}
#endif
uint32_t CNFGColor( uint32_t RGB )
{
	CNFGLastColor = SWAPS(RGB);
	return CNFGLastColor;
}
void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	short tx, ty;
	float slope, lp;
	short dx = x2 - x1;
	short dy = y2 - y1;
	if( !buffer ) return;
	if( dx < 0 ) dx = -dx;
	if( dy < 0 ) dy = -dy;
	if( dx > dy )
	{
		short minx = (x1 < x2)?x1:x2;
		short maxx = (x1 < x2)?x2:x1;
		short miny = (x1 < x2)?y1:y2;
		short maxy = (x1 < x2)?y2:y1;
		float thisy = miny;
		slope = (float)(maxy-miny) / (float)(maxx-minx);
		for( tx = minx; tx <= maxx; tx++ )
		{
			ty = thisy;
			if( tx < 0 || ty < 0 || ty >= buffery ) continue;
			if( tx >= bufferx ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisy += slope;
		}
	}
	else
	{
		short minx = (y1 < y2)?x1:x2;
		short maxx = (y1 < y2)?x2:x1;
		short miny = (y1 < y2)?y1:y2;
		short maxy = (y1 < y2)?y2:y1;
		float thisx = minx;
		slope = (float)(maxx-minx) / (float)(maxy-miny);
		for( ty = miny; ty <= maxy; ty++ )
		{
			tx = thisx;
			if( ty < 0 || tx < 0 || tx >= bufferx ) continue;
			if( ty >= buffery ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisx += slope;
		}
	}
}
void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	short minx = (x1<x2)?x1:x2;
	short miny = (y1<y2)?y1:y2;
	short maxx = (x1>=x2)?x1:x2;
	short maxy = (y1>=y2)?y1:y2;
	short x, y;
	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx >= bufferx ) maxx = bufferx-1;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		uint32_t * bufferstart = &buffer[y * bufferx + minx];
		for( x = minx; x <= maxx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
}
void CNFGTackPoly( RDPoint * points, int verts )
{
	short minx = 10000, miny = 10000;
	short maxx =-10000, maxy =-10000;
	short i, x, y;
	//Just in case...
	if( verts > 32767 ) return;
	for( i = 0; i < verts; i++ )
	{
		RDPoint * p = &points[i];
		if( p->x < minx ) minx = p->x;
		if( p->y < miny ) miny = p->y;
		if( p->x > maxx ) maxx = p->x;
		if( p->y > maxy ) maxy = p->y;
	}
	if( miny < 0 ) miny = 0;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		short startfillx = maxx;
		short endfillx = minx;
		//Figure out what line segments intersect this line.
		for( i = 0; i < verts; i++ )
		{
			short pl = i + 1;
			if( pl == verts ) pl = 0;
			RDPoint ptop;
			RDPoint pbot;
			ptop.x = points[i].x;
			ptop.y = points[i].y;
			pbot.x = points[pl].x;
			pbot.y = points[pl].y;
//printf( "Poly: %d %d\n", pbot.y, ptop.y );
			if( pbot.y < ptop.y )
			{
				RDPoint ptmp;
				ptmp.x = pbot.x;
				ptmp.y = pbot.y;
				pbot.x = ptop.x;
				pbot.y = ptop.y;
				ptop.x = ptmp.x;
				ptop.y = ptmp.y;
			}
			//Make sure this line segment is within our range.
//printf( "PT: %d %d %d\n", y, ptop.y, pbot.y );
			if( ptop.y <= y && pbot.y >= y )
			{
				short diffy = pbot.y - ptop.y;
				uint32_t placey = (uint32_t)(y - ptop.y)<<16;  //Scale by 16 so we can do integer math.
				short diffx = pbot.x - ptop.x;
				short isectx;
				if( diffy == 0 )
				{
					if( pbot.x < ptop.x )
					{
						if( startfillx > pbot.x ) startfillx = pbot.x;
						if( endfillx < ptop.x ) endfillx = ptop.x;
					}
					else
					{
						if( startfillx > ptop.x ) startfillx = ptop.x;
						if( endfillx < pbot.x ) endfillx = pbot.x;
					}
				}
				else
				{
					//Inner part is scaled by 65536, outer part must be scaled back.
					isectx = (( (placey / diffy) * diffx + 32768 )>>16) + ptop.x;
					if( isectx < startfillx ) startfillx = isectx;
					if( isectx > endfillx ) endfillx = isectx;
				}
//printf( "R: %d %d %d\n", pbot.x, ptop.x, isectx );
			}
		}
//printf( "%d %d %d\n", y, startfillx, endfillx );
		if( endfillx >= bufferx ) endfillx = bufferx - 1;
		if( endfillx >= bufferx ) endfillx = buffery - 1;
		if( startfillx < 0 ) startfillx = 0;
		if( startfillx < 0 ) startfillx = 0;
		unsigned int * bufferstart = &buffer[y * bufferx + startfillx];
		for( x = startfillx; x <= endfillx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
//exit(1);
}
void CNFGClearFrame()
{
	int i, m;
	uint32_t col = 0;
	short x, y;
	CNFGGetDimensions( &x, &y );
	if( x != bufferx || y != buffery || !buffer )
	{
		bufferx = x;
		buffery = y;
		buffer = malloc( x * y * 8 );
	}
	m = x * y;
	col = CNFGColor( CNFGBGColor );
	for( i = 0; i < m; i++ )
	{
//printf( "Got: %d %p %d\n", m, buffer, i );
		buffer[i] = col;
	}
}
void CNFGTackPixel( short x, short y )
{
	if( x < 0 || y < 0 || x >= bufferx || y >= buffery ) return;
	buffer[x+bufferx*y] = CNFGLastColor;
}
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	int ox = x;
	int stride = w;
	if( w <= 0 || h <= 0 || x >= bufferx || y >= buffery ) return;
	if( x < 0 ) { w += x; x = 0; }
	if( y < 0 ) { h += y; y = 0; }
	//Switch w,h to x2, y2
	h += y;
	w += x;
	if( w >= bufferx ) { w = bufferx; }
	if( h >= buffery ) { h = buffery; }
	for( ; y < h-1; y++ )
	{
		x = ox;
		uint32_t * indat = data;
		uint32_t * outdat = buffer + y * bufferx + x;
		for( ; x < w-1; x++ )
		{
			uint32_t newm = *(indat++);
			uint32_t oldm = *(outdat);
			if( (newm & 0xff) == 0xff )
			{
				*(outdat++) = newm;
			}
			else
			{
				//Alpha blend.
				int alfa = newm&0xff;
				int onemalfa = 255-alfa;
#ifdef __wasm__
				uint32_t newv = 255<<0; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#elif defined( ANDROID ) || defined( __android__ )
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined( CNFGOGL ) //OGL, on X11
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#else //X11
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#endif
				*(outdat++) = newv;
			}
		}
		data += stride;
	}
}
void CNFGSwapBuffers()
{
	CNFGUpdateScreenWithBitmap( (uint32_t*)buffer, bufferx, buffery );
}
#endif
extern void CNFGUpdateScreenWithBitmapInternal( uint32_t * data, int w, int h );
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h )
{
	CNFGBlitImageInternal( data, 0, 0, w, h );
	CNFGSwapBuffersInternal();
}
void	CNFGSetLineWidth( short width )
{
	//Rasterizer does not support line width.
}
void CNFGHandleInput()
{
	//Do nothing.
	//Input is handled on swap frame.
}
#endif
#else
//Copyright (c) 2011, 2017, 2018 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
//portions from 
//http://www.xmission.com/~georgeps/documentation/tutorials/Xlib_Beginner.html
//#define HAS_XINERAMA
//#define CNFG_HAS_XSHAPE
//#define FULL_SCREEN_STEAL_FOCUS
#ifndef _CNFGXDRIVER_C
#define _CNFGXDRIVER_C
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAS_XINERAMA
	#include <X11/extensions/shape.h>
	#include <X11/extensions/Xinerama.h>
#endif
#ifdef CNFG_HAS_XSHAPE
	#include <X11/extensions/shape.h>
	static    XGCValues xsval;
	static    Pixmap xspixmap;
	static    GC xsgc;
	static	int taint_shape;
	static	int prepare_xshape;
	static int was_transp;
#endif
#ifdef CNFG_BATCH
void CNFGSetupBatchInternal();
#endif
XWindowAttributes CNFGWinAtt;
XClassHint *CNFGClassHint;
char *wm_res_name = "rawdraw";
char *wm_res_class = "rawdraw";
Display *CNFGDisplay;
Window CNFGWindow;
int CNFGWindowInvisible;
Pixmap CNFGPixmap;
GC     CNFGGC;
GC     CNFGWindowGC;
Visual * CNFGVisual;
int CNFGX11ForceNoDecoration;
XImage *xi;
int g_x_global_key_state;
int g_x_global_shift_key;
void 	CNFGSetWindowIconData( int w, int h, uint32_t * data )
{
	static Atom net_wm_icon;
	static Atom cardinal; 
	if( !net_wm_icon ) net_wm_icon = XInternAtom( CNFGDisplay, "_NET_WM_ICON", False );
	if( !cardinal ) cardinal = XInternAtom( CNFGDisplay, "CARDINAL", False );
	unsigned long outdata[w*h];
	int i;
	for( i = 0; i < w*h; i++ )
	{
		outdata[i+2] = data[i];
	}
	outdata[0] = w;
	outdata[1] = h;
	XChangeProperty(CNFGDisplay, CNFGWindow, net_wm_icon, cardinal,
		32, PropModeReplace, (const unsigned char*)outdata, 2 + w*h);
}
#ifdef CNFG_HAS_XSHAPE
void	CNFGPrepareForTransparency() { prepare_xshape = 1; }
void	CNFGDrawToTransparencyMode( int transp )
{
	static Pixmap BackupCNFGPixmap;
	static GC     BackupCNFGGC;
	if( was_transp && ! transp )
	{
		CNFGGC = BackupCNFGGC;
		CNFGPixmap = BackupCNFGPixmap;
	}
	if( !was_transp && transp )
	{
		BackupCNFGPixmap = CNFGPixmap;
		BackupCNFGGC = CNFGGC;
		taint_shape = 1;
		CNFGGC = xsgc;
		CNFGPixmap = xspixmap;
	}
	was_transp = transp;
}
void	CNFGClearTransparencyLevel()
{
	taint_shape = 1;
	XSetForeground(CNFGDisplay, xsgc, 0);
	XFillRectangle(CNFGDisplay, xspixmap, xsgc, 0, 0, CNFGWinAtt.width, CNFGWinAtt.height);
	XSetForeground(CNFGDisplay, xsgc, 1);
}
#endif
#ifdef CNFGOGL
#include <GL/glx.h>
#include <GL/glxext.h>
GLXContext CNFGCtx;
void * CNFGGetExtension( const char * extname ) { return (void*)glXGetProcAddressARB((const GLubyte *) extname); }
#endif
int FullScreen = 0;
void CNFGGetDimensions( short * x, short * y )
{
	static int lastx;
	static int lasty;
	*x = CNFGWinAtt.width;
	*y = CNFGWinAtt.height;
	if( lastx != *x || lasty != *y )
	{
		lastx = *x;
		lasty = *y;
		CNFGInternalResize( lastx, lasty );
	}
}
void	CNFGChangeWindowTitle( const char * WindowName )
{
	XSetStandardProperties( CNFGDisplay, CNFGWindow, WindowName, 0, 0, 0, 0, 0 );
}
static void InternalLinkScreenAndGo( const char * WindowName )
{
	XFlush(CNFGDisplay);
	XGetWindowAttributes( CNFGDisplay, CNFGWindow, &CNFGWinAtt );
	XGetClassHint( CNFGDisplay, CNFGWindow, CNFGClassHint );
	if (!CNFGClassHint) {
		CNFGClassHint = XAllocClassHint();
		if (CNFGClassHint) {
			CNFGClassHint->res_name = wm_res_name;
			CNFGClassHint->res_class = wm_res_class;
			XSetClassHint( CNFGDisplay, CNFGWindow, CNFGClassHint );
		} else {
			fprintf( stderr, "Failed to allocate XClassHint!\n" );
		}
	} else {
		fprintf( stderr, "Pre-existing XClassHint\n" );
	}
	XSelectInput (CNFGDisplay, CNFGWindow, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | ExposureMask | PointerMotionMask );
	CNFGWindowGC = XCreateGC(CNFGDisplay, CNFGWindow, 0, 0);
	if( CNFGX11ForceNoDecoration )
	{
		Atom window_type = XInternAtom(CNFGDisplay, "_NET_WM_WINDOW_TYPE", False);
		long value = XInternAtom(CNFGDisplay, "_NET_WM_WINDOW_TYPE_SPLASH", False);
		XChangeProperty(CNFGDisplay, CNFGWindow, window_type,
		   XA_ATOM, 32, PropModeReplace, (unsigned char *) &value,1 );
	}
	CNFGPixmap = XCreatePixmap( CNFGDisplay, CNFGWindow, CNFGWinAtt.width, CNFGWinAtt.height, CNFGWinAtt.depth );
	CNFGGC = XCreateGC(CNFGDisplay, CNFGPixmap, 0, 0);
	XSetLineAttributes(CNFGDisplay, CNFGGC, 1, LineSolid, CapRound, JoinRound);
	CNFGChangeWindowTitle( WindowName );
	if( !CNFGWindowInvisible )
		XMapWindow(CNFGDisplay, CNFGWindow);
#ifdef CNFG_HAS_XSHAPE
	if( prepare_xshape )
	{
	    xsval.foreground = 1;
	    xsval.line_width = 1;
	    xsval.line_style = LineSolid;
	    xspixmap = XCreatePixmap(CNFGDisplay, CNFGWindow, CNFGWinAtt.width, CNFGWinAtt.height, 1);
	    xsgc = XCreateGC(CNFGDisplay, xspixmap, 0, &xsval);
		XSetLineAttributes(CNFGDisplay, xsgc, 1, LineSolid, CapRound, JoinRound);
	}
#endif
}
void CNFGSetupFullscreen( const char * WindowName, int screen_no )
{
#ifdef HAS_XINERAMA
	XineramaScreenInfo *screeninfo = NULL;
	int screens;
	int event_basep, error_basep, a, b;
	CNFGDisplay = XOpenDisplay(NULL);
	int screen = XDefaultScreen(CNFGDisplay);
	int xpos, ypos;
	if (!XShapeQueryExtension(CNFGDisplay, &event_basep, &error_basep)) {
		fprintf( stderr, "X-Server does not support shape extension\n" );
		exit( 1 );
	}
 	CNFGVisual = DefaultVisual(CNFGDisplay, screen);
	CNFGWinAtt.depth = DefaultDepth(CNFGDisplay, screen);
#ifdef CNFGOGL
	int attribs[] = { GLX_RGBA,
		GLX_DOUBLEBUFFER, 
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		None };
	XVisualInfo * vis = glXChooseVisual(CNFGDisplay, screen, attribs);
	CNFGVisual = vis->visual;
	CNFGWinAtt.depth = vis->depth;
	CNFGCtx = glXCreateContext( CNFGDisplay, vis, NULL, True );
#endif
	if (XineramaQueryExtension(CNFGDisplay, &a, &b ) &&
		(screeninfo = XineramaQueryScreens(CNFGDisplay, &screens)) &&
		XineramaIsActive(CNFGDisplay) && screen_no >= 0 &&
		screen_no < screens ) {
		CNFGWinAtt.width = screeninfo[screen_no].width;
		CNFGWinAtt.height = screeninfo[screen_no].height;
		xpos = screeninfo[screen_no].x_org;
		ypos = screeninfo[screen_no].y_org;
	} else
	{
		CNFGWinAtt.width = XDisplayWidth(CNFGDisplay, screen);
		CNFGWinAtt.height = XDisplayHeight(CNFGDisplay, screen);
		xpos = 0;
		ypos = 0;
	}
	if (screeninfo)
	XFree(screeninfo);
	XSetWindowAttributes setwinattr;
	setwinattr.override_redirect = 1;
	setwinattr.save_under = 1;
#ifdef CNFG_HAS_XSHAPE
	if (prepare_xshape && !XShapeQueryExtension(CNFGDisplay, &event_basep, &error_basep))
	{
    	fprintf( stderr, "X-Server does not support shape extension" );
		exit( 1 );
	}
	setwinattr.event_mask = 0;
#else
	//This code is probably made irrelevant by the XSetEventMask in InternalLinkScreenAndGo, if this code is not found needed by 2019-12-31, please remove.
	//setwinattr.event_mask = StructureNotifyMask | SubstructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonPressMask | PointerMotionMask | ButtonMotionMask | EnterWindowMask | LeaveWindowMask |KeyPressMask |KeyReleaseMask | SubstructureNotifyMask | FocusChangeMask;
#endif
	setwinattr.border_pixel = 0;
	setwinattr.colormap = XCreateColormap( CNFGDisplay, RootWindow(CNFGDisplay, 0), CNFGVisual, AllocNone);
	CNFGWindow = XCreateWindow(CNFGDisplay, XRootWindow(CNFGDisplay, screen),
		xpos, ypos, CNFGWinAtt.width, CNFGWinAtt.height,
		0, CNFGWinAtt.depth, InputOutput, CNFGVisual, 
		CWBorderPixel/* | CWEventMask */ | CWOverrideRedirect | CWSaveUnder | CWColormap, 
		&setwinattr);
	FullScreen = 1;
	InternalLinkScreenAndGo( WindowName );
#ifdef CNFGOGL
	glXMakeCurrent( CNFGDisplay, CNFGWindow, CNFGCtx );
#endif
#else
	CNFGSetup( WindowName, 640, 480 );
#endif
}
void CNFGTearDown()
{
	HandleDestroy();
	if( xi ) free( xi );
	if ( CNFGClassHint ) XFree( CNFGClassHint );
	if ( CNFGGC ) XFreeGC( CNFGDisplay, CNFGGC );
	if ( CNFGWindowGC ) XFreeGC( CNFGDisplay, CNFGWindowGC );
	if ( CNFGDisplay ) XCloseDisplay( CNFGDisplay );
	CNFGDisplay = NULL;
	CNFGWindowGC = CNFGGC = NULL;
	CNFGClassHint = NULL;
}
int CNFGSetupWMClass( const char * WindowName, int w, int h , char * wm_res_name_ , char * wm_res_class_ )
{
	wm_res_name = wm_res_name_;
	wm_res_class = wm_res_class_;
	return CNFGSetup( WindowName, w, h);
}
int CNFGSetup( const char * WindowName, int w, int h )
{
	CNFGDisplay = XOpenDisplay(NULL);
	if ( !CNFGDisplay ) {
		fprintf( stderr, "Could not get an X Display.\n%s", 
				 "Are you in text mode or using SSH without X11-Forwarding?\n" );
		exit( 1 );
	}
	atexit( CNFGTearDown );
	int screen = DefaultScreen(CNFGDisplay);
	int depth = DefaultDepth(CNFGDisplay, screen);
 	CNFGVisual = DefaultVisual(CNFGDisplay, screen);
	Window wnd = DefaultRootWindow( CNFGDisplay );
#ifdef CNFGOGL
	int attribs[] = { GLX_RGBA,
		GLX_DOUBLEBUFFER, 
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		None };
	XVisualInfo * vis = glXChooseVisual(CNFGDisplay, screen, attribs);
	CNFGVisual = vis->visual;
	depth = vis->depth;
	CNFGCtx = glXCreateContext( CNFGDisplay, vis, NULL, True );
#endif
	XSetWindowAttributes attr;
	attr.background_pixel = 0;
	attr.colormap = XCreateColormap( CNFGDisplay, wnd, CNFGVisual, AllocNone);
	if( w && h )
		CNFGWindow = XCreateWindow(CNFGDisplay, wnd, 1, 1, w, h, 0, depth, InputOutput, CNFGVisual, CWBackPixel | CWColormap, &attr );
	else
	{
		CNFGWindow = XCreateWindow(CNFGDisplay, wnd, 1, 1, 1, 1, 0, depth, InputOutput, CNFGVisual, CWBackPixel | CWColormap, &attr );
		CNFGWindowInvisible = 1;
	}
	InternalLinkScreenAndGo( WindowName );
//Not sure of the purpose of this code - if it's still commented out after 2019-12-31 and no one knows why, please delete it.
//	Atom WM_DELETE_WINDOW = XInternAtom( CNFGDisplay, "WM_DELETE_WINDOW", False );
//	XSetWMProtocols( CNFGDisplay, CNFGWindow, &WM_DELETE_WINDOW, 1 );
#ifdef CNFGOGL
	glXMakeCurrent( CNFGDisplay, CNFGWindow, CNFGCtx );
#endif
#ifdef CNFG_BATCH
	CNFGSetupBatchInternal();
#endif
	return 0;
}
void CNFGHandleInput()
{
	if( !CNFGWindow ) return;
	static int ButtonsDown;
	XEvent report;
	int bKeyDirection = 1;
	while( XPending( CNFGDisplay ) )
	{
		XNextEvent( CNFGDisplay, &report );
		bKeyDirection = 1;
		switch  (report.type)
		{
		case NoExpose:
			break;
		case Expose:
			XGetWindowAttributes( CNFGDisplay, CNFGWindow, &CNFGWinAtt );
			if( CNFGPixmap ) XFreePixmap( CNFGDisplay, CNFGPixmap );
			CNFGPixmap = XCreatePixmap( CNFGDisplay, CNFGWindow, CNFGWinAtt.width, CNFGWinAtt.height, CNFGWinAtt.depth );
			if( CNFGGC ) XFreeGC( CNFGDisplay, CNFGGC );
			CNFGGC = XCreateGC(CNFGDisplay, CNFGPixmap, 0, 0);
			break;
		case KeyRelease:
		{
			bKeyDirection = 0;
			//Tricky - handle key repeats cleanly.
			if( XPending( CNFGDisplay ) )
			{
				XEvent nev;
				XPeekEvent( CNFGDisplay, &nev );
				if (nev.type == KeyPress && nev.xkey.time == report.xkey.time && nev.xkey.keycode == report.xkey.keycode )
					bKeyDirection = 2;
			}
		}
		case KeyPress:
			g_x_global_key_state = report.xkey.state;
			g_x_global_shift_key = XLookupKeysym(&report.xkey, 1);
			HandleKey( XLookupKeysym(&report.xkey, 0), bKeyDirection );
			break;
		case ButtonRelease:
			bKeyDirection = 0;
		case ButtonPress:
			HandleButton( report.xbutton.x, report.xbutton.y, report.xbutton.button, bKeyDirection );
			ButtonsDown = (ButtonsDown & (~(1<<report.xbutton.button))) | ( bKeyDirection << report.xbutton.button );
			//Intentionall fall through -- we want to send a motion in event of a button as well.
		case MotionNotify:
			HandleMotion( report.xmotion.x, report.xmotion.y, ButtonsDown>>1 );
			break;
		case ClientMessage:
			// Only subscribed to WM_DELETE_WINDOW, so just exit
			exit( 0 );
			break;
		default:
			break;
			//printf( "Event: %d\n", report.type );
		}
	}
}
#ifdef CNFGOGL
void   CNFGSetVSync( int vson )
{
	void (*glfn)( int );
	glfn = (void (*)( int ))CNFGGetExtension( "glXSwapIntervalMESA" );	if( glfn ) glfn( vson );
	glfn = (void (*)( int ))CNFGGetExtension( "glXSwapIntervalSGI" );	if( glfn ) glfn( vson );
	glfn = (void (*)( int ))CNFGGetExtension( "glXSwapIntervalEXT" );	if( glfn ) glfn( vson );
}
#ifdef CNFGRASTERIZER
void CNFGSwapBuffersInternal()
#else
void CNFGSwapBuffers()
#endif
{
	if( CNFGWindowInvisible ) return;
#ifndef CNFGRASTERIZER
	CNFGFlushRender();
#endif
#ifdef CNFG_HAS_XSHAPE
	if( taint_shape )
	{
		XShapeCombineMask(CNFGDisplay, CNFGWindow, ShapeBounding, 0, 0, xspixmap, ShapeSet);
		taint_shape = 0;
	}
#endif //CNFG_HAS_XSHAPE
	glXSwapBuffers( CNFGDisplay, CNFGWindow );
#ifdef FULL_SCREEN_STEAL_FOCUS
	if( FullScreen )
		XSetInputFocus( CNFGDisplay, CNFGWindow, RevertToParent, CurrentTime );
#endif //FULL_SCREEN_STEAL_FOCUS
}
#else //CNFGOGL
#ifndef CNFGRASTERIZER
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	static int depth;
	static int lw, lh;
	if( !xi )
	{
		int screen = DefaultScreen(CNFGDisplay);
		depth = DefaultDepth(CNFGDisplay, screen)/8;
	}
	if( lw != w || lh != h )
	{
		if( xi ) free( xi );
		xi = XCreateImage(CNFGDisplay, CNFGVisual, depth*8, ZPixmap, 0, (char*)data, w, h, 32, w*4 );
		lw = w;
		lh = h;
	}
	//Draw image to pixmap (not a screen flip)
	XPutImage(CNFGDisplay, CNFGPixmap, CNFGGC, xi, 0, 0, x, y, w, h );
}
#endif
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h )
{
	static int depth;
	static int lw, lh;
	if( !xi )
	{
		int screen = DefaultScreen(CNFGDisplay);
		depth = DefaultDepth(CNFGDisplay, screen)/8;
//		xi = XCreateImage(CNFGDisplay, DefaultVisual( CNFGDisplay, DefaultScreen(CNFGDisplay) ), depth*8, ZPixmap, 0, (char*)data, w, h, 32, w*4 );
//		lw = w;
//		lh = h;
	}
	if( lw != w || lh != h )
	{
		if( xi ) free( xi );
		xi = XCreateImage(CNFGDisplay, CNFGVisual, depth*8, ZPixmap, 0, (char*)data, w, h, 32, w*4 );
		lw = w;
		lh = h;
	}
	//Directly write image to screen (effectively a flip)
	XPutImage(CNFGDisplay, CNFGWindow, CNFGWindowGC, xi, 0, 0, 0, 0, w, h );
}
#endif //CNFGOGL
#if !defined( CNFGOGL)
#define AGLF(x) x
#else
#define AGLF(x) static inline BACKEND_##x
#endif
#if defined( CNFGRASTERIZER ) 
//Don't call this file yourself.  It is intended to be included in any drivers which want to support the rasterizer plugin.
#ifdef CNFGRASTERIZER
//#include <stdlib.h>
#include <stdint.h>
static uint32_t * buffer = 0;
static short bufferx;
static short buffery;
#ifdef CNFGOGL
void CNFGFlushRender()
{
}
#endif
void CNFGInternalResize( short x, short y )
{
	bufferx = x;
	buffery = y;
	if( buffer ) free( buffer );
	buffer = malloc( bufferx * buffery * 4 );
#ifdef CNFGOGL
	void CNFGInternalResizeOGLBACKEND( short w, short h );
	CNFGInternalResizeOGLBACKEND( x, y );
#endif
}
#ifdef __wasm__
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<24;
	r>>=8;
	ret |= (r&0xff)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff)<<0;
	return ret;
}
#elif !defined(CNFGOGL)
#define SWAPS(x) (x>>8)
#else
static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= (r&0xff);
	r>>=8;
	ret |= (r&0xff)<<24;
	return ret;
}
#endif
uint32_t CNFGColor( uint32_t RGB )
{
	CNFGLastColor = SWAPS(RGB);
	return CNFGLastColor;
}
void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	short tx, ty;
	float slope, lp;
	short dx = x2 - x1;
	short dy = y2 - y1;
	if( !buffer ) return;
	if( dx < 0 ) dx = -dx;
	if( dy < 0 ) dy = -dy;
	if( dx > dy )
	{
		short minx = (x1 < x2)?x1:x2;
		short maxx = (x1 < x2)?x2:x1;
		short miny = (x1 < x2)?y1:y2;
		short maxy = (x1 < x2)?y2:y1;
		float thisy = miny;
		slope = (float)(maxy-miny) / (float)(maxx-minx);
		for( tx = minx; tx <= maxx; tx++ )
		{
			ty = thisy;
			if( tx < 0 || ty < 0 || ty >= buffery ) continue;
			if( tx >= bufferx ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisy += slope;
		}
	}
	else
	{
		short minx = (y1 < y2)?x1:x2;
		short maxx = (y1 < y2)?x2:x1;
		short miny = (y1 < y2)?y1:y2;
		short maxy = (y1 < y2)?y2:y1;
		float thisx = minx;
		slope = (float)(maxx-minx) / (float)(maxy-miny);
		for( ty = miny; ty <= maxy; ty++ )
		{
			tx = thisx;
			if( ty < 0 || tx < 0 || tx >= bufferx ) continue;
			if( ty >= buffery ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisx += slope;
		}
	}
}
void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	short minx = (x1<x2)?x1:x2;
	short miny = (y1<y2)?y1:y2;
	short maxx = (x1>=x2)?x1:x2;
	short maxy = (y1>=y2)?y1:y2;
	short x, y;
	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx >= bufferx ) maxx = bufferx-1;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		uint32_t * bufferstart = &buffer[y * bufferx + minx];
		for( x = minx; x <= maxx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
}
void CNFGTackPoly( RDPoint * points, int verts )
{
	short minx = 10000, miny = 10000;
	short maxx =-10000, maxy =-10000;
	short i, x, y;
	//Just in case...
	if( verts > 32767 ) return;
	for( i = 0; i < verts; i++ )
	{
		RDPoint * p = &points[i];
		if( p->x < minx ) minx = p->x;
		if( p->y < miny ) miny = p->y;
		if( p->x > maxx ) maxx = p->x;
		if( p->y > maxy ) maxy = p->y;
	}
	if( miny < 0 ) miny = 0;
	if( maxy >= buffery ) maxy = buffery-1;
	for( y = miny; y <= maxy; y++ )
	{
		short startfillx = maxx;
		short endfillx = minx;
		//Figure out what line segments intersect this line.
		for( i = 0; i < verts; i++ )
		{
			short pl = i + 1;
			if( pl == verts ) pl = 0;
			RDPoint ptop;
			RDPoint pbot;
			ptop.x = points[i].x;
			ptop.y = points[i].y;
			pbot.x = points[pl].x;
			pbot.y = points[pl].y;
//printf( "Poly: %d %d\n", pbot.y, ptop.y );
			if( pbot.y < ptop.y )
			{
				RDPoint ptmp;
				ptmp.x = pbot.x;
				ptmp.y = pbot.y;
				pbot.x = ptop.x;
				pbot.y = ptop.y;
				ptop.x = ptmp.x;
				ptop.y = ptmp.y;
			}
			//Make sure this line segment is within our range.
//printf( "PT: %d %d %d\n", y, ptop.y, pbot.y );
			if( ptop.y <= y && pbot.y >= y )
			{
				short diffy = pbot.y - ptop.y;
				uint32_t placey = (uint32_t)(y - ptop.y)<<16;  //Scale by 16 so we can do integer math.
				short diffx = pbot.x - ptop.x;
				short isectx;
				if( diffy == 0 )
				{
					if( pbot.x < ptop.x )
					{
						if( startfillx > pbot.x ) startfillx = pbot.x;
						if( endfillx < ptop.x ) endfillx = ptop.x;
					}
					else
					{
						if( startfillx > ptop.x ) startfillx = ptop.x;
						if( endfillx < pbot.x ) endfillx = pbot.x;
					}
				}
				else
				{
					//Inner part is scaled by 65536, outer part must be scaled back.
					isectx = (( (placey / diffy) * diffx + 32768 )>>16) + ptop.x;
					if( isectx < startfillx ) startfillx = isectx;
					if( isectx > endfillx ) endfillx = isectx;
				}
//printf( "R: %d %d %d\n", pbot.x, ptop.x, isectx );
			}
		}
//printf( "%d %d %d\n", y, startfillx, endfillx );
		if( endfillx >= bufferx ) endfillx = bufferx - 1;
		if( endfillx >= bufferx ) endfillx = buffery - 1;
		if( startfillx < 0 ) startfillx = 0;
		if( startfillx < 0 ) startfillx = 0;
		unsigned int * bufferstart = &buffer[y * bufferx + startfillx];
		for( x = startfillx; x <= endfillx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
//exit(1);
}
void CNFGClearFrame()
{
	int i, m;
	uint32_t col = 0;
	short x, y;
	CNFGGetDimensions( &x, &y );
	if( x != bufferx || y != buffery || !buffer )
	{
		bufferx = x;
		buffery = y;
		buffer = malloc( x * y * 8 );
	}
	m = x * y;
	col = CNFGColor( CNFGBGColor );
	for( i = 0; i < m; i++ )
	{
//printf( "Got: %d %p %d\n", m, buffer, i );
		buffer[i] = col;
	}
}
void CNFGTackPixel( short x, short y )
{
	if( x < 0 || y < 0 || x >= bufferx || y >= buffery ) return;
	buffer[x+bufferx*y] = CNFGLastColor;
}
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
{
	int ox = x;
	int stride = w;
	if( w <= 0 || h <= 0 || x >= bufferx || y >= buffery ) return;
	if( x < 0 ) { w += x; x = 0; }
	if( y < 0 ) { h += y; y = 0; }
	//Switch w,h to x2, y2
	h += y;
	w += x;
	if( w >= bufferx ) { w = bufferx; }
	if( h >= buffery ) { h = buffery; }
	for( ; y < h-1; y++ )
	{
		x = ox;
		uint32_t * indat = data;
		uint32_t * outdat = buffer + y * bufferx + x;
		for( ; x < w-1; x++ )
		{
			uint32_t newm = *(indat++);
			uint32_t oldm = *(outdat);
			if( (newm & 0xff) == 0xff )
			{
				*(outdat++) = newm;
			}
			else
			{
				//Alpha blend.
				int alfa = newm&0xff;
				int onemalfa = 255-alfa;
#ifdef __wasm__
				uint32_t newv = 255<<0; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#elif defined( ANDROID ) || defined( __android__ )
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#elif defined( CNFGOGL ) //OGL, on X11
				uint32_t newv = 255<<16; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>24)&0xff) * onemalfa + 128)>>8)<<24;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
#else //X11
				uint32_t newv = 255<<24; //Alpha, then RGB
				newv |= ((((newm>>24)&0xff) * alfa + ((oldm>>16)&0xff) * onemalfa + 128)>>8)<<16;
				newv |= ((((newm>>16)&0xff) * alfa + ((oldm>>8)&0xff) * onemalfa + 128)>>8)<<8;
				newv |= ((((newm>>8)&0xff) * alfa + ((oldm>>0)&0xff) * onemalfa + 128)>>8)<<0;
#endif
				*(outdat++) = newv;
			}
		}
		data += stride;
	}
}
void CNFGSwapBuffers()
{
	CNFGUpdateScreenWithBitmap( (uint32_t*)buffer, bufferx, buffery );
}
#endif
#undef AGLF
#define AGLF(x) static inline BACKEND_##x
#endif
uint32_t AGLF(CNFGColor)( uint32_t RGB )
{
	unsigned char red = ( RGB >> 24 ) & 0xFF;
	unsigned char grn = ( RGB >> 16 ) & 0xFF;
	unsigned char blu = ( RGB >> 8 ) & 0xFF;
	unsigned long color = (red<<16)|(grn<<8)|(blu);
	CNFGLastColor = color;
	XSetForeground(CNFGDisplay, CNFGGC, color);
	return color;
}
void AGLF(CNFGClearFrame)()
{
	XGetWindowAttributes( CNFGDisplay, CNFGWindow, &CNFGWinAtt );
	XSetForeground(CNFGDisplay, CNFGGC, CNFGColor(CNFGBGColor) );	
	XFillRectangle(CNFGDisplay, CNFGPixmap, CNFGGC, 0, 0, CNFGWinAtt.width, CNFGWinAtt.height );
}
void AGLF(CNFGSwapBuffers)()
{
#ifdef CNFG_HAS_XSHAPE
	if( taint_shape )
	{
		XShapeCombineMask(CNFGDisplay, CNFGWindow, ShapeBounding, 0, 0, xspixmap, ShapeSet);
		taint_shape = 0;
	}
#endif
	XCopyArea(CNFGDisplay, CNFGPixmap, CNFGWindow, CNFGWindowGC, 0,0,CNFGWinAtt.width,CNFGWinAtt.height,0,0);
	XFlush(CNFGDisplay);
#ifdef FULL_SCREEN_STEAL_FOCUS
	if( FullScreen )
		XSetInputFocus( CNFGDisplay, CNFGWindow, RevertToParent, CurrentTime );
#endif
}
void AGLF(CNFGTackSegment)( short x1, short y1, short x2, short y2 )
{
	if( x1 == x2 && y1 == y2 )
	{
		//On some targets, zero-length lines will not show up.
		//This is tricky - since this will also cause more draw calls for points on systems like GLAMOR.
		XDrawPoint( CNFGDisplay, CNFGPixmap, CNFGGC, x2, y2 );
	}
	else
	{
		//XXX HACK!  See discussion here: https://github.com/cntools/cnping/issues/68
		XDrawLine( CNFGDisplay, CNFGPixmap, CNFGGC, x1, y1, x2, y2 );
		XDrawLine( CNFGDisplay, CNFGPixmap, CNFGGC, x2, y2, x1, y1 );
	}
}
void AGLF(CNFGTackPixel)( short x1, short y1 )
{
	XDrawPoint( CNFGDisplay, CNFGPixmap, CNFGGC, x1, y1 );
}
void AGLF(CNFGTackRectangle)( short x1, short y1, short x2, short y2 )
{
	XFillRectangle(CNFGDisplay, CNFGPixmap, CNFGGC, x1, y1, x2-x1, y2-y1 );
}
void AGLF(CNFGTackPoly)( RDPoint * points, int verts )
{
	XFillPolygon(CNFGDisplay, CNFGPixmap, CNFGGC, (XPoint *)points, 3, Convex, CoordModeOrigin );
}
void AGLF(CNFGInternalResize)( short x, short y ) { }
void AGLF(CNFGSetLineWidth)( short width )
{
	XSetLineAttributes(CNFGDisplay, CNFGGC, width, LineSolid, CapRound, JoinRound);
}
#endif // _CNFGXDRIVER_C
#endif
/*
Copyright (c) 2010-2020 <>< Charles Lohr
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef _CNFG_C
#define _CNFG_C
#ifdef _CNFG_FANCYFONT
#endif
int CNFGPenX, CNFGPenY;
uint32_t CNFGBGColor;
uint32_t CNFGLastColor;
//uint32_t CNFGDialogColor; //background for boxes [DEPRECATED]
// The following two arrays are generated by Fonter/fonter.cpp
const unsigned short RawdrawFontCharMap[256] = {
	65535, 0, 8, 16, 24, 31, 41, 50, 51, 65535, 65535, 57, 66, 65535, 75, 83,
	92, 96, 100, 108, 114, 123, 132, 137, 147, 152, 158, 163, 169, 172, 178, 182, 
	65535, 186, 189, 193, 201, 209, 217, 226, 228, 232, 236, 244, 248, 250, 252, 253, 
	255, 261, 266, 272, 278, 283, 289, 295, 300, 309, 316, 318, 321, 324, 328, 331, 
	337, 345, 352, 362, 368, 375, 382, 388, 396, 402, 408, 413, 422, 425, 430, 435, 
	442, 449, 458, 466, 472, 476, 480, 485, 492, 500, 507, 512, 516, 518, 522, 525, 
	527, 529, 536, 541, 546, 551, 557, 564, 572, 578, 581, 586, 593, 595, 604, 610, 
	615, 621, 627, 632, 638, 642, 648, 653, 660, 664, 670, 674, 680, 684, 690, 694, 
	65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 
	65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 
	700, 703, 711, 718, 731, 740, 744, 754, 756, 760, 766, 772, 775, 777, 785, 787, 
	792, 798, 803, 811, 813, 820, 827, 828, 831, 833, 838, 844, 853, 862, 874, 880, 
	889, 898, 908, 919, 928, 939, 951, 960, 969, 978, 988, 997, 1005, 1013, 1022, 1030,
	1039, 1047, 1054, 1061, 1070, 1079, 1086, 1090, 1099, 1105, 1111, 1118, 1124, 1133, 1140, 1150,
	1159, 1168, 1178, 1189, 1198, 1209, 1222, 1231, 1239, 1247, 1256, 1264, 1268, 1272, 1277, 1281,
	1290, 1300, 1307, 1314, 1322, 1331, 1338, 1342, 1349, 1357, 1365, 1374, 1382, 1390, 1397, 65535, };
const unsigned char RawdrawFontCharData[1405] = {
	0x00, 0x09, 0x20, 0x29, 0x03, 0x23, 0x14, 0x8b, 0x00, 0x09, 0x20, 0x29, 0x04, 0x24, 0x13, 0x8c, 
	0x01, 0x21, 0x23, 0x14, 0x03, 0x09, 0x11, 0x9a, 0x11, 0x22, 0x23, 0x14, 0x03, 0x02, 0x99, 0x01, 
	0x21, 0x23, 0x09, 0x03, 0x29, 0x03, 0x09, 0x12, 0x9c, 0x03, 0x2b, 0x13, 0x1c, 0x23, 0x22, 0x11, 
	0x02, 0x8b, 0x9a, 0x1a, 0x01, 0x21, 0x23, 0x03, 0x89, 0x03, 0x21, 0x2a, 0x21, 0x19, 0x03, 0x14, 
	0x23, 0x9a, 0x01, 0x10, 0x21, 0x12, 0x09, 0x12, 0x1c, 0x03, 0xab, 0x02, 0x03, 0x1b, 0x02, 0x1a, 
	0x13, 0x10, 0xa9, 0x01, 0x2b, 0x03, 0x29, 0x02, 0x11, 0x22, 0x13, 0x8a, 0x00, 0x22, 0x04, 0x88, 
	0x20, 0x02, 0x24, 0xa8, 0x01, 0x10, 0x29, 0x10, 0x14, 0x0b, 0x14, 0xab, 0x00, 0x0b, 0x0c, 0x20, 
	0x2b, 0xac, 0x00, 0x28, 0x00, 0x02, 0x2a, 0x10, 0x1c, 0x20, 0xac, 0x01, 0x21, 0x23, 0x03, 0x09, 
	0x20, 0x10, 0x14, 0x8c, 0x03, 0x23, 0x24, 0x04, 0x8b, 0x01, 0x10, 0x29, 0x10, 0x14, 0x0b, 0x14, 
	0x2b, 0x04, 0xac, 0x01, 0x18, 0x21, 0x10, 0x9c, 0x03, 0x1c, 0x23, 0x1c, 0x10, 0x9c, 0x02, 0x22, 
	0x19, 0x22, 0x9b, 0x02, 0x2a, 0x02, 0x19, 0x02, 0x9b, 0x01, 0x02, 0xaa, 0x02, 0x22, 0x11, 0x02, 
	0x13, 0xaa, 0x11, 0x22, 0x02, 0x99, 0x02, 0x13, 0x22, 0x8a, 0x10, 0x1b, 0x9c, 0x10, 0x09, 0x20, 
	0x99, 0x10, 0x1c, 0x20, 0x2c, 0x01, 0x29, 0x03, 0xab, 0x21, 0x10, 0x01, 0x23, 0x14, 0x0b, 0x10, 
	0x9c, 0x00, 0x09, 0x23, 0x2c, 0x04, 0x03, 0x21, 0xa8, 0x21, 0x10, 0x01, 0x12, 0x03, 0x14, 0x2b, 
	0x02, 0xac, 0x10, 0x99, 0x10, 0x01, 0x03, 0x9c, 0x10, 0x21, 0x23, 0x9c, 0x01, 0x2b, 0x11, 0x1b, 
	0x21, 0x0b, 0x02, 0xaa, 0x02, 0x2a, 0x11, 0x9b, 0x04, 0x9b, 0x02, 0xaa, 0x9c, 0x03, 0xa9, 0x00, 
	0x20, 0x24, 0x04, 0x08, 0x9a, 0x01, 0x10, 0x1c, 0x04, 0xac, 0x01, 0x10, 0x21, 0x22, 0x04, 0xac, 
	0x00, 0x20, 0x24, 0x0c, 0x12, 0xaa, 0x00, 0x02, 0x2a, 0x20, 0xac, 0x20, 0x00, 0x02, 0x22, 0x24, 
	0x8c, 0x20, 0x02, 0x22, 0x24, 0x04, 0x8a, 0x00, 0x20, 0x21, 0x12, 0x9c, 0x00, 0x0c, 0x00, 0x20, 
	0x2c, 0x04, 0x2c, 0x02, 0xaa, 0x00, 0x02, 0x22, 0x20, 0x08, 0x22, 0x8c, 0x19, 0x9b, 0x19, 0x13, 
	0x8c, 0x20, 0x02, 0xac, 0x01, 0x29, 0x03, 0xab, 0x00, 0x22, 0x8c, 0x01, 0x10, 0x21, 0x12, 0x1b, 
	0x9c, 0x21, 0x01, 0x04, 0x24, 0x22, 0x12, 0x13, 0xab, 0x04, 0x01, 0x10, 0x21, 0x2c, 0x02, 0xaa, 
	0x00, 0x04, 0x14, 0x23, 0x12, 0x0a, 0x12, 0x21, 0x10, 0x88, 0x23, 0x14, 0x03, 0x01, 0x10, 0xa9, 
	0x00, 0x10, 0x21, 0x23, 0x14, 0x04, 0x88, 0x00, 0x04, 0x2c, 0x00, 0x28, 0x02, 0x9a, 0x00, 0x0c, 
	0x00, 0x28, 0x02, 0x9a, 0x21, 0x10, 0x01, 0x03, 0x14, 0x23, 0x22, 0x9a, 0x00, 0x0c, 0x20, 0x2c, 
	0x02, 0xaa, 0x00, 0x28, 0x10, 0x1c, 0x04, 0xac, 0x00, 0x20, 0x23, 0x14, 0x8b, 0x00, 0x0c, 0x02, 
	0x12, 0x21, 0x28, 0x12, 0x23, 0xac, 0x00, 0x04, 0xac, 0x04, 0x00, 0x11, 0x20, 0xac, 0x04, 0x00, 
	0x2a, 0x20, 0xac, 0x01, 0x10, 0x21, 0x23, 0x14, 0x03, 0x89, 0x00, 0x0c, 0x00, 0x10, 0x21, 0x12, 
	0x8a, 0x01, 0x10, 0x21, 0x23, 0x14, 0x03, 0x09, 0x04, 0x9b, 0x00, 0x0c, 0x00, 0x10, 0x21, 0x12, 
	0x02, 0xac, 0x21, 0x10, 0x01, 0x23, 0x14, 0x8b, 0x00, 0x28, 0x10, 0x9c, 0x00, 0x04, 0x24, 0xa8, 
	0x00, 0x03, 0x14, 0x23, 0xa8, 0x00, 0x04, 0x2c, 0x14, 0x1b, 0x24, 0xa8, 0x00, 0x01, 0x23, 0x2c, 
	0x04, 0x03, 0x21, 0xa8, 0x00, 0x01, 0x12, 0x1c, 0x12, 0x21, 0xa8, 0x00, 0x20, 0x02, 0x04, 0xac, 
	0x10, 0x00, 0x04, 0x9c, 0x01, 0xab, 0x10, 0x20, 0x24, 0x9c, 0x01, 0x10, 0xa9, 0x04, 0xac, 0x00, 
	0x99, 0x02, 0x04, 0x24, 0x2a, 0x23, 0x12, 0x8a, 0x00, 0x04, 0x24, 0x22, 0x8a, 0x24, 0x04, 0x03, 
	0x12, 0xaa, 0x20, 0x24, 0x04, 0x02, 0xaa, 0x24, 0x04, 0x02, 0x22, 0x23, 0x9b, 0x04, 0x09, 0x02, 
	0x1a, 0x01, 0x10, 0xa9, 0x23, 0x12, 0x03, 0x14, 0x23, 0x24, 0x15, 0x8c, 0x00, 0x0c, 0x03, 0x12, 
	0x23, 0xac, 0x19, 0x12, 0x9c, 0x2a, 0x23, 0x24, 0x15, 0x8c, 0x00, 0x0c, 0x03, 0x13, 0x2a, 0x13, 
	0xac, 0x10, 0x9c, 0x02, 0x0c, 0x02, 0x1b, 0x12, 0x1c, 0x12, 0x23, 0xac, 0x02, 0x0c, 0x03, 0x12, 
	0x23, 0xac, 0x02, 0x22, 0x24, 0x04, 0x8a, 0x02, 0x0d, 0x04, 0x24, 0x22, 0x8a, 0x02, 0x04, 0x2c, 
	0x25, 0x22, 0x8a, 0x02, 0x0c, 0x03, 0x12, 0xaa, 0x22, 0x02, 0x03, 0x23, 0x24, 0x8c, 0x11, 0x1c, 
	0x02, 0xaa, 0x02, 0x04, 0x14, 0x2b, 0x24, 0xaa, 0x02, 0x03, 0x14, 0x23, 0xaa, 0x02, 0x03, 0x14, 
	0x1a, 0x13, 0x24, 0xaa, 0x02, 0x2c, 0x04, 0xaa, 0x02, 0x03, 0x1c, 0x22, 0x23, 0x8d, 0x02, 0x22, 
	0x04, 0xac, 0x20, 0x10, 0x14, 0x2c, 0x12, 0x8a, 0x10, 0x19, 0x13, 0x9c, 0x00, 0x10, 0x14, 0x0c, 
	0x12, 0xaa, 0x01, 0x10, 0x11, 0xa8, 0x03, 0x04, 0x24, 0x23, 0x12, 0x8b, 0x18, 0x11, 0x9c, 0x21, 
	0x10, 0x01, 0x02, 0x13, 0x2a, 0x10, 0x9b, 0x11, 0x00, 0x04, 0x24, 0x2b, 0x02, 0x9a, 0x01, 0x0a, 
	0x11, 0x29, 0x22, 0x2b, 0x03, 0x1b, 0x02, 0x11, 0x22, 0x13, 0x8a, 0x00, 0x11, 0x28, 0x11, 0x1c, 
	0x02, 0x2a, 0x03, 0xab, 0x10, 0x1a, 0x13, 0x9d, 0x20, 0x00, 0x02, 0x11, 0x2a, 0x02, 0x13, 0x22, 
	0x24, 0x8c, 0x08, 0xa8, 0x20, 0x10, 0x11, 0xa9, 0x10, 0x29, 0x20, 0x21, 0x11, 0x98, 0x11, 0x02, 
	0x1b, 0x21, 0x12, 0xab, 0x01, 0x21, 0xaa, 0x12, 0xaa, 0x10, 0x20, 0x21, 0x19, 0x12, 0x18, 0x11, 
	0xaa, 0x00, 0xa8, 0x01, 0x10, 0x21, 0x12, 0x89, 0x02, 0x2a, 0x11, 0x1b, 0x03, 0xab, 0x01, 0x10, 
	0x21, 0x03, 0xab, 0x01, 0x10, 0x21, 0x12, 0x0a, 0x12, 0x23, 0x8b, 0x11, 0xa8, 0x02, 0x0d, 0x04, 
	0x14, 0x2b, 0x22, 0xac, 0x14, 0x10, 0x01, 0x1a, 0x10, 0x20, 0xac, 0x9a, 0x14, 0x15, 0x8d, 0x20, 
	0xa9, 0x10, 0x20, 0x21, 0x11, 0x98, 0x01, 0x12, 0x0b, 0x11, 0x22, 0x9b, 0x00, 0x09, 0x02, 0x28, 
	0x12, 0x13, 0x2b, 0x22, 0xac, 0x00, 0x09, 0x02, 0x28, 0x12, 0x22, 0x13, 0x14, 0xac, 0x00, 0x10, 
	0x11, 0x09, 0x11, 0x02, 0x28, 0x12, 0x13, 0x2b, 0x22, 0xac, 0x18, 0x11, 0x12, 0x03, 0x14, 0xab, 
	0x04, 0x02, 0x11, 0x22, 0x2c, 0x03, 0x2b, 0x10, 0xa9, 0x04, 0x02, 0x11, 0x22, 0x2c, 0x03, 0x2b, 
	0x01, 0x98, 0x04, 0x02, 0x11, 0x22, 0x2c, 0x03, 0x2b, 0x01, 0x10, 0xa9, 0x04, 0x02, 0x11, 0x22, 
	0x2c, 0x03, 0x2b, 0x01, 0x10, 0x11, 0xa8, 0x04, 0x02, 0x11, 0x22, 0x2c, 0x03, 0x2b, 0x08, 0xa8, 
	0x04, 0x02, 0x11, 0x22, 0x2c, 0x03, 0x2b, 0x00, 0x20, 0x11, 0x88, 0x00, 0x0c, 0x02, 0x2a, 0x00, 
	0x19, 0x10, 0x1c, 0x10, 0x28, 0x14, 0xac, 0x23, 0x14, 0x03, 0x01, 0x10, 0x29, 0x14, 0x15, 0x8d, 
	0x02, 0x2a, 0x02, 0x04, 0x2c, 0x03, 0x1b, 0x00, 0x99, 0x02, 0x2a, 0x02, 0x04, 0x2c, 0x03, 0x1b, 
	0x11, 0xa8, 0x02, 0x2a, 0x02, 0x04, 0x2c, 0x03, 0x1b, 0x01, 0x10, 0xa9, 0x02, 0x2a, 0x02, 0x04, 
	0x2c, 0x03, 0x1b, 0x08, 0xa8, 0x02, 0x2a, 0x12, 0x1c, 0x04, 0x2c, 0x00, 0x99, 0x02, 0x2a, 0x12, 
	0x1c, 0x04, 0x2c, 0x11, 0xa8, 0x02, 0x2a, 0x12, 0x1c, 0x04, 0x2c, 0x01, 0x10, 0xa9, 0x02, 0x2a, 
	0x12, 0x1c, 0x04, 0x2c, 0x28, 0x88, 0x00, 0x10, 0x21, 0x23, 0x14, 0x04, 0x08, 0x02, 0x9a, 0x04, 
	0x02, 0x24, 0x2a, 0x01, 0x10, 0x11, 0xa8, 0x02, 0x22, 0x24, 0x04, 0x0a, 0x00, 0x99, 0x02, 0x22, 
	0x24, 0x04, 0x0a, 0x11, 0xa8, 0x02, 0x22, 0x24, 0x04, 0x0a, 0x11, 0x28, 0x00, 0x99, 0x02, 0x22, 
	0x24, 0x04, 0x0a, 0x01, 0x10, 0x11, 0xa8, 0x01, 0x21, 0x24, 0x04, 0x09, 0x08, 0xa8, 0x01, 0x2b, 
	0x03, 0xa9, 0x01, 0x10, 0x21, 0x23, 0x14, 0x03, 0x09, 0x03, 0xa9, 0x01, 0x04, 0x24, 0x29, 0x11, 
	0xa8, 0x01, 0x04, 0x24, 0x29, 0x00, 0x99, 0x02, 0x04, 0x24, 0x2a, 0x01, 0x10, 0xa9, 0x01, 0x04, 
	0x24, 0x29, 0x08, 0xa8, 0x01, 0x02, 0x13, 0x1c, 0x13, 0x22, 0x29, 0x11, 0xa8, 0x00, 0x0c, 0x01, 
	0x11, 0x22, 0x13, 0x8b, 0x00, 0x0d, 0x00, 0x10, 0x21, 0x1a, 0x02, 0x22, 0x24, 0x8c, 0x02, 0x04, 
	0x24, 0x2a, 0x23, 0x12, 0x0a, 0x00, 0x99, 0x02, 0x04, 0x24, 0x2a, 0x23, 0x12, 0x0a, 0x11, 0xa8, 
	0x02, 0x04, 0x24, 0x2a, 0x23, 0x12, 0x0a, 0x01, 0x10, 0xa9, 0x02, 0x04, 0x24, 0x2a, 0x23, 0x12, 
	0x0a, 0x01, 0x10, 0x11, 0xa8, 0x02, 0x04, 0x24, 0x2a, 0x23, 0x12, 0x0a, 0x09, 0xa9, 0x02, 0x04, 
	0x24, 0x2a, 0x23, 0x12, 0x0a, 0x01, 0x10, 0x21, 0x89, 0x02, 0x1b, 0x02, 0x04, 0x2c, 0x12, 0x1c, 
	0x12, 0x2a, 0x13, 0x2b, 0x22, 0xab, 0x03, 0x04, 0x2c, 0x03, 0x12, 0x2a, 0x14, 0x15, 0x8d, 0x24, 
	0x04, 0x02, 0x22, 0x23, 0x1b, 0x00, 0x99, 0x24, 0x04, 0x02, 0x22, 0x23, 0x1b, 0x11, 0xa8, 0x24, 
	0x04, 0x02, 0x22, 0x23, 0x1b, 0x01, 0x10, 0xa9, 0x24, 0x04, 0x02, 0x22, 0x23, 0x1b, 0x09, 0xa9, 
	0x12, 0x1c, 0x00, 0x99, 0x12, 0x1c, 0x11, 0xa8, 0x12, 0x1c, 0x01, 0x10, 0xa9, 0x12, 0x1c, 0x09, 
	0xa9, 0x00, 0x2a, 0x11, 0x28, 0x02, 0x22, 0x24, 0x04, 0x8a, 0x02, 0x0c, 0x03, 0x12, 0x23, 0x2c, 
	0x01, 0x10, 0x11, 0xa8, 0x02, 0x04, 0x24, 0x22, 0x0a, 0x00, 0x99, 0x02, 0x04, 0x24, 0x22, 0x0a, 
	0x11, 0xa8, 0x02, 0x04, 0x24, 0x22, 0x0a, 0x01, 0x10, 0xa9, 0x02, 0x04, 0x24, 0x22, 0x0a, 0x01, 
	0x10, 0x11, 0xa8, 0x02, 0x04, 0x24, 0x22, 0x0a, 0x09, 0xa9, 0x19, 0x02, 0x2a, 0x9b, 0x02, 0x04, 
	0x24, 0x22, 0x0a, 0x04, 0xaa, 0x02, 0x04, 0x14, 0x2b, 0x24, 0x2a, 0x00, 0x99, 0x02, 0x04, 0x14, 
	0x2b, 0x24, 0x2a, 0x11, 0xa8, 0x02, 0x04, 0x14, 0x2b, 0x24, 0x2a, 0x01, 0x10, 0xa9, 0x02, 0x04, 
	0x14, 0x2b, 0x24, 0x2a, 0x09, 0xa9, 0x02, 0x03, 0x1c, 0x22, 0x23, 0x0d, 0x11, 0xa8, 0x00, 0x0c, 
	0x02, 0x11, 0x22, 0x13, 0x8a, 0x02, 0x03, 0x1c, 0x22, 0x23, 0x0d, 0x09, 0xa9, };
//Set this if you are only using CNFG to create an OpenGL context.
#ifndef CNFGCONTEXTONLY
void CNFGDrawText( const char * text, short scale )
{
	const unsigned char * lmap;
	float iox = (float)CNFGPenX; //x offset
	float ioy = (float)CNFGPenY; //y offset
	int place = 0;
	unsigned short index;
	int bQuit = 0;
	while( text[place] )
	{
		unsigned char c = text[place];
		switch( c )
		{
		case 9: // tab
			iox += 12 * scale;
			break;
		case 10: // linefeed
			iox = (float)CNFGPenX;
			ioy += 6 * scale;
			break;
		default:
			index = RawdrawFontCharMap[c];
			if( index == 65535 )
			{
				iox += 3 * scale;
				break;
			}
			lmap = &RawdrawFontCharData[index];
			short penx, peny;
			unsigned char start_seg = 1;
			do
			{
				unsigned char data = (*(lmap++));
				short x1 = (short)(((data >> 4) & 0x07)*scale + iox);
				short y1 = (short)((data        & 0x07)*scale + ioy);
				if( start_seg )
				{
					penx = x1;
					peny = y1;
					start_seg = 0;
					if( data & 0x08 )
						CNFGTackPixel( x1, y1 );
				}
				else
				{
					CNFGTackSegment( penx, peny, x1, y1 );
					penx = x1;
					peny = y1;
				}
				if( data & 0x08 ) start_seg = 1;
				bQuit = data & 0x80;
			} while( !bQuit );
			iox += 3 * scale;
		}
		place++;
	}
}
#ifndef FONT_CREATION_TOOL
#ifdef _CNFG_FANCYFONT
void CNFGDrawNiceText(const char* text, short scale)
{
	const unsigned char* lmap;
	float iox = (float)CNFGPenX; //x offset
	float ioy = (float)CNFGPenY; //y offset
	int place = 0;
	unsigned short index;
	int bQuit = 0;
	int segmentEnd = 0;
	while (text[place]) {
		unsigned char c = text[place];
		switch (c)
		{
		case 9: // tab
			iox += 16 * scale;
			break;
		case 10: // linefeed
			iox = (float)CNFGPenX;
			ioy += 6 * scale;
			break;
		default:
			index = CharIndex[c];
			if (index == 0) {
				iox += 4 * scale;
				break;
			}
			lmap = &FontData[index];
			short charWidth = ((*lmap) & 0xE0) >> 5; //0b11100000
			short xbase = ((*lmap) & 0x18) >> 3; //0b00011000
			short ybase = (*lmap) & 0x07; //0b00000111
			lmap++;
			do {
				int x1 = ((((*lmap) & 0x38) >> 3) * scale + iox + xbase * scale); //0b00111000
				int y1 = (((*lmap) & 0x07) * scale + ioy + ybase * scale);
				segmentEnd = *lmap & 0x40;
				int x2 = 0;
				int y2 = 0;
				lmap++;
				if (segmentEnd) {
					x2 = x1;
					y2 = y1;
				}
				else {
					x2 = ((((*lmap) & 0x38) >> 3) * scale + iox + xbase * scale);
					y2 = (((*lmap) & 0x07) * scale + ioy + ybase * scale);
				}
				CNFGTackSegment(x1, y1, x2, y2);
				bQuit = *(lmap - 1) & 0x80;
			} while (!bQuit);
			iox += (charWidth + 2) * scale;
			//iox += 8 * scale;
		}
		place++;
	}
}
#endif
#endif
void CNFGGetTextExtents( const char * text, int * w, int * h, int textsize )
{
	int charsx = 0;
	int charsy = 1;
	int charsline = 0;
	const char * s;
	for( s = text; *s; s++ )
	{
		if( *s == '\n' )
		{
			charsline = 0;
			if( *(s+1) )
				charsy++;
		}
		else
		{
			charsline++;
			if( charsline > charsx )
				charsx = charsline;
		}
	}
	*w = charsx * textsize * 3-1*textsize;
	*h = charsy * textsize * 6;
}
#if defined( CNFG_BATCH )
//This is the path by which we convert rawdraw functionality
//into nice batched triangle streams.
//Just FYI we use floats for geometry instead of shorts becase it is harder
//to triangularize a diagonal line int triangles with shorts and have it look good.
void CNFGEmitBackendTriangles( const float * fv, const uint32_t * col, int nr_verts );
//If on WASM, sqrtf is implied. On other platforms, need sqrtf from math.h
#ifdef __wasm__
float sqrtf( float f );
#else
#include <math.h>
#endif
//Geometry batching system - so we can batch geometry and deliver it all at once.
float CNFGVertDataV[CNFG_BATCH*3];
uint32_t CNFGVertDataC[CNFG_BATCH];
int CNFGVertPlace;
static float wgl_last_width_over_2 = .5;
static void EmitQuad( float cx0, float cy0, float cx1, float cy1, float cx2, float cy2, float cx3, float cy3 ) 
{
	//Because quads are really useful, but it's best to keep them all triangles if possible.
	//This lets us draw arbitrary quads.
	if( CNFGVertPlace >= CNFG_BATCH-6 ) CNFGFlushRender();
	float * fv = &CNFGVertDataV[CNFGVertPlace*3];
	fv[0] = cx0; fv[1] = cy0;
	fv[3] = cx1; fv[4] = cy1;
	fv[6] = cx2; fv[7] = cy2;
	fv[9] = cx2; fv[10] = cy2;
	fv[12] = cx1; fv[13] = cy1;
	fv[15] = cx3; fv[16] = cy3;
	uint32_t * col = &CNFGVertDataC[CNFGVertPlace];
	uint32_t color = CNFGLastColor;
	col[0] = color; col[1] = color; col[2] = color; col[3] = color; col[4] = color; col[5] = color;
	CNFGVertPlace += 6;
}
#ifndef CNFGRASTERIZER
void CNFGTackPixel( short x1, short y1 )
{
	x1++; y1++;
	const short l2 = wgl_last_width_over_2;
	const short l2u = wgl_last_width_over_2+0.5;
	EmitQuad( x1-l2u, y1-l2u, x1+l2, y1-l2u, x1-l2u, y1+l2, x1+l2, y1+l2 );
}
void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	float ix1 = x1;
	float iy1 = y1;
	float ix2 = x2;
	float iy2 = y2;
	float dx = ix2-ix1;
	float dy = iy2-iy1;
	float imag = 1./sqrtf(dx*dx+dy*dy);
	dx *= imag;
	dy *= imag;
	float orthox = dy*wgl_last_width_over_2;
	float orthoy =-dx*wgl_last_width_over_2;
	ix2 += dx/2 + 0.5;
	iy2 += dy/2 + 0.5;
	ix1 -= dx/2 - 0.5;
	iy1 -= dy/2 - 0.5;
	//This logic is incorrect. XXX FIXME.
	EmitQuad( (ix1 - orthox), (iy1 - orthoy), (ix1 + orthox), (iy1 + orthoy), (ix2 - orthox), (iy2 - orthoy), ( ix2 + orthox), ( iy2 + orthoy) );
}
void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	float ix1 = x1;
	float iy1 = y1;
	float ix2 = x2;
	float iy2 = y2;
	EmitQuad( ix1,iy1,ix2,iy1,ix1,iy2,ix2,iy2 );
}
void CNFGTackPoly( RDPoint * points, int verts )
{
	int i;
	int tris = verts-2;
	if( CNFGVertPlace >= CNFG_BATCH-tris*3 ) CNFGFlushRender();
	uint32_t color = CNFGLastColor;
	short * ptrsrc =  (short*)points;
	for( i = 0; i < tris; i++ )
	{
		float * fv = &CNFGVertDataV[CNFGVertPlace*3];
		fv[0] = ptrsrc[0];
		fv[1] = ptrsrc[1];
		fv[3] = ptrsrc[i*2+2];
		fv[4] = ptrsrc[i*2+3];
		fv[6] = ptrsrc[i*2+4];
		fv[7] = ptrsrc[i*2+5];
		uint32_t * col = &CNFGVertDataC[CNFGVertPlace];
		col[0] = color;
		col[1] = color;
		col[2] = color;
		CNFGVertPlace += 3;
	}
}
uint32_t CNFGColor( uint32_t RGB )
{
	return CNFGLastColor = RGB;
}
void	CNFGSetLineWidth( short width )
{
	wgl_last_width_over_2 = width/2.0;// + 0.5;
}
#endif
#ifndef __wasm__
//In WASM, Javascript takes over this functionality.
//Shader compilation errors go to stderr.
#include <stdio.h>
#ifndef GL_VERTEX_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_LINK_STATUS                    0x8B82
#define GL_TEXTURE_2D                     0x0DE1
#define GL_CLAMP_TO_EDGE                  0x812F
#define LGLchar char
#else
#define LGLchar GLchar
#endif
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#define CNFGOGL_NEED_EXTENSION
#endif
#ifdef  CNFGOGL_NEED_EXTENSION
// If we are going to be defining our own function pointer call
	#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	// Make sure to use __stdcall on Windows
		#define CHEWTYPEDEF( ret, name, rv, paramcall, ... ) \
			ret (__stdcall *CNFG##name)( __VA_ARGS__ );
	#else
		#define CHEWTYPEDEF( ret, name, rv, paramcall, ... ) \
			ret (*CNFG##name)( __VA_ARGS__ );
	#endif
#else
//If we are going to be defining the real call
#define CHEWTYPEDEF( ret, name, rv, paramcall, ... ) \
	ret name (__VA_ARGS__);
#endif
int (*MyFunc)( int program, const LGLchar *name );
CHEWTYPEDEF( GLint, glGetUniformLocation, return, (program,name), GLuint program, const LGLchar *name )
CHEWTYPEDEF( void, glEnableVertexAttribArray, , (index), GLuint index )
CHEWTYPEDEF( void, glUseProgram, , (program), GLuint program )
CHEWTYPEDEF( void, glGetProgramInfoLog, , (program,maxLength, length, infoLog), GLuint program, GLsizei maxLength, GLsizei *length, LGLchar *infoLog )
CHEWTYPEDEF( void, glGetProgramiv, , (program,pname,params), GLuint program, GLenum pname, GLint *params )
CHEWTYPEDEF( void, glBindAttribLocation, , (program,index,name), GLuint program, GLuint index, const LGLchar *name )
CHEWTYPEDEF( void, glGetShaderiv, , (shader,pname,params), GLuint shader, GLenum pname, GLint *params )
CHEWTYPEDEF( GLuint, glCreateShader, return, (e), GLenum e )
CHEWTYPEDEF( void, glVertexAttribPointer, , (index,size,type,normalized,stride,pointer), GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer )
CHEWTYPEDEF( void, glShaderSource, , (shader,count,string,length), GLuint shader, GLsizei count, const LGLchar *const*string, const GLint *length )
CHEWTYPEDEF( void, glAttachShader, , (program,shader), GLuint program, GLuint shader )
CHEWTYPEDEF( void, glCompileShader, ,(shader), GLuint shader )
CHEWTYPEDEF( void, glGetShaderInfoLog , , (shader,maxLength, length, infoLog), GLuint shader, GLsizei maxLength, GLsizei *length, LGLchar *infoLog )
CHEWTYPEDEF( GLuint, glCreateProgram, return, () , void )
CHEWTYPEDEF( void, glLinkProgram, , (program), GLuint program )
CHEWTYPEDEF( void, glDeleteShader, , (shader), GLuint shader )
CHEWTYPEDEF( void, glUniform4f, , (location,v0,v1,v2,v3), GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3 )
CHEWTYPEDEF( void, glUniform1i, , (location,i0), GLint location, GLint i0 )
CHEWTYPEDEF( void, glActiveTexture, , (texture), GLenum texture )
#ifndef CNFGOGL_NEED_EXTENSION
#define CNFGglGetUniformLocation glGetUniformLocation
#define CNFGglEnableVertexAttribArray glEnableVertexAttribArray
#define CNFGglUseProgram glUseProgram
#define CNFGglEnableVertexAttribArray glEnableVertexAttribArray
#define CNFGglUseProgram glUseProgram
#define CNFGglGetProgramInfoLog glGetProgramInfoLog
#define CNFGglGetProgramiv glGetProgramiv
#define CNFGglShaderSource glShaderSource
#define CNFGglCreateShader glCreateShader
#define CNFGglAttachShader glAttachShader
#define CNFGglGetShaderiv glGetShaderiv
#define CNFGglCompileShader glCompileShader
#define CNFGglGetShaderInfoLog glGetShaderInfoLog
#define CNFGglCreateProgram glCreateProgram
#define CNFGglLinkProgram glLinkProgram
#define CNFGglDeleteShader glDeleteShader
#define CNFGglUniform4f glUniform4f
#define CNFGglBindAttribLocation glBindAttribLocation
#define CNFGglVertexAttribPointer glVertexAttribPointer
#define CNFGglUniform1i glUniform1i
#define CNFGglActiveTexture glActiveTexture
#endif
#ifdef CNFGOGL_NEED_EXTENSION
#if defined( WIN32 ) || defined( WINDOWS ) || defined( WIN64 )
//From https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions
void * CNFGGetProcAddress(const char *name)
{
	void *p = (void *)LOADED_wglGetProcAddress(name);
	if(p == 0 ||
		(p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) ||
		(p == (void*)-1) )
	{
		static HMODULE module;
		if( !module ) module = LoadLibrary("opengl32.dll");
		p = (void *)GetProcAddress(module, name);
	}
    if (!p)
		MessageBox( 0, name, 0, 0 );
	return p;
}
#else
#include <dlfcn.h>
void * CNFGGetProcAddress(const char *name)
{
	//Tricky use RTLD_NEXT first so we don't accidentally link against ourselves.
	void * v1 = dlsym( (void*)((intptr_t)-1) /*RTLD_NEXT = -1*/ /*RTLD_DEFAULT = 0*/, name );
	//printf( "%s = %p\n", name, v1 );
	if( !v1 ) v1 = dlsym( 0, name );
	return v1;
}
#endif
static void CNFGLoadExtensionsInternal()
{
	CNFGglGetUniformLocation = (GLint(*)(GLuint, const LGLchar*))CNFGGetProcAddress( "glGetUniformLocation" );
	CNFGglEnableVertexAttribArray = (void(*)(GLuint))CNFGGetProcAddress( "glEnableVertexAttribArray" );
	CNFGglUseProgram = (void(*)(GLuint))CNFGGetProcAddress( "glUseProgram" );
	CNFGglGetProgramInfoLog = (void(*)(GLuint, GLsizei, GLsizei*, LGLchar*))CNFGGetProcAddress( "glGetProgramInfoLog" );
	CNFGglBindAttribLocation = (void(*)(GLuint, GLuint, const LGLchar*))CNFGGetProcAddress( "glBindAttribLocation" );
	CNFGglGetProgramiv = (void(*)(GLuint, GLenum, GLint*))CNFGGetProcAddress( "glGetProgramiv" );
	CNFGglGetShaderiv = (void(*)(GLuint, GLenum, GLint*))CNFGGetProcAddress( "glGetShaderiv" );
	CNFGglVertexAttribPointer = (void(*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))CNFGGetProcAddress( "glVertexAttribPointer" );
	CNFGglCreateShader = (GLuint(*)(GLenum))CNFGGetProcAddress( "glCreateShader" );
	// CNFGglVertexAttribPointer = CNFGGetProcAddress( "glVertexAttribPointer" ); // What?
	CNFGglShaderSource = (void(*)(GLuint, GLsizei, const LGLchar* const*, const GLint*))CNFGGetProcAddress( "glShaderSource" );
	CNFGglAttachShader = (void(*)(GLuint, GLuint))CNFGGetProcAddress( "glAttachShader" );
	CNFGglCompileShader = (void(*)(GLuint))CNFGGetProcAddress( "glCompileShader" );
	CNFGglGetShaderInfoLog = (void(*)(GLuint, GLsizei, GLsizei*, LGLchar*))CNFGGetProcAddress( "glGetShaderInfoLog" );
	CNFGglLinkProgram = (void(*)(GLuint))CNFGGetProcAddress( "glLinkProgram" );
	CNFGglDeleteShader = (void(*)(GLuint))CNFGGetProcAddress( "glDeleteShader" );
	CNFGglUniform4f =  (void(*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat))CNFGGetProcAddress( "glUniform4f" );
	CNFGglCreateProgram = (GLuint(*)(void))CNFGGetProcAddress( "glCreateProgram" );
	CNFGglUniform1i = (void(*)(GLint, GLint))CNFGGetProcAddress( "glUniform1i" );
	CNFGglActiveTexture = (void(*)(GLenum))CNFGGetProcAddress("glActiveTexture");
}
#else
static void CNFGLoadExtensionsInternal() { }
#endif
GLuint gRDShaderProg = -1;
GLuint gRDBlitProg = -1;
GLuint gRDShaderProgUX = -1;
GLuint gRDBlitProgUX = -1;
GLuint gRDBlitProgUT = -1;
GLuint gRDBlitProgTex = -1;
GLuint gRDLastResizeW;
GLuint gRDLastResizeH;
GLuint CNFGGLInternalLoadShader( const char * vertex_shader, const char * fragment_shader )
{
	GLuint fragment_shader_object = 0;
	GLuint vertex_shader_object = 0;
	GLuint program = 0;
	int ret;
	vertex_shader_object = CNFGglCreateShader(GL_VERTEX_SHADER); // @@
	if (!vertex_shader_object) {
		fprintf( stderr, "Error: glCreateShader(GL_VERTEX_SHADER) "
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
			"failed: 0x%08X\n", LOADED_glGetError());
#else
			"failed: 0x%08X\n", glGetError());
#endif
		goto fail;
	}
	CNFGglShaderSource(vertex_shader_object, 1, &vertex_shader, NULL);
	CNFGglCompileShader(vertex_shader_object);
	CNFGglGetShaderiv(vertex_shader_object, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		fprintf( stderr,"Error: vertex shader compilation failed!\n");
		CNFGglGetShaderiv(vertex_shader_object, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			char * log = (char*)alloca(ret);
			CNFGglGetShaderInfoLog(vertex_shader_object, ret, NULL, log);
			fprintf( stderr, "%s", log);
		}
		goto fail;
	}
	fragment_shader_object = CNFGglCreateShader(GL_FRAGMENT_SHADER);
	if (!fragment_shader_object) {
		fprintf( stderr, "Error: glCreateShader(GL_FRAGMENT_SHADER) "
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
			"failed: 0x%08X\n", LOADED_glGetError());
#else
			"failed: 0x%08X\n", glGetError());
#endif
		goto fail;
	}
	CNFGglShaderSource(fragment_shader_object, 1, &fragment_shader, NULL);
	CNFGglCompileShader(fragment_shader_object);
	CNFGglGetShaderiv(fragment_shader_object, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		fprintf( stderr, "Error: fragment shader compilation failed!\n");
		CNFGglGetShaderiv(fragment_shader_object, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			char * log = (char*)malloc(ret);
			CNFGglGetShaderInfoLog(fragment_shader_object, ret, NULL, log);
			fprintf( stderr, "%s", log);
		}
		goto fail;
	}
	program = CNFGglCreateProgram();
	if (!program) {
		fprintf( stderr, "Error: failed to create program!\n");
		goto fail;
	}
	CNFGglAttachShader(program, vertex_shader_object);
	CNFGglAttachShader(program, fragment_shader_object);
	CNFGglBindAttribLocation(program, 0, "a0");
	CNFGglBindAttribLocation(program, 1, "a1");
	CNFGglLinkProgram(program);
	CNFGglGetProgramiv(program, GL_LINK_STATUS, &ret);
	if (!ret) {
		fprintf( stderr, "Error: program linking failed!\n");
		CNFGglGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			char *log = (char*)alloca(ret);
			CNFGglGetProgramInfoLog(program, ret, NULL, log);
			fprintf( stderr, "%s", log);
		}
		goto fail;
	}
	return program;
fail:
	if( !vertex_shader_object ) CNFGglDeleteShader( vertex_shader_object );
	if( !fragment_shader_object ) CNFGglDeleteShader( fragment_shader_object );
	if( !program ) CNFGglDeleteShader( program );
	return -1;
}
#ifdef CNFGEWGL
#define PRECISIONA "lowp"
#define PRECISIONB "mediump"
#else
#define PRECISIONA
#define PRECISIONB
#endif
void CNFGSetupBatchInternal()
{
	short w, h;
	CNFGLoadExtensionsInternal();
	CNFGGetDimensions( &w, &h );
	gRDShaderProg = CNFGGLInternalLoadShader(
		"uniform vec4 xfrm;"
		"attribute vec3 a0;"
		"attribute vec4 a1;"
		"varying " PRECISIONA " vec4 vc;"
		"void main() { gl_Position = vec4( a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5 ); vc = a1; }",
		"varying " PRECISIONA " vec4 vc;"
		"void main() { gl_FragColor = vec4(vc.abgr); }" 
	);
	CNFGglUseProgram( gRDShaderProg );
	gRDShaderProgUX = CNFGglGetUniformLocation ( gRDShaderProg , "xfrm" );
	gRDBlitProg = CNFGGLInternalLoadShader(
		"uniform vec4 xfrm;"
		"attribute vec3 a0;"
		"attribute vec4 a1;"
		"varying " PRECISIONB " vec2 tc;"
		"void main() { gl_Position = vec4( a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5 ); tc = a1.xy; }",
		
		"varying " PRECISIONB " vec2 tc;"
		"uniform sampler2D tex;"
		"void main() { gl_FragColor = texture2D(tex,tc)."
#if !defined( CNFGRASTERIZER )
"wzyx"
#else
"wxyz"
#endif
";}" 	);
	CNFGglUseProgram( gRDBlitProg );
	gRDBlitProgUX = CNFGglGetUniformLocation ( gRDBlitProg , "xfrm" );
	gRDBlitProgUT = CNFGglGetUniformLocation ( gRDBlitProg , "tex" );
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glGenTextures( 1, &gRDBlitProgTex );
#else
	glGenTextures( 1, &gRDBlitProgTex );
#endif
	CNFGglEnableVertexAttribArray(0);
	CNFGglEnableVertexAttribArray(1);
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glDisable(GL_DEPTH_TEST);
	LOADED_glDepthMask( GL_FALSE );
	LOADED_glEnable( GL_BLEND );
	LOADED_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
#else
	glDisable(GL_DEPTH_TEST);
	glDepthMask( GL_FALSE );
	glEnable( GL_BLEND );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
#endif
	CNFGVertPlace = 0;
}
#ifndef CNFGRASTERIZER
void CNFGInternalResize(short x, short y)
#else
void CNFGInternalResizeOGLBACKEND(short x, short y)
#endif
{
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glViewport( 0, 0, x, y );
#else
	glViewport( 0, 0, x, y );
#endif
	gRDLastResizeW = x;
	gRDLastResizeH = y;
	if (gRDShaderProg == 0xFFFFFFFF) { return; } // Prevent trying to set uniform if the shader isn't ready yet.
	CNFGglUseProgram( gRDShaderProg );
	CNFGglUniform4f( gRDShaderProgUX, 1.f/x, -1.f/y, -0.5f, 0.5f);
}
void	CNFGEmitBackendTriangles( const float * vertices, const uint32_t * colors, int num_vertices )
{
	CNFGglUseProgram( gRDShaderProg );
	CNFGglUniform4f( gRDShaderProgUX, 1.f/gRDLastResizeW, -1.f/gRDLastResizeH, -0.5f, 0.5f);
	CNFGglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	CNFGglVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colors);
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glDrawArrays( GL_TRIANGLES, 0, num_vertices);
#else
	glDrawArrays( GL_TRIANGLES, 0, num_vertices);
#endif
}
#ifdef CNFGRASTERIZER
void CNFGBlitImageInternal( uint32_t * data, int x, int y, int w, int h )
#else
void CNFGBlitImage( uint32_t * data, int x, int y, int w, int h )
#endif
{
	if( w <= 0 || h <= 0 ) return;
	CNFGFlushRender();
	CNFGglUseProgram( gRDBlitProg );
	CNFGglUniform4f( gRDBlitProgUX,
		1.f/gRDLastResizeW, -1.f/gRDLastResizeH,
		-0.5f+x/(float)gRDLastResizeW, 0.5f-y/(float)gRDLastResizeH );
	CNFGglUniform1i( gRDBlitProgUT, 0 );
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glEnable( GL_TEXTURE_2D );
#else
	glEnable( GL_TEXTURE_2D );
#endif
	CNFGglActiveTexture( 0 );
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glBindTexture( GL_TEXTURE_2D, gRDBlitProgTex );
	LOADED_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	LOADED_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	LOADED_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	LOADED_glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,  GL_RGBA,
#else
	glBindTexture( GL_TEXTURE_2D, gRDBlitProgTex );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,  GL_RGBA,
#endif
		GL_UNSIGNED_BYTE, data );
	const float verts[] = {
		0,0, (float)w,0, (float)w,(float)h,
		0,0, (float)w,(float)h, 0,(float)h, };
	static const uint8_t colors[] = {
		0,0,   255,0,  255,255,
		0,0,  255,255, 0,255 };
	CNFGglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
	CNFGglVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_TRUE, 0, colors);
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glDrawArrays( GL_TRIANGLES, 0, 6);
#else
	glDrawArrays( GL_TRIANGLES, 0, 6);
#endif
}
void CNFGUpdateScreenWithBitmap( uint32_t * data, int w, int h )
{
#ifdef CNFGRASTERIZER
	CNFGBlitImageInternal( data, 0, 0, w, h );
	void CNFGSwapBuffersInternal();
	CNFGSwapBuffersInternal();
#else
	CNFGBlitImage( data, 0, 0, w, h );
#endif
}
#ifndef CNFGRASTERIZER
void CNFGFlushRender()
{
	if( !CNFGVertPlace ) return;
	CNFGEmitBackendTriangles( CNFGVertDataV, CNFGVertDataC, CNFGVertPlace );
	CNFGVertPlace = 0;
}
void CNFGClearFrame()
{
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glClearColor( ((CNFGBGColor&0xff000000)>>24)/255.0, 
#else
	glClearColor( ((CNFGBGColor&0xff000000)>>24)/255.0, 
#endif
		((CNFGBGColor&0xff0000)>>16)/255.0,
		(CNFGBGColor&0xff00)/65280.0,
		(CNFGBGColor&0xff)/255.0);
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
	LOADED_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
#else
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
#endif
}
#endif
#endif //__wasm__
#else
void CNFGFlushRender() { }
#endif
#endif
#endif //_CNFG_C
#ifdef CNFG3D
//Copyright 2012-2017 <>< Charles Lohr
//You may license this file under the MIT/x11, NewBSD, or any GPL license.
//This is a series of tools useful for software rendering.
//Use of this file with OpenGL is untested.
#ifdef CNFG3D
#ifdef __wasm__
double sin( double v );
double cos( double v );
double tan( double v );
double sqrt( double v );
float sinf( float v );
float cosf( float v );
float tanf( float v );
float sqrtf( float v );
void tdMATCOPY( float * x, const float * y )
{
	int i;
	for( i = 0; i < 16; i++ ) x[i] = y[i];
}
#else
#include <string.h>
#include <stdio.h>
#endif
#ifdef CNFG3D_USE_OGL_MAJOR
#define m00 0
#define m10 1
#define m20 2
#define m30 3
#define m01 4
#define m11 5
#define m21 6
#define m31 7
#define m02 8
#define m12 9
#define m22 10
#define m32 11
#define m03 12
#define m13 13
#define m23 14
#define m33 15
#else
#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15
#endif
void tdIdentity( float * f )
{
	f[m00] = 1; f[m01] = 0; f[m02] = 0; f[m03] = 0;
	f[m10] = 0; f[m11] = 1; f[m12] = 0; f[m13] = 0;
	f[m20] = 0; f[m21] = 0; f[m22] = 1; f[m23] = 0;
	f[m30] = 0; f[m31] = 0; f[m32] = 0; f[m33] = 1;
}
void tdZero( float * f )
{
	f[m00] = 0; f[m01] = 0; f[m02] = 0; f[m03] = 0;
	f[m10] = 0; f[m11] = 0; f[m12] = 0; f[m13] = 0;
	f[m20] = 0; f[m21] = 0; f[m22] = 0; f[m23] = 0;
	f[m30] = 0; f[m31] = 0; f[m32] = 0; f[m33] = 0;
}
void tdTranslate( float * f, float x, float y, float z )
{
	float ftmp[16];
	tdIdentity(ftmp);
	ftmp[m03] += x;
	ftmp[m13] += y;
	ftmp[m23] += z;
	tdMultiply( f, ftmp, f );
}
void tdScale( float * f, float x, float y, float z )
{
#if 0
	f[m00] *= x;
	f[m01] *= x;
	f[m02] *= x;
	f[m03] *= x;
	f[m10] *= y;
	f[m11] *= y;
	f[m12] *= y;
	f[m13] *= y;
	f[m20] *= z;
	f[m21] *= z;
	f[m22] *= z;
	f[m23] *= z;
#endif
	float ftmp[16];
	tdIdentity(ftmp);
	ftmp[m00] *= x;
	ftmp[m11] *= y;
	ftmp[m22] *= z;
	tdMultiply( f, ftmp, f );
}
void tdRotateAA( float * f, float angle, float ix, float iy, float iz )
{
	float ftmp[16];
	float c = tdCOS( angle*tdDEGRAD );
	float s = tdSIN( angle*tdDEGRAD );
	float absin = tdSQRT( ix*ix + iy*iy + iz*iz );
	float x = ix/absin;
	float y = iy/absin;
	float z = iz/absin;
	ftmp[m00] = x*x*(1-c)+c;
	ftmp[m01] = x*y*(1-c)-z*s;
	ftmp[m02] = x*z*(1-c)+y*s;
	ftmp[m03] = 0;
	ftmp[m10] = y*x*(1-c)+z*s;
	ftmp[m11] = y*y*(1-c)+c;
	ftmp[m12] = y*z*(1-c)-x*s;
	ftmp[m13] = 0;
	ftmp[m20] = x*z*(1-c)-y*s;
	ftmp[m21] = y*z*(1-c)+x*s;
	ftmp[m22] = z*z*(1-c)+c;
	ftmp[m23] = 0;
	ftmp[m30] = 0;
	ftmp[m31] = 0;
	ftmp[m32] = 0;
	ftmp[m33] = 1;
	tdMultiply( f, ftmp, f );
}
void tdRotateQuat( float * f, float qw, float qx, float qy, float qz )
{
	float ftmp[16];
	//float qw2 = qw*qw;
	float qx2 = qx*qx;
	float qy2 = qy*qy;
	float qz2 = qz*qz;
	ftmp[m00] = 1 - 2*qy2 - 2*qz2;
	ftmp[m01] = 2*qx*qy - 2*qz*qw;
	ftmp[m02] = 2*qx*qz + 2*qy*qw;
	ftmp[m03] = 0;
	ftmp[m10] = 2*qx*qy + 2*qz*qw;
	ftmp[m11] = 1 - 2*qx2 - 2*qz2;
	ftmp[m12] = 2*qy*qz - 2*qx*qw;
	ftmp[m13] = 0;
	ftmp[m20] = 2*qx*qz - 2*qy*qw;
	ftmp[m21] = 2*qy*qz + 2*qx*qw;
	ftmp[m22] = 1 - 2*qx2 - 2*qy2;
	ftmp[m23] = 0;
	ftmp[m30] = 0;
	ftmp[m31] = 0;
	ftmp[m32] = 0;
	ftmp[m33] = 1;
	tdMultiply( f, ftmp, f );
}
void tdRotateEA( float * f, float x, float y, float z )
{
	float ftmp[16];
	//x,y,z must be negated for some reason
	float X = -x*2*tdQ_PI/360; //Reduced calulation for speed
	float Y = -y*2*tdQ_PI/360;
	float Z = -z*2*tdQ_PI/360;
	float cx = tdCOS(X);
	float sx = tdSIN(X);
	float cy = tdCOS(Y);
	float sy = tdSIN(Y);
	float cz = tdCOS(Z);
	float sz = tdSIN(Z);
	//Row major (unless CNFG3D_USE_OGL_MAJOR is selected)
	//manually transposed
	ftmp[m00] = cy*cz;
	ftmp[m10] = (sx*sy*cz)-(cx*sz);
	ftmp[m20] = (cx*sy*cz)+(sx*sz);
	ftmp[m30] = 0;
	ftmp[m01] = cy*sz;
	ftmp[m11] = (sx*sy*sz)+(cx*cz);
	ftmp[m21] = (cx*sy*sz)-(sx*cz);
	ftmp[m31] = 0;
	ftmp[m02] = -sy;
	ftmp[m12] = sx*cy;
	ftmp[m22] = cx*cy;
	ftmp[m32] = 0;
	ftmp[m03] = 0;
	ftmp[m13] = 0;
	ftmp[m23] = 0;
	ftmp[m33] = 1;
	tdMultiply( f, ftmp, f );
}
void tdMultiply( float * fin1, float * fin2, float * fout )
{
	float fotmp[16];
	int i, k;
#ifdef CNFG3D_USE_OGL_MAJOR
	fotmp[m00] = fin1[m00] * fin2[m00] + fin1[m01] * fin2[m10] + fin1[m02] * fin2[m20] + fin1[m03] * fin2[m30];
	fotmp[m01] = fin1[m00] * fin2[m01] + fin1[m01] * fin2[m11] + fin1[m02] * fin2[m21] + fin1[m03] * fin2[m31];
	fotmp[m02] = fin1[m00] * fin2[m02] + fin1[m01] * fin2[m12] + fin1[m02] * fin2[m22] + fin1[m03] * fin2[m32];
	fotmp[m03] = fin1[m00] * fin2[m03] + fin1[m01] * fin2[m13] + fin1[m02] * fin2[m23] + fin1[m03] * fin2[m33];
	fotmp[m10] = fin1[m10] * fin2[m00] + fin1[m11] * fin2[m10] + fin1[m12] * fin2[m20] + fin1[m13] * fin2[m30];
	fotmp[m11] = fin1[m10] * fin2[m01] + fin1[m11] * fin2[m11] + fin1[m12] * fin2[m21] + fin1[m13] * fin2[m31];
	fotmp[m12] = fin1[m10] * fin2[m02] + fin1[m11] * fin2[m12] + fin1[m12] * fin2[m22] + fin1[m13] * fin2[m32];
	fotmp[m13] = fin1[m10] * fin2[m03] + fin1[m11] * fin2[m13] + fin1[m12] * fin2[m23] + fin1[m13] * fin2[m33];
	fotmp[m20] = fin1[m20] * fin2[m00] + fin1[m21] * fin2[m10] + fin1[m22] * fin2[m20] + fin1[m23] * fin2[m30];
	fotmp[m21] = fin1[m20] * fin2[m01] + fin1[m21] * fin2[m11] + fin1[m22] * fin2[m21] + fin1[m23] * fin2[m31];
	fotmp[m22] = fin1[m20] * fin2[m02] + fin1[m21] * fin2[m12] + fin1[m22] * fin2[m22] + fin1[m23] * fin2[m32];
	fotmp[m23] = fin1[m20] * fin2[m03] + fin1[m21] * fin2[m13] + fin1[m22] * fin2[m23] + fin1[m23] * fin2[m33];
	fotmp[m30] = fin1[m30] * fin2[m00] + fin1[m31] * fin2[m10] + fin1[m32] * fin2[m20] + fin1[m33] * fin2[m30];
	fotmp[m31] = fin1[m30] * fin2[m01] + fin1[m31] * fin2[m11] + fin1[m32] * fin2[m21] + fin1[m33] * fin2[m31];
	fotmp[m32] = fin1[m30] * fin2[m02] + fin1[m31] * fin2[m12] + fin1[m32] * fin2[m22] + fin1[m33] * fin2[m32];
	fotmp[m33] = fin1[m30] * fin2[m03] + fin1[m31] * fin2[m13] + fin1[m32] * fin2[m23] + fin1[m33] * fin2[m33];
#else
	for( i = 0; i < 16; i++ )
	{
		int xp = i & 0x03;
		int yp = i & 0x0c;
		fotmp[i] = 0;
		for( k = 0; k < 4; k++ )
		{
			fotmp[i] += fin1[yp+k] * fin2[(k<<2)|xp];
		}
	}
#endif
	tdMATCOPY( fout, fotmp );
}
#ifndef __wasm__
void tdPrint( const float * f )
{
	int i;
	printf( "{\n" );
#ifdef CNFG3D_USE_OGL_MAJOR
	for( i = 0; i < 4; i++ )
	{
		printf( "  %f, %f, %f, %f\n", f[0+i], f[4+i], f[8+i], f[12+i] );
	}
#else
	for( i = 0; i < 16; i+=4 )
	{
		printf( "  %f, %f, %f, %f\n", f[0+i], f[1+i], f[2+i], f[3+i] );
	}
#endif
	printf( "}\n" );
}
#endif
void tdTransposeSelf( float * f )
{
	float fout[16];
	fout[m00] = f[m00]; fout[m01] = f[m10]; fout[m02] = f[m20]; fout[m03] = f[m30];
	fout[m10] = f[m01]; fout[m11] = f[m11]; fout[m12] = f[m21]; fout[m13] = f[m31];
	fout[m20] = f[m02]; fout[m21] = f[m12]; fout[m22] = f[m22]; fout[m23] = f[m32];
	fout[m30] = f[m03]; fout[m31] = f[m13]; fout[m32] = f[m23]; fout[m33] = f[m33];
	tdMATCOPY( f, fout );
}
void tdPerspective( float fovy, float aspect, float zNear, float zFar, float * out )
{
	float f = 1./tdTAN(fovy * tdQ_PI / 360.0);
	out[m00] = f/aspect; out[m01] = 0; out[m02] = 0; out[m03] = 0;
	out[m10] = 0; out[m11] = f; out[m12] = 0; out[m13] = 0;
	out[m20] = 0; out[m21] = 0;
	out[m22] = (zFar + zNear)/(zNear - zFar);
	out[m23] = 2*zFar*zNear  /(zNear - zFar);
	out[m30] = 0; out[m31] = 0; out[m32] = -1; out[m33] = 0;
}
void tdLookAt( float * m, float * eye, float * at, float * up )
{
	float out[16];
	float F[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
	float fdiv = 1./tdSQRT( F[0]*F[0] + F[1]*F[1] + F[2]*F[2] );
	float f[3] = { F[0]*fdiv, F[1]*fdiv, F[2]*fdiv };
	float udiv = 1./tdSQRT( up[0]*up[0] + up[1]*up[1] + up[2]*up[2] );
	float UP[3] = { up[0]*udiv, up[1]*udiv, up[2]*udiv };
	float s[3];
	float u[3];
	tdCross( f, UP, s );
	tdCross( s, f, u );
	out[m00] = s[0]; out[m01] = s[1]; out[m02] = s[2]; out[m03] = 0;
	out[m10] = u[0]; out[m11] = u[1]; out[m12] = u[2]; out[m13] = 0;
	out[m20] = -f[0];out[m21] =-f[1]; out[m22] =-f[2]; out[m23] = 0;
	out[m30] = 0;    out[m31] = 0;    out[m32] = 0;    out[m33] = 1;
	tdMultiply( m, out, m );
	tdTranslate( m, -eye[0], -eye[1], -eye[2] );
}
void tdPTransform( const float * pin, float * f, float * pout )
{
	float ptmp[2];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + f[m03];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + f[m13];
	pout[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + f[m23];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
}
void tdVTransform( const float * pin, float * f, float * pout )
{
	float ptmp[2];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12];
	pout[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
}
void td4Transform( float * pin, float * f, float * pout )
{
	float ptmp[3];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13];
	ptmp[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23];
	pout[3] = pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
	pout[2] = ptmp[2];
}
void td4RTransform( float * pin, float * f, float * pout )
{
	float ptmp[3];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m10] + pin[2] * f[m20] + pin[3] * f[m30];
	ptmp[1] = pin[0] * f[m01] + pin[1] * f[m11] + pin[2] * f[m21] + pin[3] * f[m31];
	ptmp[2] = pin[0] * f[m02] + pin[1] * f[m12] + pin[2] * f[m22] + pin[3] * f[m32];
	pout[3] = pin[0] * f[m03] + pin[1] * f[m13] + pin[2] * f[m23] + pin[3] * f[m33];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
	pout[2] = ptmp[2];
}
void tdNormalizeSelf( float * vin )
{
	float vsq = 1./tdSQRT(vin[0]*vin[0] + vin[1]*vin[1] + vin[2]*vin[2]);
	vin[0] *= vsq;
	vin[1] *= vsq;
	vin[2] *= vsq;
}
void tdCross( float * va, float * vb, float * vout )
{
	float vtmp[2];
	vtmp[0] = va[1] * vb[2] - va[2] * vb[1];
	vtmp[1] = va[2] * vb[0] - va[0] * vb[2];
	vout[2] = va[0] * vb[1] - va[1] * vb[0];
	vout[0] = vtmp[0];
	vout[1] = vtmp[1];
}
float tdDistance( float * va, float * vb )
{
	float dx = va[0]-vb[0];
	float dy = va[1]-vb[1];
	float dz = va[2]-vb[2];
	return tdSQRT(dx*dx + dy*dy + dz*dz);
}
float tdDot( float * va, float * vb )
{
	return va[0]*vb[0] + va[1]*vb[1] + va[2]*vb[2];
}
//Stack functionality.
static float gsMatricies[2][tdMATRIXMAXDEPTH][16];
float * gSMatrix = gsMatricies[0][0];
static int gsMMode;
static int gsMPlace[2];
void tdPush()
{
	if( gsMPlace[gsMMode] > tdMATRIXMAXDEPTH - 2 )
		return;
	tdMATCOPY( gsMatricies[gsMMode][gsMPlace[gsMMode] + 1], gsMatricies[gsMMode][gsMPlace[gsMMode]] );
	gsMPlace[gsMMode]++;
	gSMatrix = gsMatricies[gsMMode][gsMPlace[gsMMode]];
}
void tdPop()
{
	if( gsMPlace[gsMMode] < 1 )
		return;
	gsMPlace[gsMMode]--;
	gSMatrix = gsMatricies[gsMMode][gsMPlace[gsMMode]];
}
void tdMode( int mode )
{
	if( mode < 0 || mode > 1 )
		return;
	
	gsMMode = mode;
	gSMatrix = gsMatricies[gsMMode][gsMPlace[gsMMode]];
}
static float translateX;
static float translateY;
static float scaleX;
static float scaleY;
void tdSetViewport( float leftx, float topy, float rightx, float bottomy, float pixx, float pixy )
{
	translateX = leftx;
	translateY = bottomy;
	scaleX = pixx/(rightx-leftx);
	scaleY = pixy/(topy-bottomy);
}
void tdFinalPoint( float * pin, float * pout )
{
	float tdin[4] = { pin[0], pin[1], pin[2], 1. };
	float tmp[4];
	td4Transform( tdin, gsMatricies[0][gsMPlace[0]], tmp );
//	printf( "XFORM1Out: %f %f %f %f\n", tmp[0], tmp[1], tmp[2], tmp[3] );
	td4Transform(  tmp, gsMatricies[1][gsMPlace[1]], tmp );
//	printf( "XFORM2Out: %f %f %f %f\n", tmp[0], tmp[1], tmp[2], tmp[3] );
	pout[0] = (tmp[0]/tmp[3] - translateX) * scaleX;
	pout[1] = (tmp[1]/tmp[3] - translateY) * scaleY;
	pout[2] = tmp[2]/tmp[3];
//	printf( "XFORMFOut: %f %f %f\n", pout[0], pout[1], pout[2] );
}
float tdNoiseAt( int x, int y )
{
	return ((x*13241*y + y * 33455927)%9293) / 4646. - 1.0;
}
static inline float tdFade( float f )
{
	float ft3 = f*f*f;
	return ft3 * 10 - ft3 * f * 15 + 6 * ft3 * f * f;
}
float tdFLerp( float a, float b, float t )
{
	float fr = tdFade( t );
	return a * (1.-fr) + b * fr;
}
static inline float tdFNoiseAt( float x, float y )
{
	int ix = x;
	int iy = y;
	float fx = x - ix;
	float fy = y - iy;
	float a = tdNoiseAt( ix, iy );
	float b = tdNoiseAt( ix+1, iy );
	float c = tdNoiseAt( ix, iy+1 );
	float d = tdNoiseAt( ix+1, iy+1 );
	float top = tdFLerp( a, b, fx );
	float bottom = tdFLerp( c, d, fx );
	return tdFLerp( top, bottom, fy );
}
float tdPerlin2D( float x, float y )
{
	int ndepth = 5;
	int depth;
	float ret = 0;
	for( depth = 0; depth < ndepth; depth++ )
	{
		float nx = x / (1<<(ndepth-depth-1));
		float ny = y / (1<<(ndepth-depth-1));
		ret += tdFNoiseAt( nx, ny ) / (1<<(depth+1));
	}
	return ret;
}
#endif
#endif
#endif
#endif
#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifndef _OS_GENERIC_H
#define _OS_GENERIC_H
/*
	"osgeneric" Generic, platform independent tool for threads and time.
		Geared around Windows and Linux. Designed for operation on MSVC,
		TCC, GCC and clang.  Others may work.
    It offers the following operations:
	Delay functions:
		void OGSleep( int is );
		void OGUSleep( int ius );
	Getting current time (may be time from program start, boot, or epoc)
		double OGGetAbsoluteTime();
		double OGGetFileTime( const char * file );
	Thread functions
		og_thread_t OGCreateThread( void * (routine)( void * ), void * parameter );
		void * OGJoinThread( og_thread_t ot );
		void OGCancelThread( og_thread_t ot );
	Mutex functions, used for protecting data structures.
		 (recursive on platforms where available.)
		og_mutex_t OGCreateMutex();
		void OGLockMutex( og_mutex_t om );
		void OGUnlockMutex( og_mutex_t om );
		void OGDeleteMutex( og_mutex_t om );
	Always a semaphore (not recursive)
		og_sema_t OGCreateSema(); //Create a semaphore, comes locked initially.
          NOTE: For platform compatibility, max count is 32767
		void OGLockSema( og_sema_t os );
		int OGGetSema( og_sema_t os );  //if <0 there was a failure.
		void OGUnlockSema( og_sema_t os );
		void OGDeleteSema( og_sema_t os );
	TLS (Thread-Local Storage)
		og_tls_t OGCreateTLS();
		void OGDeleteTLS( og_tls_t tls );
		void OGSetTLS( og_tls_t tls, void * data );
		void * OGGetTLS( og_tls_t tls );
   You can permute the operations of this file by the following means:
    OSG_NO_IMPLEMENTATION
	OSG_PREFIX
	OSG_NOSTATIC
   The default behavior is to do static inline.
   Copyright (c) 2011-2012,2013,2016,2018,2019,2020 <>< Charles Lohr
   This file may be licensed under the MIT/x11 license, NewBSD or CC0 licenses
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of this file.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.
	Date Stamp: 2019-09-05 CNL: Allow for noninstantiation and added TLS.
	Date Stamp: 2018-03-25 CNL: Switched to header-only format.
*/
#if defined( OSG_NOSTATIC ) && OSG_NOSTATIC != 0
#ifndef OSG_PREFIX
#define OSG_PREFIX
#endif
#ifndef OSG_NO_IMPLEMENTATION
#define OSG_NO_IMPLEMENTATION
#endif
#endif
#ifndef OSG_PREFIX
#ifdef __wasm__
#define OSG_PREFIX
#else
#define OSG_PREFIX static inline
#endif
#endif
//In case you want to hook the closure of a thread, i.e. if your system has thread-local storage.
#ifndef OSG_TERM_THREAD_CODE
#define OSG_TERM_THREAD_CODE
#endif
typedef void* og_thread_t;
typedef void* og_mutex_t;
typedef void* og_sema_t;
typedef void* og_tls_t;
#ifdef __cplusplus
extern "C" {
#endif
OSG_PREFIX void OGSleep( int is );
OSG_PREFIX void OGUSleep( int ius );
OSG_PREFIX double OGGetAbsoluteTime();
OSG_PREFIX double OGGetFileTime( const char * file );
OSG_PREFIX og_thread_t OGCreateThread( void * (routine)( void * ), void * parameter );
OSG_PREFIX void * OGJoinThread( og_thread_t ot );
OSG_PREFIX void OGCancelThread( og_thread_t ot );
OSG_PREFIX og_mutex_t OGCreateMutex();
OSG_PREFIX void OGLockMutex( og_mutex_t om );
OSG_PREFIX void OGUnlockMutex( og_mutex_t om );
OSG_PREFIX void OGDeleteMutex( og_mutex_t om );
OSG_PREFIX og_sema_t OGCreateSema();
OSG_PREFIX int OGGetSema( og_sema_t os );
OSG_PREFIX void OGLockSema( og_sema_t os );
OSG_PREFIX void OGUnlockSema( og_sema_t os );
OSG_PREFIX void OGDeleteSema( og_sema_t os );
OSG_PREFIX og_tls_t OGCreateTLS();
OSG_PREFIX void OGDeleteTLS( og_tls_t key );
OSG_PREFIX void * OGGetTLS( og_tls_t key );
OSG_PREFIX void OGSetTLS( og_tls_t key, void * data );
#ifdef __cplusplus
};
#endif
#ifndef OSG_NO_IMPLEMENTATION
#if defined( WIN32 ) || defined (WINDOWS) || defined( _WIN32)
#define USE_WINDOWS
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifdef USE_WINDOWS
#include <windows.h>
#include <stdint.h>
OSG_PREFIX void OGSleep( int is )
{
	Sleep( is*1000 );
}
OSG_PREFIX void OGUSleep( int ius )
{
	Sleep( ius/1000 );
}
OSG_PREFIX double OGGetAbsoluteTime()
{
	static LARGE_INTEGER lpf;
	LARGE_INTEGER li;
	if( !lpf.QuadPart )
	{
		QueryPerformanceFrequency( &lpf );
	}
	QueryPerformanceCounter( &li );
	return (double)li.QuadPart / (double)lpf.QuadPart;
}
OSG_PREFIX double OGGetFileTime( const char * file )
{
	FILETIME ft;
	HANDLE h = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if( h==INVALID_HANDLE_VALUE )
		return -1;
	GetFileTime( h, 0, 0, &ft );
	CloseHandle( h );
	return ft.dwHighDateTime + ft.dwLowDateTime;
}
OSG_PREFIX og_thread_t OGCreateThread( void * (routine)( void * ), void * parameter )
{
	return (og_thread_t)CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)routine, parameter, 0, 0 );
}
OSG_PREFIX void * OGJoinThread( og_thread_t ot )
{
	WaitForSingleObject( ot, INFINITE );
	OSG_TERM_THREAD_CODE
	CloseHandle( ot );
	return 0;
}
OSG_PREFIX void OGCancelThread( og_thread_t ot )
{
	OSG_TERM_THREAD_CODE
	TerminateThread( ot, 0);
	CloseHandle( ot );	
}
OSG_PREFIX og_mutex_t OGCreateMutex()
{
	return CreateMutex( 0, 0, 0 );
}
OSG_PREFIX void OGLockMutex( og_mutex_t om )
{
	WaitForSingleObject(om, INFINITE);
}
OSG_PREFIX void OGUnlockMutex( og_mutex_t om )
{
	ReleaseMutex(om);
}
OSG_PREFIX void OGDeleteMutex( og_mutex_t om )
{
	CloseHandle( om );
}
OSG_PREFIX og_sema_t OGCreateSema()
{
	HANDLE sem = CreateSemaphore( 0, 0, 32767, 0 );
	return (og_sema_t)sem;
}
OSG_PREFIX int OGGetSema( og_sema_t os )
{
	typedef LONG NTSTATUS;
	HANDLE sem = (HANDLE)os;
	typedef NTSTATUS (NTAPI *_NtQuerySemaphore)(
		HANDLE SemaphoreHandle, 
		DWORD SemaphoreInformationClass, /* Would be SEMAPHORE_INFORMATION_CLASS */
		PVOID SemaphoreInformation,      /* but this is to much to dump here     */
		ULONG SemaphoreInformationLength, 
		PULONG ReturnLength OPTIONAL
	);
	typedef struct _SEMAPHORE_BASIC_INFORMATION {   
		ULONG CurrentCount; 
		ULONG MaximumCount;
	} SEMAPHORE_BASIC_INFORMATION;
	static _NtQuerySemaphore NtQuerySemaphore;
	SEMAPHORE_BASIC_INFORMATION BasicInfo;
	NTSTATUS Status;
	if( !NtQuerySemaphore )
	{	
	    NtQuerySemaphore = (_NtQuerySemaphore)GetProcAddress (GetModuleHandle ("ntdll.dll"), "NtQuerySemaphore");
		if( !NtQuerySemaphore )
		{
			return -1;
		}
	}
	
    Status = NtQuerySemaphore (sem, 0 /*SemaphoreBasicInformation*/, 
        &BasicInfo, sizeof (SEMAPHORE_BASIC_INFORMATION), NULL);
    if (Status == ERROR_SUCCESS)
    {       
        return BasicInfo.CurrentCount;
    }
	return -2;
}
OSG_PREFIX void OGLockSema( og_sema_t os )
{
	WaitForSingleObject( (HANDLE)os, INFINITE );
}
OSG_PREFIX void OGUnlockSema( og_sema_t os )
{
	ReleaseSemaphore( (HANDLE)os, 1, 0 );
}
OSG_PREFIX void OGDeleteSema( og_sema_t os )
{
	CloseHandle( os );
}
OSG_PREFIX og_tls_t OGCreateTLS()
{
	return (og_tls_t)(intptr_t)TlsAlloc();
}
OSG_PREFIX void OGDeleteTLS( og_tls_t key )
{
	TlsFree( (DWORD)(intptr_t)key );
}
OSG_PREFIX void * OGGetTLS( og_tls_t key )
{
	return TlsGetValue( (DWORD)(intptr_t)key );
}
OSG_PREFIX void OGSetTLS( og_tls_t key, void * data )
{
	TlsSetValue( (DWORD)(intptr_t)key, data );
}
#elif defined( __wasm__ )
//We don't actually have any function defintions here.
//The outside system will handle it.
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>
#include <unistd.h>
OSG_PREFIX void OGSleep( int is )
{
	sleep( is );
}
OSG_PREFIX void OGUSleep( int ius )
{
	usleep( ius );
}
OSG_PREFIX double OGGetAbsoluteTime()
{
	struct timeval tv;
	gettimeofday( &tv, 0 );
	return ((double)tv.tv_usec)/1000000. + (tv.tv_sec);
}
OSG_PREFIX double OGGetFileTime( const char * file )
{
	struct stat buff; 
	int r = stat( file, &buff );
	if( r < 0 )
	{
		return -1;
	}
	return buff.st_mtime;
}
OSG_PREFIX og_thread_t OGCreateThread( void * (routine)( void * ), void * parameter )
{
	pthread_t * ret = (pthread_t *)malloc( sizeof( pthread_t ) );
	if( !ret ) return 0;
	int r = pthread_create( ret, 0, routine, parameter );
	if( r )
	{
		free( ret );
		return 0;
	}
	return (og_thread_t)ret;
}
OSG_PREFIX void * OGJoinThread( og_thread_t ot )
{
	void * retval;
	if( !ot )
	{
		return 0;
	}
	pthread_join( *(pthread_t*)ot, &retval );
	OSG_TERM_THREAD_CODE
	free( ot );
	return retval;
}
OSG_PREFIX void OGCancelThread( og_thread_t ot )
{
	if( !ot )
	{
		return;
	}
#ifdef ANDROID
	pthread_kill( *(pthread_t*)ot, SIGTERM );
#else
	pthread_cancel( *(pthread_t*)ot );
#endif
	OSG_TERM_THREAD_CODE
	free( ot );
}
OSG_PREFIX og_mutex_t OGCreateMutex()
{
	pthread_mutexattr_t   mta;
	og_mutex_t r = malloc( sizeof( pthread_mutex_t ) );
	if( !r ) return 0;
	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init( (pthread_mutex_t *)r, &mta );
	return r;
}
OSG_PREFIX void OGLockMutex( og_mutex_t om )
{
	if( !om )
	{
		return;
	}
	pthread_mutex_lock( (pthread_mutex_t*)om );
}
OSG_PREFIX void OGUnlockMutex( og_mutex_t om )
{
	if( !om )
	{
		return;
	}
	pthread_mutex_unlock( (pthread_mutex_t*)om );
}
OSG_PREFIX void OGDeleteMutex( og_mutex_t om )
{
	if( !om )
	{
		return;
	}
	pthread_mutex_destroy( (pthread_mutex_t*)om );
	free( om );
}
OSG_PREFIX og_sema_t OGCreateSema()
{
	sem_t * sem = (sem_t *)malloc( sizeof( sem_t ) );
	if( !sem ) return 0;
	sem_init( sem, 0, 0 );
	return (og_sema_t)sem;
}
OSG_PREFIX int OGGetSema( og_sema_t os )
{
	int valp;
	sem_getvalue( (sem_t*)os, &valp );
	return valp;
}
OSG_PREFIX void OGLockSema( og_sema_t os )
{
	sem_wait( (sem_t*)os );
}
OSG_PREFIX void OGUnlockSema( og_sema_t os )
{
	sem_post( (sem_t*)os );
}
OSG_PREFIX void OGDeleteSema( og_sema_t os )
{
	sem_destroy( (sem_t*)os );
	free(os);
}
OSG_PREFIX og_tls_t OGCreateTLS()
{
	pthread_key_t ret = 0;
	pthread_key_create(&ret, 0);
	return (og_tls_t)(intptr_t)ret;
}
OSG_PREFIX void OGDeleteTLS( og_tls_t key )
{
	pthread_key_delete( (pthread_key_t)(intptr_t)key );
}
OSG_PREFIX void * OGGetTLS( og_tls_t key )
{
	return pthread_getspecific( (pthread_key_t)(intptr_t)key );
}
OSG_PREFIX void OGSetTLS( og_tls_t key, void * data )
{
	pthread_setspecific( (pthread_key_t)(intptr_t)key, data );
}
#endif
#ifdef __cplusplus
};
#endif
#endif //OSG_NO_IMPLEMENTATION
#endif //_OS_GENERIC_H
#ifdef __cplusplus
};
#endif

// PISHTOV_START
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

struct Image {
    long long width;
    long long height;
    std::vector<uint32_t> data;
};

#define Color uint32_t

int windowX = 800, windowY = 600;
bool isKeyPressed[256];
bool isMousePressed[16];
int mouseX, mouseY;

void setup();
void update();
void draw();
void keydown(int);
void keyup(int);
void mousedown(int);
void mouseup(int);

const Color colorBlack  = 0x000000ff;
const Color colorBlue   = 0x0000ffff;
const Color colorGreen  = 0x00ff00ff;
const Color colorCyan   = 0x00ffffff;
const Color colorRed    = 0xff0000ff;
const Color colorPurple = 0xff00ffff;
const Color colorYellow = 0xffff00ff;
const Color colorWhite  = 0xffffffff;

const int ms_per_frame = 10;

// Translate a keycode to JS keycode.
int translate_keycode(int key) {
#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
    switch (key) {
    case 0x08: return   8; // Backspace
    case 0x09: return   9; // Tab
    case 0x0d: return  13; // Enter
    case 0x10: return  16; // Shift
    case 0xa2: return  17; // Left Control
    case 0xa3: return  17; // Right Control
    case 0x12: return  18; // Alt
    case 0x13: return  19; // Pause/Break
    case 0x14: return  20; // Caps Lock
    case 0x1b: return  27; // Escape
    case 0x20: return  32; // Spacebar
    case 0x21: return  33; // Page Up
    case 0x22: return  34; // Page Down
    case 0x24: return  35; // Home
    case 0x23: return  36; // End
    case 0x25: return  37; // Left Arrow
    case 0x26: return  38; // Up Arrow
    case 0x27: return  39; // Right Arrow
    case 0x28: return  40; // Down Arrow
    case 0x2d: return  45; // Insert
    case 0x2e: return  46; // Delete
    case 0x30: return  48; // 0
    case 0x31: return  49; // 1
    case 0x32: return  50; // 2
    case 0x33: return  51; // 3
    case 0x34: return  52; // 4
    case 0x35: return  53; // 5
    case 0x36: return  54; // 6
    case 0x37: return  55; // 7
    case 0x38: return  56; // 8
    case 0x39: return  57; // 9
    case 0x41: return  65; // A
    case 0x42: return  66; // B
    case 0x43: return  67; // C
    case 0x44: return  68; // D
    case 0x45: return  69; // E
    case 0x46: return  70; // F
    case 0x47: return  71; // G
    case 0x48: return  72; // H
    case 0x49: return  73; // I
    case 0x4a: return  74; // J
    case 0x4b: return  75; // K
    case 0x4c: return  76; // L
    case 0x4d: return  77; // M
    case 0x4e: return  78; // N
    case 0x4f: return  79; // O
    case 0x50: return  80; // P
    case 0x51: return  81; // Q
    case 0x52: return  82; // R
    case 0x53: return  83; // S
    case 0x54: return  84; // T
    case 0x55: return  85; // U
    case 0x56: return  86; // V
    case 0x57: return  87; // W
    case 0x58: return  88; // X
    case 0x59: return  89; // Y
    case 0x5a: return  90; // Z
    case 0x5b: return  91; // Left Windows Key
    case 0x5c: return  92; // Right Windows Key
    case 0x29: return  93; // Select key
    case 0x60: return  96; // Keypad 0
    case 0x61: return  97; // Keypad 1
    case 0x62: return  98; // Keypad 2
    case 0x63: return  99; // Keypad 3
    case 0x64: return 100; // Keypad 4
    case 0x65: return 101; // Keypad 5
    case 0x66: return 102; // Keypad 6
    case 0x67: return 103; // Keypad 7
    case 0x68: return 104; // Keypad 8
    case 0x69: return 105; // Keypad 9
    case 0x6a: return 106; // Keypad *
    case 0x6b: return 107; // Keypad +
    case 0x6d: return 109; // Keypad -
    case 0x6e: return 110; // Keypad .
    case 0x6f: return 111; // Keypad /
    case 0x70: return 112; // F1
    case 0x71: return 113; // F2
    case 0x72: return 114; // F3
    case 0x73: return 115; // F4
    case 0x74: return 116; // F5
    case 0x75: return 117; // F6
    case 0x76: return 118; // F7
    case 0x77: return 119; // F8
    case 0x78: return 120; // F9
    case 0x79: return 121; // F10
    case 0x7a: return 122; // F11
    case 0x7b: return 123; // F12
    case 0x90: return 144; // Num Lock
    case 0x91: return 145; // Scroll Lock
    case 0xba: return 186; // ; :
    case 0xbb: return 187; // = +
    case 0xbc: return 188; // , <
    case 0xbd: return 189; // - _
    case 0xbe: return 190; // . >
    case 0xbf: return 191; // / ?
    case 0xc0: return 192; // ` ~
    case 0xdb: return 219; // [ {
    case 0xdc: return 220; // \ |
    case 0xdd: return 221; // ] }
    case 0xde: return 222; // ' "
    default: return 0;
    }
    return key;
#elif defined( EGL_LEAN_AND_MEAN ) // doesn't have any keys
#elif defined( __android__ ) || defined( ANDROID ) // ^
#elif defined( __wasm__ ) // As far as I understand wasm and js keycodes are the same.
    return key;
#else // most likely x11
    switch (key) {
    case 65288: return   8; // Backspace
    case 65289: return   9; // Tab
    case 65293: return  13; // Enter
    case 65505: return  16; // Shift
    case 65507: return  17; // Left Control
    case 65508: return  17; // Right Control
    case 65513: return  18; // Alt
    case 65299: return  19; // Pause/Break
    case 65509: return  20; // Caps Lock
    case 65307: return  27; // Escape
    case    32: return  32; // Spacebar
    case 65365: return  33; // Page Up
    case 65366: return  34; // Page Down
    case 65360: return  35; // Home
    case 65367: return  36; // End
    case 65361: return  37; // Left Arrow
    case 65362: return  38; // Up Arrow
    case 65363: return  39; // Right Arrow
    case 65364: return  40; // Down Arrow
    case 65379: return  45; // Insert
    case 65535: return  46; // Delete
    case    48: return  48; // 0
    case    49: return  49; // 1
    case    50: return  50; // 2
    case    51: return  51; // 3
    case    52: return  52; // 4
    case    53: return  53; // 5
    case    54: return  54; // 6
    case    55: return  55; // 7
    case    56: return  56; // 8
    case    57: return  57; // 9
    case    97: return  65; // A
    case    98: return  66; // B
    case    99: return  67; // C
    case   100: return  68; // D
    case   101: return  69; // E
    case   102: return  70; // F
    case   103: return  71; // G
    case   104: return  72; // H
    case   105: return  73; // I
    case   106: return  74; // J
    case   107: return  75; // K
    case   108: return  76; // L
    case   109: return  77; // M
    case   110: return  78; // N
    case   111: return  79; // O
    case   112: return  80; // P
    case   113: return  81; // Q
    case   114: return  82; // R
    case   115: return  83; // S
    case   116: return  84; // T
    case   117: return  85; // U
    case   118: return  86; // V
    case   119: return  87; // W
    case   120: return  88; // X
    case   121: return  89; // Y
    case   122: return  90; // Z
    case 65515: return  91; // Left Windows Key
    // TODO Right Windows Key
    // TODO Select
    case 65421: return  13; // Keypad Enter
    case 65438: return  96; // Keypad 0
    case 65436: return  97; // Keypad 1
    case 65433: return  98; // Keypad 2
    case 65435: return  99; // Keypad 3
    case 65430: return 100; // Keypad 4
    case 65437: return 101; // Keypad 5
    case 65432: return 102; // Keypad 6
    case 65429: return 103; // Keypad 7
    case 65431: return 104; // Keypad 8
    case 65434: return 105; // Keypad 9
    case 65450: return 106; // Keypad *
    case 65451: return 107; // Keypad +
    case 65453: return 109; // Keypad -
    case 65439: return 110; // Keypad .
    case 65455: return 111; // Keypad /
    case 65470: return 112; // F1
    case 65471: return 113; // F2
    case 65472: return 114; // F3
    case 65473: return 115; // F4
    case 65474: return 116; // F5
    case 65475: return 117; // F6
    case 65476: return 118; // F7
    case 65477: return 119; // F8
    case 65478: return 120; // F9
    case 65479: return 121; // F10
    case 65480: return 122; // F11
    case 65481: return 123; // F12
    case 65407: return 144; // Num Lock
    case 65300: return 145; // Scroll Lock
    case    59: return 186; // ; :
    case    61: return 187; // = +
    case    44: return 188; // , <
    case    45: return 189; // - _
    case    46: return 190; // . >
    case    47: return 191; // / ?
    case    96: return 192; // ` ~
    case    91: return 219; // [ {
    case    92: return 220; // \ |
    case    93: return 221; // ] }
    case    39: return 222; // ' "
    default: return 0;
    }
#endif
}

void HandleKey( int keycode, int bDown ) {
    keycode = translate_keycode(keycode);
    if (!keycode) return;
    if (bDown) {
        isKeyPressed[keycode] = true;
        keydown(keycode);
    }
    else {
        isKeyPressed[keycode] = false;
        keyup(keycode);
    }
}
void HandleButton( int x, int y, int button, int bDown ) {
    if (bDown) {
        isMousePressed[button] = true;
        mousedown(button);
    }
    else {
        isMousePressed[button] = false;
        mouseup(button);
    }
}
void HandleMotion( int x, int y, int mask ) {
    mouseX = x;
    mouseY = y;
}
void HandleDestroy() {}

void fillRect(int x, int y, int w, int h) {
    CNFGTackRectangle(x, y, x + w, y + h);
}

Image loadPng(const char *filename) {
    Image img;
    unsigned long w, h;
    std::vector<unsigned char> file_data;
    std::vector<unsigned char> raw_img;
    FILE *fp = fopen(filename, "rb");
	setvbuf(stdin, NULL, _IOFBF, 16384);
	while (true) {
        unsigned char c = getc(fp);
        if (feof(fp)) break;
        file_data.push_back(c);
	}
    fclose(fp);
    decodePNG(raw_img, w, h, &file_data[0], file_data.size());
    img.width = w;
    img.height = h;
    img.data.reserve(img.height * img.width);
    for (int i = 0; i < img.height; ++i) for (int j = 0; j < img.width * 4; j += 4) {
        img.data.push_back((raw_img[i * img.width * 4 + j] << 24) |
                           (raw_img[i * img.width * 4 + j + 1] << 16) |
                           (raw_img[i * img.width * 4 + j + 2] << 8) |
                           raw_img[i * img.width * 4 + j + 3]);
    }
    return img;
}

Image scaleImage(Image in, int w, int h) {
    Image out;
    out.width = w;
    out.height = h;
    out.data.resize(w * h);
    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++)
        out.data[i * w + j] = in.data[i * in.height / h * in.width + j * in.width / w];
    return out;
}

void drawImage(Image img, int x, int y, int w, int h) {
    if (w < 0) {
        w = -w;
        x -= w;
    }
    if (h < 0) {
        h = -h;
        y -= h;
    }
    if (w != img.width || h != img.height)
        drawImage(scaleImage(img, w, h), x, y, w, h);
    else
        CNFGBlitImage(&img.data[0], x, y, img.width, img.height);
}

void drawPixel(int x, int y) {
    CNFGTackPixel(x, y);
}

void fillColor(Color color) {
    CNFGColor(color);
}

Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return r << 24 | g << 16 | b << 8 | a;
}

Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return r << 24 | g << 16 | b << 8 | 0xff;
}

Color grayscale(uint8_t c) {
    return c << 24 | c << 16 | c << 8 | 0xff;
}

bool areColliding(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    if (w1 < 0) {
        w1 = -w1;
        x1 -= w1;
    }
    if (h1 < 0) {
        h1 = -h1;
        y1 -= h1;
    }
    if (w2 < 0) {
        w2 = -w2;
        x2 -= w2;
    }
    if (h2 < 0) {
        h2 = -h2;
        y2 -= h2;
    }
    return x2 <= x1 + w1 && x1 <= x2 + w2 && y2 <= y1 + h1 && y1 <= y2 + h2;
}

void fillText(char *text, int x, int y);

struct Vec2 {
    float x;
    float y;
};

Vec2 operator+ (Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

int main() {
    srand(time(0));
	CNFGSetup("Igra", windowX, windowY);
    setup();
	while(1) {
		CNFGHandleInput();
		CNFGGetDimensions((short*)&windowX, (short*)&windowY );
        update();
		CNFGBGColor = 0xffffffff;
		CNFGClearFrame();
        CNFGColor(colorBlue);
        draw();
        CNFGSwapBuffers();
        // We assume the frame is infinitely fast
        //OGUSleep(ms_per_frame * 1000);
        fillColor(colorBlack);
    }
}
