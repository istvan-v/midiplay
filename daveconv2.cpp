
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
static const size_t loadAddrDiffFlag = 0x0001;
static const size_t loadAddrNIFlag = 0x0002;
static const size_t loadAddr1TrkFlag = 0x0004;
static const size_t loadAddrDecodeFlag = 0x0008;

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

template < typename T > class RadixTree {
 protected:
  // Each node has a size of 3 elements in the buffer:
  //        0:  length of the sub-string at this node
  //        1:  start position of the full sequence in the input data
  //        2:  buffer position of table of child nodes (0: none) for the
  //            2 most significant bits of the next character.
  //            A chain of such tables leads to the next node, each one
  //            contains 4 buffer positions for the next 2 bits of the
  //            character
  // The first four elements of the buffer are unused (always zero)
  std::vector< unsigned int > buf;
  // --------
  EP128EMU_INLINE unsigned int findNextNode(T c, unsigned int bufPos) const;
  unsigned int allocNode(T c, unsigned int bufPos);
  // returns the matching prefix length between 'p1' and 'p2'
  static EP128EMU_INLINE unsigned int compareStrings(
      const T *p1, size_t l1, const T *p2, size_t l2);
 public:
  RadixTree(size_t bufSize_ = 0x00100000);
  ~RadixTree();
  // writes the shortest offsets of matches found to offsTable[0..maxLen-1],
  // and returns the maximum length
  size_t findMatches(unsigned int *offsTable, const T *inBuf,
                     const T *s, size_t maxLen) const;
  // returns the position of 's' in inBuf, or -1 if not found
  long findString(const T *inBuf, const T *s, size_t len) const;
  void addString(const T *inBuf, size_t inBufPos, size_t len);
  void clear();
};

template < typename T > EP128EMU_INLINE
    unsigned int RadixTree< T >::findNextNode(T c, unsigned int bufPos) const
{
  unsigned int  nextPos = buf[bufPos + 2U];
  for (unsigned char b = (unsigned char) (sizeof(T) * 8 - 2);
       b >= 8; b = b - 2) {
    nextPos = buf[nextPos + ((c >> b) & 3)];
  }
  nextPos = buf[nextPos + ((c >> 6) & 3)];
  nextPos = buf[nextPos + ((c >> 4) & 3)];
  nextPos = buf[nextPos + ((c >> 2) & 3)];
  nextPos = buf[nextPos + (c & 3)];
  return nextPos;
}

