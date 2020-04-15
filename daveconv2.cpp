
// daveconv2: converts raw DAVE register data to simple compressed format
// Copyright (C) 2016-2020 Istvan Varga <istvanv@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <map>
#include <algorithm>
#include <exception>
#include <stdexcept>

static const size_t lengthMaxValue = 64;
static const size_t rleOffsetFlag = 0x00400000;

static void errorMessage(const char *fmt, ...)
{
  char    buf[1024];
  std::va_list  ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';
  throw std::runtime_error(std::string(buf));
}

// ----------------------------------------------------------------------------

// ep128emu/src/ep128emu.hpp, ep128emu/src/comprlib.cpp

#if defined(__GNUC__) && (__GNUC__ >= 3) && !defined(__ICC)
#  define EP128EMU_INLINE         __attribute__ ((__always_inline__)) inline
#  define EP128EMU_EXPECT(x__)    __builtin_expect((x__), 1)
#  define EP128EMU_UNLIKELY(x__)  __builtin_expect((x__), 0)
#else
#  define EP128EMU_INLINE         inline
#  define EP128EMU_EXPECT(x__)    x__
#  define EP128EMU_UNLIKELY(x__)  x__
#endif

class RadixTree {
 protected:
  friend class LZSearchTable;
  // Each node has a size of 3 elements in the buffer:
  //        0:  length of the sub-string at this node
  //        1:  start position of the full sequence in the input data
  //        2:  buffer position of table of child nodes (0: none) for the
  //            2 most significant bits of the next character.
  //            A chain of 4 such tables leads to the next node, each one
  //            contains 4 buffer positions for the next 2 bits of the
  //            character
  // The first four elements of the buffer are unused (always zero)
  std::vector< unsigned int > buf;
  unsigned int  bufPos;
  // --------
  EP128EMU_INLINE unsigned int findNextNode(unsigned char c);
  void allocNode(unsigned char c);
  // returns the matching prefix length between 'p1' and 'p2'
  static EP128EMU_INLINE unsigned int compareStrings(
      const unsigned char *p1, size_t l1, const unsigned char *p2, size_t l2);
 public:
  RadixTree(size_t bufSize_ = 0x00100000);
  ~RadixTree();
  // writes the shortest offset - 1 of matches found to offsTable[1..maxLen]
  // (each offset is stored only at its maximum length), and returns the
  // maximum length; for each inBufPos, findMatches() should be called first,
  // then addString()
  size_t findMatches(unsigned int *offsTable,
                     const unsigned char *inBuf, size_t inBufPos,
                     size_t maxLen, size_t maxDistance = 0xFFFFFFFFU);
  void addString(const unsigned char *inBuf, size_t inBufPos, size_t len);
  // returns the position of 's' in inBuf, or -1 if not found
  long findString(const unsigned char *inBuf,
                  const unsigned char *s, size_t len);
  void clear();
};

EP128EMU_INLINE unsigned int RadixTree::findNextNode(unsigned char c)
{
  unsigned int  nextPos = buf[bufPos + 2U];
  nextPos = buf[nextPos + ((c >> 6) & 3)];
  nextPos = buf[nextPos + ((c >> 4) & 3)];
  nextPos = buf[nextPos + ((c >> 2) & 3)];
  nextPos = buf[nextPos + (c & 3)];
  return nextPos;
}

EP128EMU_INLINE unsigned int RadixTree::compareStrings(
    const unsigned char *p1, size_t l1, const unsigned char *p2, size_t l2)
{
  size_t  l = (l1 < l2 ? l1 : l2);
  if (l < 1 || p1[0] != p2[0])
    return 0U;
  if (l < 2 || p1[1] != p2[1])
    return 1U;
  if (l >= 8) {
    if (std::memcmp(p1 + 2, p2 + 2, l - 2) == 0)
      return l;
  }
  size_t  i = 2;
  for ( ; (i + 4) <= l; i = i + 4) {
    if (((p1[i] ^ p2[i]) | (p1[i + 1] ^ p2[i + 1])
         | (p1[i + 2] ^ p2[i + 2]) | (p1[i + 3] ^ p2[i + 3])) != 0) {
      break;
    }
  }
  for ( ; i < l; i++) {
    if (p1[i] != p2[i])
      return (unsigned int) i;
  }
  return (unsigned int) l;
}

RadixTree::RadixTree(size_t bufSize_)
  : bufPos(4U)
{
  buf.reserve(bufSize_);
  buf.resize(7, 0U);
}

RadixTree::~RadixTree()
{
}

size_t RadixTree::findMatches(unsigned int *offsTable,
                              const unsigned char *inBuf, size_t inBufPos,
                              size_t maxLen, size_t maxDistance)
{
  bufPos = 4U;
  if (buf[bufPos] == 0U) {
    bufPos = findNextNode(inBuf[inBufPos]);
    if (!bufPos)
      return 0;
  }
  else if (inBuf[inBufPos] != inBuf[buf[bufPos + 1U]]) {
    return 0;
  }
  size_t  len = 0;
  do {
    unsigned int  matchPos = buf[bufPos + 1U];
    unsigned int  d = (unsigned int) inBufPos - matchPos;
    if (d > maxDistance)
      return len;
    do {
      unsigned int  l =
          RadixTree::compareStrings(
              inBuf + (inBufPos + len), maxLen - len,
              inBuf + size_t(buf[bufPos + 1U]) + len, size_t(buf[bufPos]));
      len = len + size_t(l);
      if (l < buf[bufPos] || len >= maxLen) {
        bufPos = 0U;
        if (!l)
          return len;
        break;
      }
      bufPos = findNextNode(inBuf[inBufPos + len]);
    } while (bufPos && buf[bufPos + 1U] == matchPos);
    offsTable[len] = d - 1U;
  } while (bufPos);
  return len;
}

void RadixTree::allocNode(unsigned char c)
{
  unsigned int  nextPos = buf[bufPos + 2U];
  unsigned char nBits = 6;
  unsigned int  bufSize = (unsigned int) buf.size();
  if (nextPos) {
    unsigned int  nextPos_;
    while ((nextPos_ = buf[nextPos + ((c >> nBits) & 3)]) != 0U) {
      nextPos = nextPos_;
      nBits = nBits - 2;
    }
  }
  else {
    nextPos = bufSize;
    bufSize = bufSize + 4U;
    buf[bufPos + 2U] = nextPos;
  }
  unsigned int  nextPos_ = bufSize;
  bufSize = bufSize + (3U + (nBits << 1));
  if (EP128EMU_UNLIKELY(size_t(bufSize) > buf.capacity()))
    buf.reserve(((bufSize + (bufSize >> 2)) | 0x3FFFU) + 1U);
  buf.resize(bufSize, 0U);
  do {
    buf[nextPos + ((c >> nBits) & 3)] = nextPos_;
    nextPos = nextPos_;
    nextPos_ = nextPos_ + 4U;
    nBits = nBits - 2;
  } while ((signed char) nBits >= 0);
  bufPos = nextPos;
}

void RadixTree::addString(const unsigned char *inBuf, size_t inBufPos,
                          size_t len)
{
  bufPos = 4U;
  if (buf[bufPos] == 0U) {
    unsigned char c = inBuf[inBufPos];
    unsigned int  nextPos = findNextNode(c);
    if (!nextPos) {
      // empty tree or new leaf node
      if (buf[bufPos + 2U])
        allocNode(c);
      buf[bufPos] = (unsigned int) len;
      buf[bufPos + 1U] = (unsigned int) inBufPos;
      buf[bufPos + 2U] = 0U;
      return;
    }
    bufPos = nextPos;
  }
  for (size_t n = 0; n < len; ) {
    if (buf[bufPos] == 1U &&
        inBuf[inBufPos + n] == inBuf[size_t(buf[bufPos + 1U]) + n]) {
      do {
        buf[bufPos + 1U] = (unsigned int) inBufPos;
        if (++n >= len)
          return;
        unsigned int  nextPos = findNextNode(inBuf[inBufPos + n]);
        if (!nextPos) {
          allocNode(inBuf[inBufPos + n]);
          buf[bufPos] = (unsigned int) (len - n);
          buf[bufPos + 1U] = (unsigned int) inBufPos;
          return;
        }
        bufPos = nextPos;
      } while (buf[bufPos] == 1U);
    }
    unsigned int  l =
        RadixTree::compareStrings(inBuf + (inBufPos + n), len - n,
                                  inBuf + (size_t(buf[bufPos + 1U]) + n),
                                  size_t(buf[bufPos]));
    n = n + size_t(l);
    if (l >= buf[bufPos]) {
      // full match, update position
      buf[bufPos + 1U] = (unsigned int) inBufPos;
      if (n >= len)
        break;
      unsigned char c = inBuf[inBufPos + n];
      unsigned int  nextPos = findNextNode(c);
      if (nextPos) {
        bufPos = nextPos;
        continue;
      }
      allocNode(c);
      if (!buf[bufPos]) {
        // new leaf node
        buf[bufPos] = (unsigned int) (len - n);
        buf[bufPos + 1U] = (unsigned int) inBufPos;
        break;
      }
    }
    else {
      // partial match, need to split the original sub-string of the node
      unsigned int  savedBufPos = bufPos;
      unsigned int  savedChildrenPos = buf[bufPos + 2U];
      buf[bufPos + 2U] = 0U;
      allocNode(inBuf[buf[bufPos + 1U] + (unsigned int) n]);
      buf[bufPos] = buf[savedBufPos] - l;
      buf[bufPos + 1U] = buf[savedBufPos + 1U];
      buf[bufPos + 2U] = savedChildrenPos;
      buf[savedBufPos] = l;
      buf[savedBufPos + 1U] = (unsigned int) inBufPos;
      if (n < len) {
        // create new leaf node
        bufPos = savedBufPos;
        allocNode(inBuf[inBufPos + n]);
        buf[bufPos] = (unsigned int) (len - n);
        buf[bufPos + 1U] = (unsigned int) inBufPos;
      }
      break;
    }
  }
}