template < typename T > EP128EMU_INLINE
    unsigned int RadixTree< T >::compareStrings(
        const T *p1, size_t l1, const T *p2, size_t l2)
{
  size_t  l = (l1 < l2 ? l1 : l2);
  if (l < 1 || p1[0] != p2[0])
    return 0U;
  if (l < 2 || p1[1] != p2[1])
    return 1U;
  if (l >= 8) {
    if (std::memcmp(p1 + 2, p2 + 2, (l - 2) * sizeof(T)) == 0)
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

template < typename T > RadixTree< T >::RadixTree(size_t bufSize_)
{
  buf.reserve(bufSize_);
  buf.resize(7, 0U);
}

template < typename T > RadixTree< T >::~RadixTree()
{
}

template < typename T >
    size_t RadixTree< T >::findMatches(unsigned int *offsTable, const T *inBuf,
                                       const T *s, size_t maxLen) const
{
  unsigned int  bufPos = 4U;
  if (buf[bufPos] == 0U) {
    bufPos = findNextNode(*s, bufPos);
    if (!bufPos)
      return 0;
  }
  else if (*s != inBuf[buf[bufPos + 1U]]) {
    return 0;
  }
  size_t  len = 0;
  do {
    unsigned int  matchPos = buf[bufPos + 1U];
    do {
      unsigned int  l =
          RadixTree::compareStrings(
              s + len, maxLen - len,
              inBuf + size_t(buf[bufPos + 1U]) + len, size_t(buf[bufPos]));
      for (unsigned int i = 0U; i < l; i++, len++)
        offsTable[len] = matchPos;
      if (l < buf[bufPos] || len >= maxLen) {
        bufPos = 0U;
        if (!l)
          return len;
        break;
      }
      bufPos = findNextNode(s[len], bufPos);
    } while (bufPos && buf[bufPos + 1U] == matchPos);
  } while (bufPos);
  return len;
}

template < typename T >
    long RadixTree< T >::findString(const T *inBuf,
                                    const T *s, size_t len) const
{
  if (len < 1)
    return 0L;
  unsigned int  bufPos = 4U;
  if (buf[bufPos] == 0U)
    bufPos = findNextNode(*s, bufPos);
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
    bufPos = findNextNode(s[i], bufPos);
  }
  return -1L;
}

template < typename T >
    unsigned int RadixTree< T >::allocNode(T c, unsigned int bufPos)
{
  unsigned int  nextPos = buf[bufPos + 2U];
  unsigned char nBits = (unsigned char) (sizeof(T) * 8 - 2);
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
  return nextPos;
}

template < typename T >
    void RadixTree< T >::addString(const T *inBuf, size_t inBufPos, size_t len)
{
  unsigned int  bufPos = 4U;
  if (buf[bufPos] == 0U) {
    T       c = inBuf[inBufPos];
    unsigned int  nextPos = findNextNode(c, bufPos);
    if (!nextPos) {
      // empty tree or new leaf node
      if (buf[bufPos + 2U])
        bufPos = allocNode(c, bufPos);
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
        unsigned int  nextPos = findNextNode(inBuf[inBufPos + n], bufPos);
        if (!nextPos) {
          bufPos = allocNode(inBuf[inBufPos + n], bufPos);
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
      T       c = inBuf[inBufPos + n];
      unsigned int  nextPos = findNextNode(c, bufPos);
      if (nextPos) {
        bufPos = nextPos;
        continue;
      }
      bufPos = allocNode(c, bufPos);
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
      bufPos = allocNode(inBuf[buf[bufPos + 1U] + (unsigned int) n], bufPos);
      buf[bufPos] = buf[savedBufPos] - l;
      buf[bufPos + 1U] = buf[savedBufPos + 1U];
      buf[bufPos + 2U] = savedChildrenPos;
      buf[savedBufPos] = l;
      buf[savedBufPos + 1U] = (unsigned int) inBufPos;
      if (n < len) {
        // create new leaf node
        bufPos = savedBufPos;
        bufPos = allocNode(inBuf[inBufPos + n], bufPos);
        buf[bufPos] = (unsigned int) (len - n);
        buf[bufPos + 1U] = (unsigned int) inBufPos;
      }
      break;
    }
  }
}

template < typename T > void RadixTree< T >::clear()
{
  buf.clear();
  buf.resize(7, 0U);
}

// ----------------------------------------------------------------------------

static void optimizeEnvelopes(std::vector< unsigned short >& envBuf,
                              std::vector< int >& envUsed,
                              const std::vector< unsigned char >& len_,
                              const std::vector< unsigned int >& offs_,
                              RadixTree< unsigned short > *envTree =
                                  (RadixTree< unsigned short > *) 0)
{
  std::vector< unsigned short > tmpEnv(envBuf);
  std::vector< unsigned char >  len;
  std::vector< unsigned int >   offs;
  envBuf.clear();
  envUsed.clear();
  for (size_t i = 0; i < len_.size(); i = i + len_[i]) {
    if (!(offs_[i] & rleOffsetFlag)) {
      len.push_back(len_[i]);
      offs.push_back(offs_[i]);
    }
  }
  if (envTree) {
    // merge pairs of sequences where one is a
    // substring, prefix, or suffix of another
    envTree->clear();
    std::map< unsigned int, unsigned int >  tmp1;
    {
      std::map< unsigned int, unsigned int >  tmp2;
      for (size_t i = 0; i < offs.size(); i++) {
        std::map< unsigned int, unsigned int >::iterator  k =
            tmp2.find(offs[i]);
        if (k == tmp2.end())
          tmp2.insert(std::pair< unsigned int, unsigned int >(offs[i], len[i]));
        else if (len[i] > k->second)
          k->second = len[i];
      }
      for (std::map< unsigned int, unsigned int >::iterator i = tmp2.begin();
           i != tmp2.end(); i++) {
        unsigned int  d = i->first;
        unsigned int  l = i->second;
        tmp1.insert(std::pair< unsigned int, unsigned int >(
                        (unsigned int) envBuf.size(), l));
        envBuf.insert(envBuf.end(), tmpEnv.begin() + d, tmpEnv.begin() + d + l);
      }
    }
    for (std::map< unsigned int, unsigned int >::iterator i = tmp1.begin();
         i != tmp1.end(); ) {
      std::map< unsigned int, unsigned int >::iterator  k = i;
      i++;
      if (envTree->findString(&(envBuf.front()),
                              &(envBuf.front()) + k->first, k->second) < 0L) {
        for (size_t j = 0; j < k->second; j++)
          envTree->addString(&(envBuf.front()), k->first + j, k->second - j);
      }
      else {
        tmp1.erase(k);
      }
    }
    for (std::map< unsigned int, unsigned int >::iterator i = tmp1.begin();
         i != tmp1.end(); ) {
      std::map< unsigned int, unsigned int >::iterator  k = i;
      i++;
      if (envTree->findString(&(envBuf.front()),
                              &(envBuf.front()) + k->first, k->second)
          != long(k->first)) {
        tmp1.erase(k);
      }
    }
    envTree->clear();
    for (std::map< unsigned int, unsigned int >::iterator i = tmp1.begin();
         i != tmp1.end(); i++) {
      envTree->addString(&(envBuf.front()), i->first, i->second);
    }
    while (true) {
      unsigned int  bestOffs1 = 0U;
      unsigned int  bestOffs2 = 0U;
      size_t  bestOverlap = 0;
      size_t  bestOverlapOffs = 0;
      for (std::map< unsigned int, unsigned int >::iterator i = tmp1.begin();
           i != tmp1.end(); i++) {
        size_t  d = i->first;
        size_t  l = i->second;
        for (size_t j = 1; j < l && (l - j) >= bestOverlap; j++) {
          long    k = envTree->findString(&(envBuf.front()),
                                          &(envBuf.front()) + d + j, l - j);
          if (k < 0L || tmp1.find((unsigned int) k) == tmp1.end() ||
              size_t(k) == d) {
            continue;
          }
          size_t  n = l - j;
          if (n > bestOverlap ||
              (n == bestOverlap &&
               (l + tmp1[k]) < (tmp1[bestOffs1] + tmp1[bestOffs2]))) {
            bestOverlap = n;
            bestOverlapOffs = j;
            bestOffs1 = (unsigned int) d;
            bestOffs2 = (unsigned int) k;
          }
        }
      }
      if (bestOverlap < 1)
        break;
      unsigned int  newOffs = (unsigned int) envBuf.size();
      unsigned int  newLen = (unsigned int) bestOverlapOffs + tmp1[bestOffs2];
      for (size_t i = 0; i < bestOverlapOffs; i++)
        envBuf.push_back(envBuf[bestOffs1 + i]);
      for (size_t i = bestOverlapOffs; i < newLen; i++)
        envBuf.push_back(envBuf[bestOffs2 + i - bestOverlapOffs]);
      envTree->addString(&(envBuf.front()), newOffs, newLen);
      tmp1.erase(bestOffs1);
      tmp1.erase(bestOffs2);
      tmp1.insert(std::pair< unsigned int, unsigned int >(newOffs, newLen));
    }
    envTree->clear();
    {
      std::vector< unsigned short > tmpEnv2;
      for (std::map< unsigned int, unsigned int >::iterator i = tmp1.begin();
           i != tmp1.end(); i++) {
        for (size_t j = 0; j < i->second; j++)
          tmpEnv2.push_back(envBuf[i->first + j]);
      }
      envBuf.clear();
      for (size_t i = tmpEnv2.size(); i-- > 0; ) {
        size_t  l = tmpEnv2.size() - i;
        if (l > lengthMaxValue)
          l = lengthMaxValue;
        envTree->addString(&(tmpEnv2.front()), i, l);
      }
      for (size_t i = 0; i < offs.size(); i++) {
        long    k = envTree->findString(&(tmpEnv2.front()),
                                        &(tmpEnv.front()) + offs[i], len[i]);
        if (EP128EMU_UNLIKELY(k < 0L))
          errorMessage("internal error in optimizeEnvelopes()");
        offs[i] = (unsigned int) k;
      }
      tmpEnv = tmpEnv2;
    }
  }
  std::vector< int >  tmpEnvUsed(tmpEnv.size(), 0);
  for (size_t i = 0; i < len.size(); i++) {
    size_t  k = offs[i];
    for (size_t j = 0; j < len[i]; j++)
      tmpEnvUsed[k + j]++;
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

static void createEnvTables(
    std::vector< unsigned char >& outBuf, size_t loadAddr,
    std::map< unsigned int, unsigned char >& npMapF,
    std::map< unsigned int, unsigned char >& npMapV,
    const std::vector< unsigned short >& envBuf,
    const std::vector< unsigned char >& len,
    const std::vector< unsigned int >& offs)
{
  size_t  envOffs = (!(loadAddr & loadAddr1TrkFlag) ? 0x0310 : 0x0302);
  outBuf.clear();
  npMapF.clear();
  npMapV.clear();
  outBuf.resize(envBuf.size() * 2 + envOffs, 0);
  size_t  nFrames = len.size() >> 3;
  for (size_t i = 0; i < 2; i++) {
    std::map< unsigned int, int > envCnts;
    for (size_t j = (nFrames * 4) * i;
         j < ((nFrames * 4) * (i + 1)); j = j + len[j]) {
      unsigned int  key = (offs[j] << 8) | len[j];
      int     n =
          ((i == 0 && j >= (nFrames * 3) && (offs[j] & rleOffsetFlag) != 0) ?
           1 : 2);
      std::map< unsigned int, int >::iterator k = envCnts.find(key);
      if (k != envCnts.end())
        k->second = k->second + n;
      else
        envCnts.insert(std::pair< unsigned int, int >(key, n));
    }
    std::vector< unsigned long long > tmp;
    for (std::map< unsigned int, int >::iterator j = envCnts.begin();
         j != envCnts.end(); j++) {
      tmp.push_back(((unsigned long long) (0x7FFFFFFF - j->second) << 32)
                    | j->first);
    }
    std::sort(tmp.begin(), tmp.end());
    if (tmp.size() > 128)
      tmp.resize(128);
    for (size_t j = 0; j < tmp.size(); j++) {
      tmp[j] = (tmp[j] & 0xFFFFFFFFUL)
               | ((tmp[j] & 0xFFUL) << 48) | ((tmp[j] & 0x00FFFF00UL) << 24)
               | ((~tmp[j] & (rleOffsetFlag << 8)) << 32);
    }
    std::sort(tmp.begin(), tmp.end());
    for (unsigned char j = 0; j < 128; j++) {
      unsigned int  np = 0;
      unsigned char l = 0;
      if (j < tmp.size()) {
        np = (unsigned int) (tmp[j] & 0xFFFFFFFFUL);
        if (!i)
          npMapF.insert(std::pair< unsigned int, unsigned char >(np, j | 0x80));
        else
          npMapV.insert(std::pair< unsigned int, unsigned char >(np, j | 0x80));
        l = (unsigned char) (((np & 0xFF) - 1) << 1);
        np = np >> 8;
        if (!(np & rleOffsetFlag)) {
          np = np << (unsigned char) (!(loadAddr & loadAddrNIFlag));
          np = np + (loadAddr & 0xFF00) + envOffs;
          l = l | 0x80;
        }
        np = np & 0xFFFF;
      }
      outBuf[size_t(j) | (i << 7) | 0x0000] = l;
      outBuf[size_t(j) | (i << 7) | 0x0100] = (unsigned char) (np & 0xFF);
      outBuf[size_t(j) | (i << 7) | 0x0200] = (unsigned char) (np >> 8);
    }
  }
  if (!(loadAddr & loadAddrNIFlag)) {
    for (size_t i = 0; i < envBuf.size(); i++) {
      outBuf[envOffs + (i << 1)] = (unsigned char) (envBuf[i] & 0xFF);
      outBuf[envOffs + (i << 1) + 1] = (unsigned char) (envBuf[i] >> 8);
    }
  }
  else {
    for (size_t i = 0; i < envBuf.size(); i++) {
      outBuf[envOffs + i] = (unsigned char) (envBuf[i] & 0xFF);
      outBuf[envOffs + i + envBuf.size()] = (unsigned char) (envBuf[i] >> 8);
    }
  }
  if (loadAddr & loadAddrDiffFlag) {
    size_t  d = size_t(!(loadAddr & loadAddrNIFlag)) + 1;
    for (size_t i = envBuf.size() * 2; i-- > d; ) {
      outBuf[envOffs + i] =
          (outBuf[envOffs + i] - outBuf[envOffs + i - d]) & 0xFF;
    }
  }
}

// ----------------------------------------------------------------------------

static size_t optimizeTrackDataP1(
    std::vector< unsigned char >& len, std::vector< unsigned int >& offs,
    const std::vector< unsigned short >& inBuf,
    const std::vector< unsigned int >& offsTable)
{
  size_t  nFrames = inBuf.size() >> 3;
  std::vector< unsigned int > byteCnts(nFrames * 8 + 1, 0);
  size_t  rleLength = 1;
  for (size_t i = nFrames * 8; i-- > 0; ) {
    size_t  n = (nFrames * 8 - i) % nFrames;
    if (!n)
      n = nFrames;
    if (n > lengthMaxValue)
      n = lengthMaxValue;
    if (n < 2 || inBuf[i] != inBuf[i + 1])
      rleLength = 1;
    else if (rleLength < n)
      rleLength++;
    size_t  bestSize = 0x7FFFFFFF;
    size_t  bestLen = 1;
    size_t  bestOffs = rleOffsetFlag;
    for (size_t j = rleLength; j >= 1; j--) {
      size_t  nBytes = byteCnts[i + j] + 3
                       - size_t(i >= (nFrames * 3) && i < (nFrames * 4));
      if (nBytes < bestSize) {
        bestSize = nBytes;
        bestLen = j;
        bestOffs = rleOffsetFlag | inBuf[i];
      }
    }
    const unsigned int  *p = &(offsTable.front()) + offsTable[i];
    for (unsigned int j = 2; *p; p = p - 2) {
      size_t  k = *(p - 1);
      for ( ; j <= *p; j++) {
        size_t  nBytes = byteCnts[i + j] + 3;
        if (k >= i)
          nBytes = nBytes + (j * 2);
        else if ((k + j) > i)
          nBytes = nBytes + ((k + j - i) * 2);
        if (nBytes < bestSize) {
          bestSize = nBytes;
          bestLen = j;
          bestOffs = k;
        }
      }
    }
    byteCnts[i] = bestSize;
    len[i] = (unsigned char) bestLen;
    offs[i] = (unsigned int) bestOffs;
  }
  return (byteCnts[0] + 1);
}

static std::vector< unsigned char > envCostTable;

static size_t optimizeTrackData(
    std::vector< unsigned char >& len, std::vector< unsigned int >& offs,
    const std::vector< unsigned short >& inBuf,
    const std::vector< unsigned int >& offsTable,
    const std::vector< int >& envUsed,
    const std::map< unsigned int, unsigned char >& npMapF,
    const std::map< unsigned int, unsigned char >& npMapV)
{
  if (envCostTable.size() < 1) {
    envCostTable.resize((lengthMaxValue * lengthMaxValue * 2 + 1)
                        * lengthMaxValue);
    for (size_t i = 0; i <= (lengthMaxValue * lengthMaxValue * 2); i++) {
      for (size_t j = 1; j <= lengthMaxValue; j++) {
        unsigned int  tmp = (unsigned int) (j * 2);
        if (i > j)
          tmp = (unsigned int) j * (unsigned int) j * 2U / (unsigned int) i;
        envCostTable[i * lengthMaxValue + j - 1] = (unsigned char) tmp;
      }
    }
  }
  size_t  nFrames = inBuf.size() >> 3;
  std::vector< unsigned int > byteCnts(nFrames * 8 + 1, 0);
  std::vector< unsigned int > byteCntsNoEnv(nFrames * 8 + 1, 0);
  size_t  rleLength = 1;
  for (size_t i = nFrames * 8; i-- > 0; ) {
    const std::map< unsigned int, unsigned char >&  npMap =
        (i < (nFrames * 4) ? npMapF : npMapV);
    size_t  n = (nFrames * 8 - i) % nFrames;
    if (!n)
      n = nFrames;
    if (n > lengthMaxValue)
      n = lengthMaxValue;
    if (n < 2 || inBuf[i] != inBuf[i + 1])
      rleLength = 1;
    else if (rleLength < n)
      rleLength++;
    size_t  bestSize = 0x7FFFFFFF;
    size_t  bestSizeNoEnv = 0x7FFFFFFF;
    size_t  bestLen = 1;
    size_t  bestOffs = rleOffsetFlag;
    for (size_t j = rleLength; j >= 1; j--) {
      size_t  nBytes = byteCnts[i + j] + 1;
      if (nBytes >= bestSize)
        continue;
      if (npMap.find((unsigned int) (((rleOffsetFlag | inBuf[i]) << 8) | j))
          == npMap.end()) {
        nBytes = nBytes + 2 - size_t(i >= (nFrames * 3) && i < (nFrames * 4));
      }
      if (nBytes < bestSize) {
        bestSize = nBytes;
        bestSizeNoEnv = nBytes + byteCntsNoEnv[i + j] - byteCnts[i + j];
        bestLen = j;
        bestOffs = rleOffsetFlag | inBuf[i];
      }
    }
    const unsigned int  *p = &(offsTable.front()) + offsTable[i];
    for (unsigned int j = 2; *p; p = p - 2) {
      size_t  k = *(p - 1);
      for ( ; j <= *p; j++) {
        size_t  envBytes = 0;
        if (envUsed.size() > 0) {
          envBytes = size_t(envUsed[k + j] - envUsed[k]);
          if (envBytes > (lengthMaxValue * lengthMaxValue * 2))
            envBytes = 0;
          else
            envBytes = envCostTable[envBytes * lengthMaxValue + j - 1];
        }
        size_t  nBytes = envBytes + byteCnts[i + j] + 1;
        if (nBytes > bestSize)
          continue;
        if (npMap.find((unsigned int) ((k << 8) | j)) == npMap.end())
          nBytes = nBytes + 2;
        size_t  nBytesNoEnv =
            nBytes - (envBytes + byteCnts[i + j] - byteCntsNoEnv[i + j]);
        if (nBytes < bestSize ||
            (nBytes == bestSize && nBytesNoEnv > bestSizeNoEnv)) {
          bestSize = nBytes;
          bestSizeNoEnv = nBytesNoEnv;
          bestLen = j;
          bestOffs = k;
        }
      }
    }
    byteCnts[i] = bestSize;
    byteCntsNoEnv[i] = bestSizeNoEnv;
    len[i] = (unsigned char) bestLen;
    offs[i] = (unsigned int) bestOffs;
  }
  return (byteCntsNoEnv[0] + 1);
}

static void writeTrackData(
    std::vector< unsigned char >& outBuf, size_t loadAddr,
    unsigned char len, unsigned int offs, bool isChn3Freq,
    const std::map< unsigned int, unsigned char >& npMap)
{
  std::map< unsigned int, unsigned char >::const_iterator i =
      npMap.find((offs << 8) | len);
  if (i != npMap.end()) {
    outBuf.push_back(i->second);
  }
  else if (!(offs & rleOffsetFlag)) {
    outBuf.push_back((unsigned char) ((len - 1) | 0x40));
    size_t  tmp =
        (offs << (unsigned char) (!(loadAddr & loadAddrNIFlag)))
        + (loadAddr & 0xFF00)
        + (!(loadAddr & loadAddr1TrkFlag) ? 0x0310 : 0x0302);
    outBuf.push_back((unsigned char) (tmp & 0xFF));
    outBuf.push_back((unsigned char) (tmp >> 8));
  }
  else {
    outBuf.push_back((unsigned char) (len - 1));
    outBuf.push_back((unsigned char) (offs & 0xFF));
    if (!isChn3Freq)
      outBuf.push_back((unsigned char) (offs >> 8));
  }
}

static void writeTrackData(
    std::vector< unsigned char >& outBuf, size_t loadAddr,
    const std::vector< unsigned char >& len,
    const std::vector< unsigned int >& offs,
    const std::map< unsigned int, unsigned char >& npMapF,
    const std::map< unsigned int, unsigned char >& npMapV)
{
  size_t  nFrames = len.size() >> 3;
  if (!(loadAddr & loadAddr1TrkFlag)) {
    for (size_t i = 0; i < 8; i++) {
      size_t  trackOffs = (loadAddr & 0xFF00) + outBuf.size();
      outBuf[0x0300 + (i << 1)] = (unsigned char) (trackOffs & 0xFF);
      outBuf[0x0301 + (i << 1)] = (unsigned char) (trackOffs >> 8);
      for (size_t j = nFrames * i; j < (nFrames * (i + 1)); j = j + len[j]) {
        writeTrackData(outBuf, loadAddr, len[j], offs[j], (i == 3),
                       (i < 4 ? npMapF : npMapV));
      }
      if (i == 0)
        outBuf.push_back(0x40);
    }
  }
  else {
    size_t  envTimers[8];
    size_t  trackPtrs[8];
    for (size_t i = 0; i < 8; i++) {
      envTimers[i] = 0;
      trackPtrs[i] = nFrames * (((i & 1) << 2) | ((i & 6) >> 1));
    }
    {
      size_t  trackOffs = (loadAddr & 0xFF00) + outBuf.size();
      outBuf[0x0300] = (unsigned char) (trackOffs & 0xFF);
      outBuf[0x0301] = (unsigned char) (trackOffs >> 8);
    }
    for (size_t i = 0; true; i = ((i + 1) & 7)) {
      if (envTimers[i] > 1) {
        envTimers[i]--;
        continue;
      }
      size_t  j = trackPtrs[i];
      if ((i == 0 && j >= nFrames) || j >= len.size())
        break;
      writeTrackData(outBuf, loadAddr, len[j], offs[j], (i == 6),
                     (!(i & 1) ? npMapF : npMapV));
      envTimers[i] = len[j];
      trackPtrs[i] = j + len[j];
    }
    outBuf.push_back(0x40);
  }
}

static void convertFile(std::vector< unsigned char >& outBuf,
                        const std::vector< unsigned short >& inBuf,
                        size_t loadAddr, size_t maxLen1)
{
  size_t  nFrames = inBuf.size() >> 3;
  std::vector< unsigned short > envBuf(inBuf);
  std::vector< int >  envUsed;
  std::vector< unsigned char >  len(nFrames * 8, 0);
  std::vector< unsigned int >   offs(nFrames * 8);
  std::map< unsigned int, unsigned char > npMapF;
  std::map< unsigned int, unsigned char > npMapV;
  std::vector< unsigned int >   offsTable(nFrames * 8, 0U);
  RadixTree< unsigned short >   envTree;
  size_t  prvEnvSize = 0x7FFFFFFF;
  bool    finalPass = false;
  for (size_t passCnt = 1; true; passCnt++) {
    size_t  maxLen = passCnt * maxLen1;
    if (maxLen > lengthMaxValue || finalPass)
      maxLen = lengthMaxValue;
    if (passCnt > 1 && !finalPass)
      optimizeEnvelopes(envBuf, envUsed, len, offs);
    bool    finalPassNext = ((envBuf.size() << 1) >= prvEnvSize);
    if (finalPassNext)
      optimizeEnvelopes(envBuf, envUsed, len, offs, &envTree);
    while (true) {
      envTree.clear();
      for (size_t i = envBuf.size(); i-- > 0; ) {
        size_t  n = envBuf.size() - i;
        if (n > maxLen)
          n = maxLen;
        envTree.addString(&(envBuf.front()), i, n);
      }
      offsTable.resize(nFrames * 8);
      for (size_t i = 0; i < (nFrames * 8); i++) {
        size_t  n = (nFrames * 8 - i) % nFrames;
        if (!n)
          n = nFrames;
        if (n > maxLen)
          n = maxLen;
        unsigned int  tmpBuf[lengthMaxValue];
        size_t  l = envTree.findMatches(&(tmpBuf[0]), &(envBuf.front()),
                                        &(inBuf.front()) + i, n);
        unsigned int  prvOffs = 0xFFFFFFFFU;
        offsTable.push_back(0U);
        for ( ; l >= 2; l--) {
          unsigned int  d = tmpBuf[l - 1];
          if (d != prvOffs) {
            prvOffs = d;
            offsTable.push_back(d);
            offsTable.push_back((unsigned int) l);
          }
        }
        offsTable[i] = (unsigned int) (offsTable.size() - 1);
      }
      if (passCnt > 1 || len[0] != 0)
        break;
      (void) optimizeTrackDataP1(len, offs, inBuf, offsTable);
      optimizeEnvelopes(envBuf, envUsed, len, offs);
    }
    size_t  trkSize = 0x7FFFFFFF;
    if (finalPass)
      envUsed.clear();
    outBuf.clear();
    npMapF.clear();
    npMapV.clear();
    for (size_t k = 0; k < (!finalPass ? 3 : 8); k++) {
      if (k > 0)
        createEnvTables(outBuf, loadAddr, npMapF, npMapV, envBuf, len, offs);
      size_t  prvSize = trkSize;
      trkSize = optimizeTrackData(len, offs, inBuf, offsTable, envUsed,
                                  npMapF, npMapV);
      if (trkSize >= prvSize)
        break;
    }
    size_t  envSize = envBuf.size() << 1;
    size_t  totalSize =
        envSize + trkSize + (!(loadAddr & loadAddr1TrkFlag) ? 0x0310 : 0x0302);
    if (finalPass) {
      writeTrackData(outBuf, loadAddr, len, offs, npMapF, npMapV);
      totalSize = outBuf.size();
    }
    prvEnvSize = envSize;
    std::fprintf(stderr,
                 "\rPass %2d: file size:%6u bytes, "
                 "envelope data: %5u, track data: %5u %c",
                 int(passCnt), (unsigned int) totalSize,
                 (unsigned int) envSize, (unsigned int) trkSize,
                 (!finalPass ? ' ' : '\n'));
    if (finalPass)
      break;
    finalPass = finalPassNext;
  }
}

// ----------------------------------------------------------------------------

static void decodeFile(std::vector< unsigned char >& outBuf,
                       const std::vector< unsigned char >& inBuf,
                       size_t loadAddr)
{
  outBuf.clear();
  size_t  envOffs = (!(loadAddr & loadAddr1TrkFlag) ? 0x0310 : 0x0302);
  size_t  envSize = 0;
  if (inBuf.size() < (envOffs + 1))
    errorMessage("invalid data size");
  size_t  envTimers[8];
  size_t  envPtrs[8];
  size_t  trackPtrs[8];
  for (size_t i = 0; i < 8; i++) {
    envTimers[i] = 0;
    envPtrs[i] = 0;
    size_t  j = 0x0300;
    if (!(loadAddr & loadAddr1TrkFlag))
      j = j + (((i & 1) << 3) | (i & 6));
    trackPtrs[i] = (size_t(inBuf[j + 1]) << 8) | inBuf[j];
    if (trackPtrs[i] < ((loadAddr & 0xFF00) + envOffs + envSize))
      errorMessage("invalid track offset");
    trackPtrs[i] = trackPtrs[i] - (loadAddr & 0xFF00);
    if (i == 0) {
      envSize = trackPtrs[i] - envOffs;
      if ((envOffs + envSize) > inBuf.size())
        errorMessage("invalid track offset");
    }
  }
  std::vector< unsigned char >  envData;
  envData.insert(envData.end(),
                 inBuf.begin() + envOffs, inBuf.begin() + (envOffs + envSize));
  if (loadAddr & loadAddrDiffFlag) {
    size_t  d = size_t(!(loadAddr & loadAddrNIFlag)) + 1;
    for (size_t i = d; i < envData.size(); i++)
      envData[i] = (envData[i] + envData[i - d]) & 0xFF;
  }
  for (size_t i = 0; true; i = ((i + 1) & 7)) {
    if ((envTimers[i] & 0x3F) < 1) {
      size_t& p = trackPtrs[(!(loadAddr & loadAddr1TrkFlag) ? i : 0)];
      if (p >= inBuf.size())
        errorMessage("unexpected end of data");
      unsigned char c = inBuf[p];
      if (c == 0x40 && i == 0)
        break;
      p++;
      size_t  offs = 0;
      if (c & 0x80) {
        if (!(i & 1))
          c = c & 0x7F;
        offs = (size_t(inBuf[size_t(c) | 0x0200]) << 8)
               | inBuf[size_t(c) | 0x0100];
        c = inBuf[size_t(c)] >> 1;
      }
      else {
        if (p >= inBuf.size())
          errorMessage("unexpected end of data");
        offs = inBuf[p];
        p++;
        if (!(c < 0x40 && i == 6)) {
          if (p >= inBuf.size())
            errorMessage("unexpected end of data");
          offs = offs | (size_t(inBuf[p]) << 8);
          p++;
        }
      }
      if (c & 0x40) {
        if (offs < ((loadAddr & 0xFF00) + envOffs))
          errorMessage("invalid envelope data offset");
        offs = offs - ((loadAddr & 0xFF00) + envOffs);
      }
      envTimers[i] = c;
      envPtrs[i] = offs;
    }
    else {
      envTimers[i]--;
    }
    if (i == 0)
      outBuf.resize(outBuf.size() + 16, 0);
    unsigned char l = (unsigned char) (envPtrs[i] & 0xFF);
    unsigned char h = (unsigned char) (envPtrs[i] >> 8);
    if (envTimers[i] & 0x40) {
      if (!(loadAddr & loadAddrNIFlag)) {
        if ((envPtrs[i] + 1) >= envData.size())
          errorMessage("invalid envelope data offset");
        l = envData[envPtrs[i]];
        h = envData[envPtrs[i] + 1];
        envPtrs[i] = envPtrs[i] + 2;
      }
      else {
        if (envPtrs[i] >= (envData.size() >> 1))
          errorMessage("invalid envelope data offset");
        l = envData[envPtrs[i]];
        h = envData[envPtrs[i] + (envData.size() >> 1)];
        envPtrs[i] = envPtrs[i] + 1;
      }
    }
    if (!(i & 1)) {
      outBuf[outBuf.size() + i - 16] = l;
      if (i != 6)
        outBuf[outBuf.size() + i - 15] = h;
    }
    else {
      outBuf[outBuf.size() + (i >> 1) - 8] = l;
      outBuf[outBuf.size() + (i >> 1) - 4] = h;
    }
  }
}

// ----------------------------------------------------------------------------

static size_t loadInputFile(std::vector< unsigned char >& inBuf,
                            const char *fileName,
                            size_t minSize = 0, size_t maxSize = 0x7FFFFFFF)
{
  inBuf.clear();
  std::FILE *f = std::fopen(fileName, "rb");
  if (!f)
    errorMessage("error opening input file '%s'\n", fileName);
  int     c;
  while ((c = std::fgetc(f)) != EOF) {
    inBuf.push_back((unsigned char) (c & 0xFF));
    if (inBuf.size() > maxSize)
      break;
  }
  std::fclose(f);
  if (inBuf.size() < minSize || inBuf.size() > maxSize)
    errorMessage("invalid input file size");
  return inBuf.size();
}

static void writeOutputFile(const std::vector< unsigned char >& outBuf,
                            const char *fileName)
{
  std::FILE *f = std::fopen(fileName, "wb");
  if (!f)
    errorMessage("error opening output file '%s'", fileName);
  try {
    if (std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(),
                    f) != outBuf.size() ||
        std::fflush(f) != 0) {
      errorMessage("error writing output file '%s'", fileName);
    }
    std::fclose(f);
    f = (std::FILE *) 0;
  }
  catch (...) {
    std::fclose(f);
    throw;
  }
}

static size_t loadInputFile(std::vector< unsigned short >& inBuf,
                            const char *fileName, int zeroVolMode)
{
  inBuf.clear();
  std::vector< unsigned char >  tmpBuf;
  size_t  nFrames = loadInputFile(tmpBuf, fileName, 16, rleOffsetFlag * 2) >> 4;
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

static void parseCommandLine(int argc, char **argv,
                             size_t& loadAddr, size_t& maxLen1,
                             int& zeroVolMode)
{
  if (argc < 3 || argc > 6) {
    std::fprintf(stderr, "Usage: %s INFILE.DAV OUTFILE "
                         "[LOADADDR[:FLAGS] [MAXLEN1 [f|h|z]]]\n", argv[0]);
    std::fprintf(stderr,
                 "    LOADADDR    load address of output file "
                 "(default: 0x%04X)\n", (unsigned int) loadAddr);
    std::fprintf(stderr, "    LOADADDR:d  differential envelope format\n");
    std::fprintf(stderr, "    LOADADDR:n  non-interleaved envelope format\n");
    std::fprintf(stderr, "    LOADADDR:s  single track format\n");
    std::fprintf(stderr, "    LOADADDR:x  decode input file\n");
    std::fprintf(stderr,
                 "    MAXLEN1     initial maximum envelope length (2..64) "
                 "(default: %d),\n", int(maxLen1));
    std::fprintf(stderr,
                 "                0 or 1: search for the best value "
                 "up to 32 or 64 (slow)\n");
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
    if (!endp || endp == argv[3] || (*endp != '\0' && *endp != ':') ||
        (loadAddr & 0xFF00) != loadAddr) {
      errorMessage("invalid load address");
    }
    if (*endp == ':') {
      char    c = *(++endp);
      do {
        switch (c & char(0xDF)) {
        case 'D':
          loadAddr |= loadAddrDiffFlag;
          break;
        case 'N':
          loadAddr |= loadAddrNIFlag;
          break;
        case 'S':
          loadAddr |= loadAddr1TrkFlag;
          break;
        case 'X':
          loadAddr |= loadAddrDecodeFlag;
          break;
        default:
          errorMessage("invalid load address format flags");
        }
        c = *(++endp);
      } while (c != '\0');
    }
  }
  if (argc >= 5) {
    char    *endp = (char *) 0;
    maxLen1 = size_t(std::strtol(argv[4], &endp, 0));
    if (!endp || endp == argv[4] || *endp != '\0' ||
        !(long(maxLen1) >= 0L && maxLen1 <= 64)) {
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
  size_t  maxLen1 = 16;
  try {
    std::vector< unsigned short > inBuf;
    size_t  nFrames = 0;
    {
      int     zeroVolMode = 0;
      parseCommandLine(argc, argv, loadAddr, maxLen1, zeroVolMode);
      if (loadAddr & loadAddrDecodeFlag) {
        std::vector< unsigned char >  inBuf2;
        std::vector< unsigned char >  outBuf;
        loadInputFile(inBuf2, argv[1], 0x0303, 0xFFFF);
        decodeFile(outBuf, inBuf2, loadAddr);
        writeOutputFile(outBuf, argv[2]);
        return 0;
      }
      nFrames = loadInputFile(inBuf, argv[1], zeroVolMode);
    }
    std::vector< unsigned char >  outBuf;
    if (maxLen1 < 2) {
      size_t  bestMaxLen1 = 0;
      std::vector< unsigned char >  tmpBuf;
      for (maxLen1 = (!maxLen1 ? 32 : 64); maxLen1 >= 2; maxLen1--) {
        tmpBuf.clear();
        std::fprintf(stderr, "MAXLEN1 = %2d (best: %2d, file size: %5u):\n",
                     int(maxLen1), int(bestMaxLen1),
                     (unsigned int) outBuf.size());
        convertFile(tmpBuf, inBuf, loadAddr, maxLen1);
        if (tmpBuf.size() <= outBuf.size() || outBuf.size() < 1) {
          bestMaxLen1 = maxLen1;
          outBuf.clear();
          outBuf = tmpBuf;
        }
      }
    }
    else {
      convertFile(outBuf, inBuf, loadAddr, maxLen1);
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
    writeOutputFile(outBuf, argv[2]);
  }
  catch (std::exception& e) {
    std::fprintf(stderr, " *** %s: %s\n", argv[0], e.what());
    return -1;
  }
  return 0;
}