long RadixTree::findString(const unsigned char *inBuf,
                           const unsigned char *s, size_t len)
{
  if (len < 1)
    return 0L;
  bufPos = 4U;
  if (buf[bufPos] == 0U)
    bufPos = findNextNode(*s);
  else if (*s != inBuf[buf[bufPos + 1U]])
    return -1L;
  for (size_t i = 0; bufPos != 0U; ) {
    unsigned int  l =
        RadixTree::compareStrings(
            s + i, len - i,
            inBuf + size_t(buf[bufPos + 1U]) + i, size_t(buf[bufPos]));
    i = i + size_t(l);
    if (i >= len)
      return long(buf[bufPos + 1U]);
    if (!l)
      return -1L;
    bufPos = findNextNode(s[i]);
  }
  return -1L;
}

void RadixTree::clear()
{
  buf.clear();
  buf.resize(7, 0U);
  bufPos = 4U;
}

// ----------------------------------------------------------------------------

static void optimizeEnvelopes(std::vector< unsigned short >& envBuf,
                              std::vector< int >& envUsed,
                              const std::vector< size_t >& len,
                              const std::vector< size_t >& offs)
{
  std::vector< unsigned short > tmpEnv(envBuf);
  envBuf.clear();
  envUsed.clear();
  std::vector< int >  tmpEnvUsed(tmpEnv.size(), 0);
  size_t  nFrames = len.size() >> 3;
  for (size_t i = 0; i < (nFrames * 8); i = i + len[i]) {
    if (offs[i] & rleOffsetFlag)
      continue;
    for (size_t j = 0; j < len[i]; j++)
      tmpEnvUsed[offs[i] + j]++;
  }
  int     n = 0;
  for (size_t i = 0; i < tmpEnvUsed.size(); i++) {
    if (tmpEnvUsed[i] > 0) {
      envBuf.push_back(tmpEnv[i]);
      envUsed.push_back(n);
      n += tmpEnvUsed[i];
    }
  }
  envUsed.push_back(n);
}

static void createEnvTables(std::vector< unsigned char >& outBuf,
                            size_t loadAddr,
                            std::map< size_t, unsigned char >& npMapF,
                            std::map< size_t, unsigned char >& npMapV,
                            const std::vector< unsigned short >& envBuf,
                            const std::vector< size_t >& len,
                            const std::vector< size_t >& offs)
{
  outBuf.clear();
  npMapF.clear();
  npMapV.clear();
  outBuf.resize(envBuf.size() * 2 + 0x0310, 0);
  size_t  nFrames = len.size() >> 3;
  for (size_t i = 0; i < 2; i++) {
    std::map< size_t, int > envCnts;
    for (size_t j = (nFrames * 4) * i;
         j < ((nFrames * 4) * (i + 1)); j = j + len[j]) {
      size_t  key = (offs[j] << 8) | len[j];
      int     n =
          ((i == 0 && j >= (nFrames * 3) && (offs[j] & rleOffsetFlag) != 0) ?
           2 : 3);
      std::map< size_t, int >::iterator k = envCnts.find(key);
      if (k != envCnts.end())
        k->second = k->second + n;
      else
        envCnts.insert(std::pair< size_t, int >(key, n));
    }
    std::vector< unsigned long long > tmp;
    for (std::map< size_t, int >::iterator j = envCnts.begin();
         j != envCnts.end(); j++) {
      tmp.push_back(((unsigned long long) (0x7FFFFFFF - j->second) << 32)
                    | j->first);
    }
    std::sort(tmp.begin(), tmp.end());
    for (unsigned char j = 0; j < 128; j++) {
      size_t  np = 0;
      unsigned char   l = 0;
      if (j < tmp.size()) {
        np = size_t(tmp[j] & 0xFFFFFFFFUL);
        if (!i)
          npMapF.insert(std::pair< size_t, unsigned char >(np, j | 0x80));
        else
          npMapV.insert(std::pair< size_t, unsigned char >(np, j | 0x80));
        l = (unsigned char) (((np & 0xFF) - 1) << 1);
        np = np >> 8;
        if (!(np & rleOffsetFlag)) {
          np = (np << 1) + loadAddr + 0x0310;
          l = l | 0x80;
        }
        np = np & 0xFFFF;
      }
      if (!i) {
        outBuf[size_t(j << 1) | 0x0000] = (unsigned char) (np & 0xFF);
        outBuf[size_t(j << 1) | 0x0001] = (unsigned char) (np >> 8);
        outBuf[size_t(j << 1) | 0x0100] = l;
      }
      else {
        outBuf[size_t(j << 1) | 0x0200] = (unsigned char) (np & 0xFF);
        outBuf[size_t(j << 1) | 0x0201] = (unsigned char) (np >> 8);
        outBuf[size_t(j << 1) | 0x0101] = l;
      }
    }
  }
  for (size_t i = 0; i < envBuf.size(); i++) {
    outBuf[0x0310 + (i << 1)] = (unsigned char) (envBuf[i] & 0xFF);
    outBuf[0x0310 + (i << 1) + 1] = (unsigned char) (envBuf[i] >> 8);
  }
}

// ----------------------------------------------------------------------------

static size_t optimizeTrackData(
    std::vector< size_t >& len, std::vector< size_t >& offs, size_t maxLen,
    const std::vector< unsigned short >& inBuf,
    const std::vector< unsigned short >& envBuf,
    const std::vector< int >& envUsed,
    const std::map< size_t, unsigned char >& npMapF,
    const std::map< size_t, unsigned char >& npMapV)
{
  size_t  nFrames = inBuf.size() >> 3;
  std::vector< size_t > byteCnts(nFrames * 8 + 1, 0);
  std::vector< size_t > byteCntsNoEnv(nFrames * 8 + 1, 0);
  RadixTree   envTree;
  for (size_t i = envBuf.size(); i-- > 0; ) {
    size_t  n = envBuf.size() - i;
    if (n > maxLen)
      n = maxLen;
    envTree.addString(reinterpret_cast< const unsigned char * >(
                          &(envBuf.front())),
                      i * sizeof(unsigned short), n * sizeof(unsigned short));
  }
  for (size_t i = nFrames * 8; i-- > 0; ) {
    const std::map< size_t, unsigned char >&  npMap =
        (i < (nFrames * 4) ? npMapF : npMapV);
    size_t  n = (nFrames * 8 - i) % nFrames;
    if (!n)
      n = nFrames;
    if (n > lengthMaxValue)
      n = lengthMaxValue;
    size_t  bestSize = 0x7FFFFFFF;
    size_t  bestSizeNoEnv = 0x7FFFFFFF;
    size_t  bestLen = 1;
    size_t  bestOffs = rleOffsetFlag;
    bool    isRLE = true;
    for (size_t j = 1; j <= n; j++) {
      isRLE = (isRLE && (inBuf[i + j - 1] == inBuf[i]));
      if (j > maxLen && !isRLE)
        break;
      if (isRLE) {
        size_t  nBytes = byteCnts[i + j] + 1;
        if (npMap.find(((size_t(inBuf[i]) | rleOffsetFlag) << 8) | j)
            == npMap.end()) {
          nBytes = nBytes + 2 - size_t(i >= (nFrames * 3) && i < (nFrames * 4));
        }
        if (nBytes <= bestSize) {
          bestSize = nBytes;
          bestSizeNoEnv = nBytes + byteCntsNoEnv[i + j] - byteCnts[i + j];
          bestLen = j;
          bestOffs = size_t(inBuf[i]) | rleOffsetFlag;
        }
      }
      if (j < 2)
        continue;
      long    k =
          envTree.findString(reinterpret_cast< const unsigned char * >(
                                 &(envBuf.front())),
                             reinterpret_cast< const unsigned char * >(
                                 &(inBuf.front()))
                             + (i * sizeof(unsigned short)),
                             j * sizeof(unsigned short));
      if (k < 0L) {
        if (isRLE)
          continue;
        break;
      }
      k = long(size_t(k) / sizeof(unsigned short));
      size_t  envBytes = 0;
      if (envUsed.size() > 0) {
        envBytes = size_t(envUsed[size_t(k) + j] - envUsed[size_t(k)]);
        if (envBytes > j)
          envBytes = (j * j * 2) / envBytes;
        else
          envBytes = j * 2;
      }
      envBytes += (byteCnts[i + j] - byteCntsNoEnv[i + j]);
      size_t  nBytes = byteCntsNoEnv[i + j] + 1;
      if (npMap.find((size_t(k) << 8) | j) == npMap.end())
        nBytes = nBytes + 2;
      if ((nBytes + envBytes) < bestSize ||
          ((nBytes + envBytes) == bestSize && nBytes > bestSizeNoEnv)) {
        bestSize = nBytes + envBytes;
        bestSizeNoEnv = nBytes;
        bestLen = j;
        bestOffs = size_t(k);
      }
    }
    byteCnts[i] = bestSize;
    byteCntsNoEnv[i] = bestSizeNoEnv;
    len[i] = bestLen;
    offs[i] = bestOffs;
  }
  return (byteCntsNoEnv[0] + 1);
}

static void writeTrackData(std::vector< unsigned char >& outBuf,
                           size_t loadAddr,
                           const std::vector< size_t >& len,
                           const std::vector< size_t >& offs,
                           const std::map< size_t, unsigned char >& npMapF,
                           const std::map< size_t, unsigned char >& npMapV)
{
  size_t  nFrames = len.size() >> 3;
  size_t  trackOffs = loadAddr + outBuf.size();
  for (size_t i = 0; i < len.size(); i = i + len[i]) {
    if ((i % nFrames) == 0) {
      if (i == nFrames)
        outBuf.push_back(0x40);
      trackOffs = loadAddr + outBuf.size();
      outBuf[768 + ((i / nFrames) << 1)] = (unsigned char) (trackOffs & 0xFF);
      outBuf[768 + ((i / nFrames) << 1) + 1] = (unsigned char) (trackOffs >> 8);
    }
    const std::map< size_t, unsigned char >&  npMap =
        (i < (nFrames * 4) ? npMapF : npMapV);
    std::map< size_t, unsigned char >::const_iterator j =
        npMap.find((offs[i] << 8) | len[i]);
    if (j != npMap.end()) {
      outBuf.push_back(j->second);
    }
    else if (!(offs[i] & rleOffsetFlag)) {
      outBuf.push_back((unsigned char) ((len[i] - 1) | 0x40));
      size_t  tmp = (offs[i] << 1) + loadAddr + 0x0310;
      outBuf.push_back((unsigned char) (tmp & 0xFF));
      outBuf.push_back((unsigned char) (tmp >> 8));
    }
    else {
      outBuf.push_back((unsigned char) (len[i] - 1));
      outBuf.push_back((unsigned char) (offs[i] & 0xFF));
      if (!(i >= (nFrames * 3) && i < (nFrames * 4)))
        outBuf.push_back((unsigned char) (offs[i] >> 8));
    }
  }
}

// ----------------------------------------------------------------------------

static size_t loadInputFile(std::vector< unsigned short >& inBuf,
                            const char *fileName, int zeroVolMode)
{
  inBuf.clear();
  std::vector< unsigned char >  tmpBuf;
  {
    std::FILE *f = std::fopen(fileName, "rb");
    if (!f)
      errorMessage("error opening input file '%s'\n", fileName);
    int     c;
    while ((c = std::fgetc(f)) != EOF)
      tmpBuf.push_back((unsigned char) (c & 0xFF));
    std::fclose(f);
  }
  size_t  nFrames = tmpBuf.size() >> 4;
  tmpBuf.resize(nFrames * 16);
  inBuf.resize(nFrames * 8, 0);
  for (size_t i = 0; i < tmpBuf.size(); i = i + 16) {
    tmpBuf[i + 7] = 0x00;
    for (size_t j = 8; j < 16; j++)
      tmpBuf[i + j] = tmpBuf[i + j] & 0x3F;
    for (int j = 0; j < 4; j++) {
      unsigned char *vol_l = &(tmpBuf.front()) + (i + 8);
      unsigned char *vol_r = &(tmpBuf.front()) + (i + 12);
      unsigned char *freq = &(tmpBuf.front()) + i;
      for (size_t c = 0; c < 4; c++) {
        if ((vol_l[c] | vol_r[c]) == 0 &&
            (freq[c << 1] | freq[(c << 1) + 1]) != 0) {
          // volume == 0, frequency != 0:
          // check if other channels depend on this channel
          vol_l[c] = vol_l[c] | 0x40;
          vol_r[c] = vol_r[c] | 0x40;
          do {
            if (freq[((c ^ 2) << 1) + size_t(c != 1)] & 0x80) {
              // ring modulation to channel + 2
              if ((vol_l[c ^ 2] | vol_r[c ^ 2]) != 0)
                break;
            }
            if (freq[(((c + 3) & 3) << 1) + size_t(c != 0)] & 0x40) {
              // highpass filter to channel - 1
              if ((vol_l[(c + 3) & 3] | vol_r[(c + 3) & 3]) != 0)
                break;
            }
            if (c == 2 && (freq[6] & 0x20) != 0) {
              // lowpass filter to noise channel
              if ((vol_l[3] | vol_r[3]) != 0)
                break;
            }
            if ((freq[6] & 0x03) == (c + 1)) {
              // clock source to noise channel
              if ((vol_l[3] | vol_r[3]) != 0)
                break;
            }
            vol_l[c] = vol_l[c] & 0x3F;
            vol_r[c] = vol_r[c] & 0x3F;
          } while (false);
        }
      }
    }
  }
  for (size_t c = 0; c < 4; c++) {
    int     prvFreq = 0;
    for (size_t i = 0; i < tmpBuf.size(); i = i + 16) {
      unsigned char vol_l = tmpBuf[i + 8 + c];
      unsigned char vol_r = tmpBuf[i + 12 + c];
      int     freq = int(tmpBuf[i + (c << 1)]);
      if (c < 3) {
        freq = freq | (int(tmpBuf[i + (c << 1) + 1]) << 8);
        if (freq <= 1 && zeroVolMode >= 0) {
          vol_l = 0;
          vol_r = 0;
          freq = 0;
        }
      }
      if ((vol_l | vol_r) == 0) {
        if (zeroVolMode == 1)
          freq = 0;
        else if (zeroVolMode == 2)
          freq = prvFreq;
      }
      prvFreq = freq;
      inBuf[(i >> 4) + (c * nFrames)] = (unsigned short) freq;
      inBuf[(i >> 4) + ((c + 4) * nFrames)] =
          (unsigned short) (vol_l & 0x3F)
          | ((unsigned short) (vol_r & 0x3F) << 8);
    }
  }
  return nFrames;
}

// ----------------------------------------------------------------------------

static void decodeFile(std::vector< unsigned char >& outBuf,
                       const std::vector< unsigned char >& inBuf,
                       size_t loadAddr)
{
  outBuf.clear();
  if (inBuf.size() < 0x0311)
    errorMessage("invalid data size");
  for (size_t i = 0; i < 8; i++) {
    size_t  p =
        (size_t(inBuf[0x0301 + (i << 1)]) << 8) | inBuf[0x0300 + (i << 1)];
    if (p < (loadAddr + 0x0310))
      errorMessage("invalid track offset");
    p = p - loadAddr;
    for (size_t n = 0; true; ) {
      if (i > 0 && n >= (outBuf.size() >> 4))
        break;
      if (p >= inBuf.size())
        errorMessage("unexpected end of data");
      unsigned char c = inBuf[p];
      size_t  offs = 0;
      p++;
      if (c >= 0x80) {
        c = (c & 0x7F) << 1;
        if (i < 4) {
          offs = (size_t(inBuf[size_t(c) | 0x0001]) << 8)
                 | inBuf[size_t(c) | 0x0000];
          c = inBuf[size_t(c) | 0x0100] >> 1;
        }
        else {
          offs = (size_t(inBuf[size_t(c) | 0x0201]) << 8)
                 | inBuf[size_t(c) | 0x0200];
          c = inBuf[size_t(c) | 0x0101] >> 1;
        }
      }
      else {
        if (p >= inBuf.size())
          errorMessage("unexpected end of data");
        offs = inBuf[p];
        p++;
        if (!(c < 0x40 && i == 3)) {
          if (p >= inBuf.size())
            errorMessage("unexpected end of data");
          offs = offs | (size_t(inBuf[p]) << 8);
          p++;
        }
      }
      if (c == 0x40 && i == 0)
        break;
      if (c & 0x40) {
        if (offs < (loadAddr + 0x0310))
          errorMessage("invalid envelope data offset");
        offs = offs - loadAddr;
      }
      for (size_t len = (c & 0x3F) + 1; len-- > 0; n++) {
        if (i == 0)
          outBuf.resize(outBuf.size() + 16, 0);
        size_t  r0 = (i < 4 ? (i << 1) : (i + 4));
        size_t  r1 = (i < 4 ? (r0 + 1) : (r0 + 4));
        unsigned char   v0 = (unsigned char) (offs & 0xFF);
        unsigned char   v1 = (unsigned char) (offs >> 8);
        if (c & 0x40) {
          if ((offs + 1) >= inBuf.size())
            errorMessage("unexpected end of data");
          v0 = inBuf[offs];
          v1 = inBuf[offs + 1];
          offs = offs + 2;
        }
        if (i == 3)
          v1 = 0;
        outBuf[(n << 4) | r0] = v0;
        outBuf[(n << 4) | r1] = v1;
      }
    }
  }
}

// ----------------------------------------------------------------------------

static void parseCommandLine(int argc, char **argv,
                             size_t& loadAddr, size_t& maxLen1,
                             int& zeroVolMode)
{
  if (argc < 3 || argc > 6) {
    std::fprintf(stderr,
                 "Usage: %s INFILE.DAV OUTFILE [LOADADDR [MAXLEN1 [f|h|z]]]\n",
                 argv[0]);
    std::fprintf(stderr,
                 "    LOADADDR    load address of output file "
                 "(default: 0x%04X)\n", (unsigned int) loadAddr);
    std::fprintf(stderr,
                 "    MAXLEN1     initial maximum envelope length (2..64) "
                 "(default: %d)\n", int(maxLen1));
    std::fprintf(stderr,
                 "    f           preserve tones at frequency registers < 2\n");
    std::fprintf(stderr,
                 "    h           hold previous frequency at zero volume\n");
    std::fprintf(stderr,
                 "    z           frequency registers = 0 at zero volume\n");
    errorMessage("invalid number of arguments");
  }
  if (argv[1][0] == '\0')
    errorMessage("invalid input file name");
  if (argv[2][0] == '\0')
    errorMessage("invalid output file name");
  if (argc >= 4) {
    char    *endp = (char *) 0;
    loadAddr = size_t(std::strtol(argv[3], &endp, 0));
    if (!endp || endp == argv[3] || *endp != '\0' ||
        (loadAddr & 0xFF00) != loadAddr) {
      errorMessage("invalid load address");
    }
  }
  if (argc >= 5) {
    char    *endp = (char *) 0;
    maxLen1 = size_t(std::strtol(argv[4], &endp, 0));
    if (!endp || endp == argv[4] || *endp != '\0' ||
        maxLen1 < 2 || maxLen1 > 64) {
      errorMessage("invalid initial maximum length");
    }
  }
  if (argc >= 6) {
    char    c = argv[5][0] & char(0xDF);
    if (c != '\0' && argv[5][1] == '\0') {
      switch (c) {
      case 'F':
        zeroVolMode = -1;
        return;
      case 'H':
        zeroVolMode = 2;
        return;
      case 'Z':
        zeroVolMode = 1;
        return;
      }
    }
    errorMessage("invalid zero volume mode");
  }
}

int main(int argc, char **argv)
{
  size_t  loadAddr = 0x1000;
  size_t  maxLen1 = 12;
  try {
    std::vector< unsigned short > inBuf;
    size_t  nFrames = 0;
    {
      int     zeroVolMode = 0;
      parseCommandLine(argc, argv, loadAddr, maxLen1, zeroVolMode);
      nFrames = loadInputFile(inBuf, argv[1], zeroVolMode);
      if (nFrames < 1)
        errorMessage("empty input file");
    }
    std::vector< unsigned char >  outBuf;
    std::vector< unsigned short > envBuf(inBuf);
    std::vector< int >  envUsed;
    std::vector< size_t >   len(nFrames * 8);
    std::vector< size_t >   offs(nFrames * 8);
    std::map< size_t, unsigned char > npMapF;
    std::map< size_t, unsigned char > npMapV;
    size_t  prvTotalSize = 0x7FFFFFFF;
    for (size_t passCnt = 1; true; passCnt++) {
      size_t  maxLen = passCnt * maxLen1;
      if (maxLen > lengthMaxValue)
        maxLen = lengthMaxValue;
      if (passCnt > 1)
        optimizeEnvelopes(envBuf, envUsed, len, offs);
      size_t  envSize = envBuf.size() << 1;
      if (passCnt > 1) {
        outBuf.clear();
        npMapF.clear();
        npMapV.clear();
        (void) optimizeTrackData(len, offs, maxLen, inBuf,
                                 envBuf, envUsed, npMapF, npMapV);
        createEnvTables(outBuf, loadAddr, npMapF, npMapV, envBuf, len, offs);
      }
      size_t  trkSize = optimizeTrackData(len, offs, maxLen, inBuf,
                                          envBuf, envUsed, npMapF, npMapV);
      size_t  totalSize = envSize + trkSize + 0x0310;
      std::fprintf(stderr,
                   "\rPass %2d: file size: %5u bytes, "
                   "envelope data: %5u, track data: %5u  ",
                   int(passCnt), (unsigned int) totalSize,
                   (unsigned int) envSize, (unsigned int) trkSize);
      if (totalSize < prvTotalSize) {
        prvTotalSize = totalSize;
      }
      else {
        createEnvTables(outBuf, loadAddr, npMapF, npMapV, envBuf, len, offs);
        envSize = envBuf.size() << 1;
        envUsed.clear();
        trkSize = optimizeTrackData(len, offs, maxLen, inBuf,
                                    envBuf, envUsed, npMapF, npMapV);
        writeTrackData(outBuf, loadAddr, len, offs, npMapF, npMapV);
        totalSize = outBuf.size();
        std::fprintf(stderr,
                     "\rPass %2d: file size: %5u bytes, "
                     "envelope data: %5u, track data: %5u  \n",
                     int(passCnt + 1), (unsigned int) totalSize,
                     (unsigned int) envSize, (unsigned int) trkSize);
        break;
      }
    }
    try {
      std::vector< unsigned char >  tmpBuf;
      decodeFile(tmpBuf, outBuf, loadAddr);
      if (tmpBuf.size() != (nFrames * 16))
        errorMessage("data size does not match input");
      for (size_t i = 0; i < 8; i++) {
        size_t  r0 = (i < 4 ? (i << 1) : (i + 4));
        size_t  r1 = (i < 4 ? (r0 + 1) : (r0 + 4));
        for (size_t j = 0; j < nFrames; j++) {
          unsigned char v0 = (unsigned char) (inBuf[i * nFrames + j] & 0xFF);
          unsigned char v1 = (unsigned char) (inBuf[i * nFrames + j] >> 8);
          if (tmpBuf[(j << 4) | r0] != v0 || tmpBuf[(j << 4) | r1] != v1)
            errorMessage("decoded data differs from input");
        }
      }
    }
    catch (...) {
      std::fprintf(stderr, " *** internal error: verifying output failed:\n");
      throw;
    }
    std::FILE *f = std::fopen(argv[2], "wb");
    if (!f)
      errorMessage("error opening output file '%s'", argv[2]);
    try {
      if (std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(),
                      f) != outBuf.size() ||
          std::fflush(f) != 0) {
        errorMessage("error writing output file '%s'", argv[2]);
      }
      std::fclose(f);
      f = (std::FILE *) 0;
    }
    catch (...) {
      std::fclose(f);
      throw;
    }
  }
  catch (std::exception& e) {
    std::fprintf(stderr, " *** %s: %s\n", argv[0], e.what());
    return -1;
  }
  return 0;
}
