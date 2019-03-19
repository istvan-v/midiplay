
// daveconv: converts raw DAVE register data to simple compressed format
// Copyright (C) 2018-2019 Istvan Varga <istvanv@users.sourceforge.net>
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
#include <string>
#include <vector>
#include <map>
#include <algorithm>

static size_t   envelopeBufSize = 0x1000;
static size_t   envelopeBufSamples = 0x0FFC;
static int      envelopeBufFrames = 0x07FE;
static size_t   trackOffsetsPos = 0x1800;
static size_t   trackDataPos = 0x1808;
static int      envelopeMaxDur = 31;
static bool     envNoInterleave = false;
static bool     disableNPTable2 = false;
static bool     enableTuneDataOpt2 = false;
static bool     disableMessages = false;

struct Envelopes {
  std::vector< std::vector< unsigned char > > envelopeData;
  std::vector< unsigned char >  currentEnvelope;
  int     prvL;
  int     prvR;
  int     threshold;
  int     maxDuration;
  size_t  envPos;
  // --------
  Envelopes(int threshold_, int maxDuration_);
  int findEnvelope();
  bool addSample(int l, int r, bool newNote = false);
  void optimizeEnvelopes();
  void reset();
};

Envelopes::Envelopes(int threshold_, int maxDuration_)
  : prvL(-1),
    prvR(-1),
    threshold(threshold_),
    maxDuration(maxDuration_),
    envPos(0)
{
}

int Envelopes::findEnvelope()
{
  for (size_t i = 0; i < currentEnvelope.size(); i++) {
    if (currentEnvelope[i] != 0)
      break;
    if ((i + 1) >= currentEnvelope.size())
      return envelopeBufFrames;
  }
  bool    foundMatch = false;
  for (size_t i = 0; i < envelopeData.size(); i++) {
    for (size_t offs = 0;
         (currentEnvelope.size() + offs) <= envelopeData[i].size();
         offs = offs + 2) {
      foundMatch = true;
      for (size_t j = 0; j < currentEnvelope.size(); j++) {
        if (currentEnvelope[j] != envelopeData[i][j + offs]) {
          foundMatch = false;
          break;
        }
      }
      if (foundMatch)
        return int(i | (offs << 16));
    }
  }
  return -1;
}

bool Envelopes::addSample(int l, int r, bool newNote)
{
  if (l == 0 && r == 0 && prvL == 0 && prvR == 0 &&
      currentEnvelope.size() >= size_t(maxDuration << 1)) {
    for (size_t i = 0; i < currentEnvelope.size(); i++) {
      if (currentEnvelope[i] != 0) {
        newNote = true;
        break;
      }
    }
    newNote |= (currentEnvelope.size() >= size_t(envelopeMaxDur << 1));
  }
  else {
    newNote |= (currentEnvelope.size() >= size_t(maxDuration << 1));
  }
  newNote |= ((l >= threshold || r >= threshold) &&
              prvL < threshold && prvR < threshold);
  prvL = l;
  prvR = r;
  if (newNote) {
    currentEnvelope.clear();
    envPos = 0;
  }
  int     n = -1;
  if (currentEnvelope.size() > 0) {
    n = findEnvelope();
    if (n == envelopeBufFrames && (l | r) != 0) {
      newNote = true;
      currentEnvelope.clear();
      envPos = 0;
      n = -1;
    }
  }
  currentEnvelope.push_back((unsigned char) l);
  currentEnvelope.push_back((unsigned char) r);
  if (findEnvelope() >= 0)
    return newNote;
  if (n >= 0 &&
      (currentEnvelope.size() + size_t(n >> 16))
      == (envelopeData[n & 0xFFFF].size() + 2)) {
    envelopeData[n & 0xFFFF].push_back((unsigned char) l);
    envelopeData[n & 0xFFFF].push_back((unsigned char) r);
  }
  else {
    envelopeData.push_back(currentEnvelope);
  }
  return newNote;
}

void Envelopes::optimizeEnvelopes()
{
  for (size_t i = 0; i < envelopeData.size(); ) {
    currentEnvelope.clear();
    currentEnvelope.insert(currentEnvelope.end(),
                           envelopeData[i].begin(), envelopeData[i].end());
    envelopeData[i].clear();
    if (findEnvelope() < 0) {
      envelopeData[i].insert(envelopeData[i].end(),
                             currentEnvelope.begin(), currentEnvelope.end());
      i++;
    }
    else {
      if ((i + 1) < envelopeData.size()) {
        envelopeData[i].insert(envelopeData[i].end(),
                               envelopeData[envelopeData.size() - 1].begin(),
                               envelopeData[envelopeData.size() - 1].end());
      }
      envelopeData.resize(envelopeData.size() - 1);
    }
  }
}

void Envelopes::reset()
{
  currentEnvelope.clear();
  prvL = -1;
  prvR = -1;
  envPos = 0;
}

struct Tune : public Envelopes {
  int     prvFreq;
  // --------
  Tune(int threshold_, int maxDuration_);
  void addSample(int l, int r, int freq);
  void processDaveData(const std::vector< unsigned char >& buf, int c);
};

Tune::Tune(int threshold_, int maxDuration_)
  : Envelopes(threshold_, maxDuration_),
    prvFreq(-1)
{
}

void Tune::addSample(int l, int r, int freq)
{
  bool    newNote = (prvFreq < 0);
  if (freq >= 0) {
    if (freq != prvFreq) {
      Envelopes::reset();
      newNote = true;
    }
    newNote = newNote | Envelopes::addSample(l, r, newNote);
  }
  else {
    newNote = true;
  }
  prvFreq = freq;
}

void Tune::processDaveData(const std::vector< unsigned char >& buf, int c)
{
  size_t  bufSize = buf.size() & ~(size_t(15));
  size_t  nBytes = bufSize;
  Envelopes::reset();
  prvFreq = -1;
  for (size_t i = 0; i < nBytes; i = i + 16) {
    int     l = buf[i + size_t(8 + c)];
    int     r = buf[i + size_t(12 + c)];
    int     freq = buf[i + size_t(c << 1)];
    if (c != 3) {
      freq |= (int(buf[i + size_t((c << 1) + 1)]) << 8);
      if (freq <= 1) {
        l = 0;
        r = 0;
        freq = 0;
      }
    }
    addSample(l, r, freq);
  }
  addSample(0, 0, -1);
}

// ----------------------------------------------------------------------------

struct EnvelopeTrie {
  std::vector< unsigned int > buf;
  size_t addFrame(size_t bufPos, unsigned short vol_lr, size_t envPos);
  int findEnvelope(const unsigned short *inBufPtr, size_t len) const;
  EnvelopeTrie(const unsigned char *envData, size_t envDataSize,
               size_t maxDur, size_t totalFrames);
  ~EnvelopeTrie();
};

size_t EnvelopeTrie::addFrame(size_t bufPos,
                              unsigned short vol_lr, size_t envPos)
{
  for (int i = 0; i < 7; i++) {
    bufPos = bufPos | (vol_lr & 0x03);
    vol_lr = vol_lr >> 2;
    if (bufPos < buf.size()) {
      if (buf[bufPos] != 0U) {
        bufPos = buf[bufPos];
        if (i == 6)
          bufPos = (bufPos & 0x0FFFFF) << 2;
        continue;
      }
    }
    else {
      size_t  newSize = (bufPos | 3) + 1;
      if (newSize > buf.capacity())
        buf.reserve(((newSize + (newSize >> 2)) | 0x0FFF) + 1);
      buf.resize(newSize, 0U);
    }
    if (i == 6)
      buf[bufPos] = (unsigned int) ((envPos << 20) | (buf.size() >> 2));
    else
      buf[bufPos] = (unsigned int) buf.size();
    bufPos = buf.size();
  }
  return bufPos;
}

int EnvelopeTrie::findEnvelope(const unsigned short *inBufPtr, size_t len) const
{
  if (buf.size() < 8)
    return -1;
  size_t  bufPos = 4;
  for (size_t i = 0; i < len; i++) {
    unsigned short  vol_lr = inBufPtr[i];
    bufPos = bufPos | (vol_lr & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 2) & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 4) & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 6) & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 8) & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 10) & 0x03);
    bufPos = buf[bufPos] | ((vol_lr >> 12) & 0x03);
    if (buf[bufPos] == 0U)
      return -1;
    if ((i + 1) >= len)
      return int(buf[bufPos] >> 20);
    bufPos = size_t(buf[bufPos] & 0x0FFFFFU) << 2;
  }
  return -1;
}

EnvelopeTrie::EnvelopeTrie(const unsigned char *envData, size_t envDataSize,
                           size_t maxDur, size_t totalFrames)
  : buf(4, 0U)
{
  for (size_t i = 0; i < (envDataSize >> 1); i++) {
    size_t  bufPos = 4;
    for (size_t j = 0; (j < maxDur) && ((i + j) < totalFrames); j++) {
      unsigned short  tmp = 0;
      if ((i + j) < (envDataSize >> 1)) {
        tmp = envData[(i + j) << 1]
              | ((unsigned short) envData[((i + j) << 1) + 1] << 8);
      }
      bufPos = addFrame(bufPos, tmp, i);
    }
  }
  if (buf.size() >= 0x400000) {
    std::fprintf(stderr,
                 " *** internal error: envelope trie buffer overflow\n");
    std::exit(-1);
  }
}

EnvelopeTrie::~EnvelopeTrie()
{
}

// ----------------------------------------------------------------------------

static size_t convertNoteParams(unsigned int& d, unsigned int& n,
                                unsigned int& frq, const int *tuneData)
{
  unsigned int  tmp = 0;
  size_t  envSize = 0;
  d = (unsigned int) (tuneData[0] & 0xFF);
  n = (unsigned int) (tuneData[1] & 0x0FFF);
  frq = (unsigned int) (tuneData[2] & (tuneData[2] >= 0 ? 0xFFFF : 0x00FF));
  switch (envelopeBufSize) {
  case 0x0200:
    n = n & 0x00FF;
    break;
  case 0x0400:
    d = d & 0x7F;
    n = n & 0x01FF;
    tmp = (n & 0x0100) >> 1;
    break;
  case 0x0800:
    d = d & 0x3F;
    n = n & 0x03FF;
    tmp = (n & 0x0300) >> 2;
    break;
  case 0x1000:
    d = d & 0x1F;
    n = n & 0x07FF;
    tmp = (n & 0x0700) >> 3;
    break;
  case 0x2000:
    d = d & 0x0F;
    n = n & 0x0FFF;
    tmp = (n & 0x0F00) >> 4;
    break;
  }
  if (n != (unsigned int) envelopeBufFrames)
    envSize = (n + d) << 1;
  d = d | tmp;
  n = n & 0xFF;
  return envSize;
}

static unsigned int noteParamsToKey(const int *tuneData)
{
  unsigned int  d, n, frq;
  (void) convertNoteParams(d, n, frq, tuneData);
  return ((d << 24) | (n << 16) | frq);
}

static size_t optimizeTuneData(std::vector< int >& outBuf,
                               const std::vector< unsigned char >& inBuf,
                               const EnvelopeTrie& envelopeData, int chn,
                               const std::map< unsigned int, int > *npMap = 0)
{
  unsigned short  tmpBufVol[256];
  unsigned short  tmpBufFrq[256];
  size_t  nFrames = inBuf.size() >> 4;
  std::vector< size_t > tuneDataSize(nFrames + 1, 0);
  std::vector< size_t > duration(nFrames, 0);
  std::vector< int >    env(nFrames, 0);
  for (size_t i = nFrames; i-- > 0; ) {
    for (size_t j = 0; j < size_t(envelopeMaxDur) && (i + j) < nFrames; j++) {
      const unsigned char *p = &(inBuf.front()) + ((i + j) << 4);
      tmpBufVol[j] = p[chn + 8] | ((unsigned short) p[chn + 12] << 8);
      tmpBufFrq[j] = p[chn << 1] | ((unsigned short) p[(chn << 1) + 1] << 8);
    }
    size_t  bestSize = 0x00FFFFFF;
    size_t  bestSize2 = 0x0FFFFFFF;
    size_t  bestLen = 0;
    int     bestEnv = 0;
    for (size_t d = 1; d <= size_t(envelopeMaxDur) && (i + d) <= nFrames; d++) {
      int     n = envelopeBufFrames;
      int     freq = 0;
      for (size_t j = 0; j < d; j++) {
        if (tmpBufVol[j] != 0) {
          n = 0;
          freq = int(tmpBufFrq[j]);
          while (++j < d) {
            if (tmpBufVol[j] != 0 && int(tmpBufFrq[j]) != freq) {
              n = -1;
              break;
            }
          }
          break;
        }
      }
      if (!n)
        n = envelopeData.findEnvelope(tmpBufVol, d);
      if (n < 0)
        break;
      size_t  nBytes = 2;
      if (npMap) {
        int     tmp[3];
        tmp[0] = int(d);
        tmp[1] = n;
        tmp[2] = (chn < 3 ? freq : (freq - 256));
        unsigned int  key = noteParamsToKey(&(tmp[0]));
        std::map< unsigned int, int >::const_iterator p = npMap->find(key);
        size_t  nBytes_ = 0x05FFFFFF;
        if (p != npMap->end())
          nBytes_ = size_t(p->second);
        nBytes = nBytes_ >> 24;
        if (chn == 3 && nBytes == 5)
          nBytes--;
        nBytes = nBytes + tuneDataSize[i + d];
        if (nBytes > bestSize || (nBytes == bestSize && nBytes_ > bestSize2))
          continue;
        bestSize2 = nBytes_;
      }
      else {
        if (n != envelopeBufFrames)
          nBytes = (chn < 3 ? 4 : 3);
        nBytes = nBytes + tuneDataSize[i + d];
      }
      if (nBytes <= bestSize) {
        bestSize = nBytes;
        bestLen = d;
        bestEnv = n;
      }
    }
    tuneDataSize[i] = bestSize;
    duration[i] = bestLen;
    env[i] = bestEnv;
  }
  for (size_t i = 0; i < nFrames; ) {
    size_t  d = duration[i];
    int     n = env[i];
    int     freq = 0;
    if (d < 1)
      return 0;
    if (n != envelopeBufFrames) {
      for (size_t j = 0; j < d; j++) {
        const unsigned char *p = &(inBuf.front()) + ((i + j) << 4);
        if ((p[chn + 8] | p[chn + 12]) != 0) {
          freq = int(p[chn << 1]) | (int(p[(chn << 1) + 1]) << 8);
          break;
        }
      }
      if (chn >= 3)
        freq = freq - 256;
    }
    outBuf.push_back(int(d));
    outBuf.push_back(n);
    outBuf.push_back(freq);
    i = i + d;
  }
  outBuf.push_back(0);
  outBuf.push_back(0);
  outBuf.push_back(0);
  return (tuneDataSize[0] + 1);
}

static size_t optimizeTuneData2(std::vector< int >& outBuf,
                                const std::vector< unsigned char >& inBuf,
                                const unsigned char *envelopeData)
{
  outBuf.clear();
  EnvelopeTrie  tmpEnv(envelopeData, envelopeBufSamples,
                       size_t(envelopeMaxDur),
                       size_t(envelopeBufFrames + envelopeMaxDur));
  for (int c = 0; c < 4; c++) {
    if (!optimizeTuneData(outBuf, inBuf, tmpEnv, c))
      return 0;
  }
  if (enableTuneDataOpt2) {
    // run second pass of optimization with known note parameter sizes
    std::map< unsigned int, int > noteParamMap;
    for (size_t i = 0; i < outBuf.size(); i = i + 3) {
      unsigned int  key = noteParamsToKey(&(outBuf.front()) + i);
      std::map< unsigned int, int >::iterator p = noteParamMap.find(key);
      if (p == noteParamMap.end())
        noteParamMap.insert(std::pair< unsigned int, int >(key, -1));
      else
        p->second--;
    }
    outBuf.clear();
    {
      std::vector< long long >  noteParamTable;
      for (std::map< unsigned int, int >::iterator i = noteParamMap.begin();
           i != noteParamMap.end(); i++) {
        noteParamTable.push_back((long long) i->first
                                 | ((long long) i->second << 32));
      }
      std::stable_sort(noteParamTable.begin(), noteParamTable.end());
      size_t  maxSize = (disableNPTable2 ? 255 : 510);
      for (size_t i = 0; i < noteParamTable.size(); i++) {
        noteParamMap[size_t(noteParamTable[i] & (long long) 0xFFFFFFFFU)] =
            int(i | (i < (maxSize & 0xFF) ?
                     0x01000000 : (i < maxSize ? 0x02000000 : 0x05000000)));
      }
    }
    for (int c = 0; c < 4; c++) {
      if (!optimizeTuneData(outBuf, inBuf, tmpEnv, c, &noteParamMap))
        return 0;
    }
  }
  return (outBuf.size() / 3);
}

static size_t compressTuneData(std::vector< unsigned char >& outBuf,
                               const std::vector< int >& tuneData,
                               unsigned int tuneLoadAddress,
                               bool tuneDataBeforeEnv, bool envOffs8)
{
  std::map< unsigned int, int > noteParamMap;
  size_t  envSize = 0;
  for (size_t i = 0; i < tuneData.size(); i = i + 3) {
    unsigned int  d, n, frq;
    size_t  tmp = convertNoteParams(d, n, frq, &(tuneData.front()) + i);
    if (tmp > envSize)
      envSize = tmp;
    unsigned int  key = (d << 24) | (n << 16) | frq;
    std::map< unsigned int, int >::iterator i_ = noteParamMap.find(key);
    if (i_ == noteParamMap.end())
      noteParamMap.insert(std::pair< unsigned int, int >(key, -1));
    else
      i_->second--;
  }
  std::vector< long long >  noteParamTable;
  for (std::map< unsigned int, int >::iterator i = noteParamMap.begin();
       i != noteParamMap.end(); i++) {
    noteParamTable.push_back((long long) i->first
                             | ((long long) i->second << 32));
    i->second = -1;
  }
  std::stable_sort(noteParamTable.begin(), noteParamTable.end());
  {
    size_t  maxSize = (disableNPTable2 ? 255 : 510);
    size_t  maxSize1 = maxSize & 0xFF;
    if (noteParamTable.size() > maxSize)
      noteParamTable.resize(maxSize);
    maxSize = noteParamTable.size();
    maxSize1 = (maxSize1 < maxSize ? maxSize1 : maxSize);
    for (size_t i = 0; i < maxSize; i++)
      noteParamTable[i] = noteParamTable[i] & (long long) 0xFFFFFFFFU;
    std::stable_sort(noteParamTable.begin(), noteParamTable.begin() + maxSize1);
    if (maxSize1 < maxSize)
      std::stable_sort(noteParamTable.begin() + maxSize1, noteParamTable.end());
  }
  for (size_t i = 0; i < noteParamTable.size(); i++) {
    unsigned int  key = (unsigned int) noteParamTable[i];
    size_t  offs = envelopeBufSize;
    if (disableNPTable2)
      offs = offs + (i + 1);
    else
      offs = offs + (i < 254 ? 0x0000 : 0x0400) + ((i + 2) & 0xFF);
    outBuf[offs + 0x0000] = (unsigned char) ((key >> 24) & 0xFFU);  // d, env H
    outBuf[offs + 0x0100] = (unsigned char) ((key >> 16) & 0xFFU);  // env L
    outBuf[offs + 0x0200] = (unsigned char) (key & 0xFFU);          // freq L
    outBuf[offs + 0x0300] = (unsigned char) ((key >> 8) & 0xFFU);   // freq H
    noteParamMap[key] = int(i);
  }
  size_t  chn = 0;
  bool    newChannel = true;
  for (size_t i = 0; i < tuneData.size(); i = i + 3) {
    if (newChannel) {
      newChannel = false;
      size_t  offs = (outBuf.size() + tuneLoadAddress) & 0xFFFF;
      outBuf[trackOffsetsPos + (chn << 1)] = (unsigned char) (offs & 0xFF);
      outBuf[trackOffsetsPos + 1 + (chn << 1)] = (unsigned char) (offs >> 8);
    }
    unsigned int  d, n, frq;
    (void) convertNoteParams(d, n, frq, &(tuneData.front()) + i);
    unsigned int  key = (d << 24) | (n << 16) | frq;
    int     c = noteParamMap[key];
    if (c < 0 || c >= int(noteParamTable.size())) {
      outBuf.push_back(0x00);
      outBuf.push_back((unsigned char) d);
      outBuf.push_back((unsigned char) n);
      outBuf.push_back((unsigned char) (frq & 0xFF));
      if (chn != 3)
        outBuf.push_back((unsigned char) (frq >> 8));
    }
    else if (disableNPTable2) {
      outBuf.push_back((unsigned char) (c + 1));
    }
    else {
      if (c >= 254)
        outBuf.push_back(0x01);
      outBuf.push_back((unsigned char) ((c + 2) & 0xFF));
    }
    if (!d) {
      newChannel = true;
      chn++;
    }
  }
  outBuf[envelopeBufSize - 2] = 0x80;
  outBuf[envelopeBufSize - 1] = 0x80;
  size_t  envDataEnd = envSize;
  size_t  envBufEnd = envelopeBufSamples;
  if (envNoInterleave) {
    envDataEnd = (envDataEnd + envelopeBufSize) >> 1;
    envBufEnd = (envBufEnd + envelopeBufSize) >> 1;
    std::vector< unsigned char >  tmpBuf(envelopeBufSize, 0x00);
    for (size_t i = 0; i < envelopeBufSize; i = i + 2) {
      tmpBuf[i >> 1] = outBuf[i];
      tmpBuf[(i + envelopeBufSize) >> 1] = outBuf[i + 1];
    }
    for (size_t i = 0; i < envelopeBufSize; i++)
      outBuf[i] = tmpBuf[i];
  }
  size_t  trackDataSize = outBuf.size() - trackDataPos;
  if (!disableMessages)
    std::printf("Track 1: data size = %5d bytes\n", int(trackDataSize));
  if ((envDataEnd + trackDataSize) <= envBufEnd) {
    // if possible, relocate data to free space in envelope buffer
    for (size_t i = trackDataPos; i < outBuf.size(); i++)
      outBuf[i + envDataEnd - trackDataPos] = outBuf[i];
    outBuf.resize(trackDataPos);
    size_t  offs = trackDataPos - envDataEnd;
    for (size_t i = 0; i < 4; i++) {
      size_t  addr = size_t(outBuf[trackOffsetsPos + (i << 1)])
                     + (size_t(outBuf[trackOffsetsPos + 1 + (i << 1)]) << 8);
      addr = addr - offs;
      outBuf[trackOffsetsPos + (i << 1)] = (unsigned char) (addr & 0xFF);
      outBuf[trackOffsetsPos + 1 + (i << 1)] = (unsigned char) (addr >> 8);
    }
    if (!disableMessages) {
      std::printf("Memory used:         0x%04X-0x%04X\n",
                  tuneLoadAddress + (envOffs8 ? 8U : 0U),
                  (tuneLoadAddress + (unsigned int) trackOffsetsPos + 7U)
                  & 0xFFFFU);
    }
    if (envOffs8)
      outBuf.erase(outBuf.begin(), outBuf.begin() + 8);
  }
  else if (tuneDataBeforeEnv) {
    size_t  offs = outBuf.size();
    if (!disableMessages) {
      std::printf("Load address relocated to 0x%04X\n",
                  (unsigned int) (tuneLoadAddress + trackDataPos - offs)
                  & 0xFFFFU);
    }
    for (size_t i = 0; i < 4; i++) {
      size_t  addr = size_t(outBuf[trackOffsetsPos + (i << 1)])
                     + (size_t(outBuf[trackOffsetsPos + 1 + (i << 1)]) << 8);
      addr = (addr - offs) & 0xFFFF;
      outBuf[trackOffsetsPos + (i << 1)] = (unsigned char) (addr & 0xFF);
      outBuf[trackOffsetsPos + 1 + (i << 1)] = (unsigned char) (addr >> 8);
    }
    std::vector< unsigned char >  tmpBuf;
    tmpBuf.insert(tmpBuf.end(), outBuf.begin(), outBuf.end());
    outBuf.clear();
    outBuf.insert(outBuf.end(), tmpBuf.begin() + trackDataPos, tmpBuf.end());
    outBuf.insert(outBuf.end(), tmpBuf.begin(), tmpBuf.begin() + trackDataPos);
    if (!disableMessages) {
      std::printf("Memory used:         0x%04X-0x%04X\n",
                  (unsigned int) (tuneLoadAddress + trackDataPos - offs)
                  & 0xFFFFU,
                  (tuneLoadAddress + (unsigned int) trackOffsetsPos + 7U)
                  & 0xFFFFU);
    }
  }
  else {
    if (!disableMessages) {
      std::printf("Memory used:         0x%04X-0x%04X\n",
                  tuneLoadAddress + (envOffs8 ? 8U : 0U),
                  (unsigned int) (tuneLoadAddress + outBuf.size() - 1)
                  & 0xFFFFU);
    }
    if (envOffs8)
      outBuf.erase(outBuf.begin(), outBuf.begin() + 8);
  }
  // return total data size actually in use without empty areas
  return (envSize + (noteParamTable.size() << 2) + 10 + trackDataSize);
}

// ----------------------------------------------------------------------------

static void loadInputFile(std::vector< unsigned char >& inBuf,
                          const char *fileName, int zeroVolMode)
{
  inBuf.clear();
  {
    std::FILE *f = std::fopen(fileName, "rb");
    if (!f) {
      std::fprintf(stderr, " *** error opening input file '%s'\n", fileName);
      std::exit(-1);
    }
    int     c;
    while ((c = std::fgetc(f)) != EOF)
      inBuf.push_back((unsigned char) (c & 0xFF));
    std::fclose(f);
  }
  inBuf.resize(inBuf.size() & ~(size_t(15)));
  for (size_t i = 0; i < inBuf.size(); i = i + 16) {
    inBuf[i + 7] = 0x00;
    for (size_t j = 8; j < 16; j++)
      inBuf[i + j] = inBuf[i + j] & 0x3F;
    for (int j = 0; j < 4; j++) {
      unsigned char *vol_l = &(inBuf.front()) + (i + 8);
      unsigned char *vol_r = &(inBuf.front()) + (i + 12);
      unsigned char *freq = &(inBuf.front()) + i;
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
    for (size_t i = 0; i < inBuf.size(); i = i + 16) {
      unsigned char vol_l = inBuf[i + 8 + c];
      unsigned char vol_r = inBuf[i + 12 + c];
      int     freq = int(inBuf[i + (c << 1)]);
      if (c < 3) {
        freq = freq | (int(inBuf[i + (c << 1) + 1]) << 8);
        if (freq <= 1) {
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
      inBuf[i + 8 + c] = vol_l;
      inBuf[i + 12 + c] = vol_r;
      inBuf[i + (c << 1)] = (unsigned char) (freq & 0xFF);
      inBuf[i + (c << 1) + 1] = (unsigned char) (freq >> 8);
    }
  }
}

static int convertFile(std::vector< unsigned char >& outBuf,
                       const std::vector< unsigned char >& inBuf,
                       int envThreshold, int maxDur,
                       long tuneLoadAddress, bool envOffs8,
                       unsigned char *prvOutBuf = 0)
{
  outBuf.clear();
  if (maxDur < 1 || maxDur > envelopeMaxDur)
    maxDur = envelopeMaxDur;
  if (tuneLoadAddress >= 0x010000L)
    tuneLoadAddress = 0x010000L - long(trackDataPos);
  bool    tuneDataBeforeEnv = (tuneLoadAddress < 0L);
  if (tuneDataBeforeEnv)
    tuneLoadAddress = -tuneLoadAddress;
  tuneLoadAddress = tuneLoadAddress & 0xFFFFL;

  Tune    tune(envThreshold, maxDur);
  for (int c = 0; c < 4; c++)
    tune.processDaveData(inBuf, c);
  tune.optimizeEnvelopes();
  outBuf.resize(trackDataPos, 0x00);
  std::vector< size_t > envOffsTable(tune.envelopeData.size(), 0);
  size_t  n = (envOffs8 ? (envNoInterleave ? 16 : 8) : 0);
  for (size_t i = 0; i < n; i++)
    outBuf[i] = 0xFF;
  for (size_t i = 0; i < tune.envelopeData.size(); i++) {
    envOffsTable[i] = n >> 1;
    for (size_t j = 0; j < tune.envelopeData[i].size(); j++) {
      if (n < envelopeBufSamples) {
        outBuf[n] = (unsigned char) tune.envelopeData[i][j];
        n++;
      }
      else if (n > envelopeBufSamples || tune.envelopeData[i][j] != 0) {
        n++;
      }
    }
    n = (n + 1) & ~(size_t(1));
  }
  if (n > envelopeBufSamples) {
    if (!disableMessages) {
      std::fprintf(stderr, " *** envelope data size overflow (%d bytes)\n",
                   int(n));
    }
    return -1;
  }
  if (!disableMessages) {
    std::printf("Envelope data size = %4d / %d bytes\n",
                int(n), int(envelopeBufSamples));
  }
  if (prvOutBuf) {
    if (std::memcmp(&(outBuf.front()), prvOutBuf, envelopeBufSize) == 0)
      return -1;
    std::memcpy(prvOutBuf, &(outBuf.front()), envelopeBufSize);
  }

  std::vector< int >    tuneData;
  if (!optimizeTuneData2(tuneData, inBuf, &(outBuf.front()))) {
    if (!disableMessages)
      std::fprintf(stderr, " *** internal error: envelope not found\n");
    return -1;
  }
  for (size_t i = 0; i < envelopeBufSize; i++)
    outBuf[i] = outBuf[i] & 0x3F;
  return int(compressTuneData(outBuf, tuneData,
                              (unsigned int) tuneLoadAddress,
                              tuneDataBeforeEnv, envOffs8));
}

static void setEnvBufSize(size_t n)
{
  envelopeBufSize = n;
  envelopeBufSamples = envelopeBufSize - 4;
  envelopeBufFrames = int(envelopeBufSamples >> 1);
  trackOffsetsPos = envelopeBufSize + (disableNPTable2 ? 0x0400 : 0x0800);
  trackDataPos = trackOffsetsPos + 8;
  envelopeMaxDur = 131072 / int(envelopeBufSize) - 1;
}

static void printUsage(const char *prgName, const char *errMsg = (char *) 0)
{
  std::fprintf(
      stderr,
      "Usage: %s OUTFILE INFILE[:[OPTIONS][,THRESHOLD[,MAXDUR]]] [LOADADDR]\n"
      "Options (comma separated list):\n"
      "    r:  reserve 8 bytes at the beginning of the envelope buffer\n"
      "    h:  hold previous frequency if volume is zero\n"
      "    z:  zero frequency if volume is zero\n"
      "    e0..e4: envelope buffer size (2^(N+9) bytes, default: e3 (4096))\n"
      "    n:  non-interleaved envelope data\n"
      "    d:  disable second note parameter table (1024 bytes)\n"
      "    s:  find the threshold and max. duration for minimum data size\n"
      "    f:  find the threshold and max. duration for minimum file size\n",
      prgName);
  if (errMsg)
    std::fprintf(stderr, " *** %s\n", errMsg);
}

int main(int argc, char **argv)
{
  if (argc != 3 && argc != 4) {
    printUsage(argv[0]);
    return -1;
  }
  setEnvBufSize(0x1000);
  std::vector< unsigned char >  inBuf;
  int     threshold = 0x30;
  int     maxDur = 0;
  long    tuneLoadAddress = 0x7FFFFFFFL;
  int     zeroVolMode = 0;
  bool    envOffs8 = false;
  char    optimizeParams = 0;
  bool    haveThreshold = false;
  bool    haveMaxDur = false;
  {
    std::string fileName(argv[2]);
    size_t n = fileName.rfind(':');
    if (n != std::string::npos && n < fileName.length()) {
      const char  *s = fileName.c_str() + (n + 1);
      while (true) {
        char    c = *s;
        if (c == '\0')
          break;
        if (c >= 'A' && c <= 'Z')
          c = c + ('a' - 'A');
        s++;
        if (c >= '0' && c <= '9') {
          s--;
          if (!haveThreshold) {
            haveThreshold = true;
            char    *endp = (char *) 0;
            long    tmp = std::strtol(s, &endp, 0);
            if (!endp || endp == s || (*endp != '\0' && *endp != ',') ||
                !(tmp >= 1L && tmp <= 64L)) {
              std::fprintf(stderr, " *** invalid threshold in '%s'\n",
                           fileName.c_str());
              return -1;
            }
            threshold = int(tmp);
            s = endp;
          }
          else if (!haveMaxDur) {
            haveMaxDur = true;
            char    *endp = (char *) 0;
            long    tmp = std::strtol(s, &endp, 0);
            if (!endp || endp == s || *endp != '\0' ||
                !(tmp >= 1L && tmp <= long(envelopeMaxDur))) {
              std::fprintf(stderr, " *** invalid maximum duration in '%s'\n",
                           fileName.c_str());
              return -1;
            }
            maxDur = int(tmp);
            s = endp;
          }
          else {
            printUsage(argv[0], "invalid option syntax");
            return -1;
          }
        }
        else if (c == 'r') {
          envOffs8 = true;
        }
        else if (c == 'h') {
          zeroVolMode = 2;
        }
        else if (c == 'z') {
          zeroVolMode = 1;
        }
        else if (c == 'e') {
          if (!(*s >= '0' && *s <= '4')) {
            printUsage(argv[0], "invalid envelope buffer size");
            return -1;
          }
          setEnvBufSize(size_t(0x4000 >> ('5' - *s)));
          s++;
        }
        else if (c == 'n') {
          envNoInterleave = true;
        }
        else if (c == 'd') {
          disableNPTable2 = true;
          setEnvBufSize(envelopeBufSize);
        }
        else if (c == 's') {
          optimizeParams = 1;
        }
        else if (c == 'f') {
          optimizeParams = 2;
        }
        else {
          printUsage(argv[0], "invalid option syntax");
          return -1;
        }
        if (*s == ',' && s[1] != '\0')
          s++;
      }
      fileName.resize(n);
    }
    if (argc > 3) {
      const char  *s = argv[3];
      char    *endp = (char *) 0;
      long    tmp = std::strtol(s, &endp, 0);
      if (!endp || endp == s || *endp != '\0' ||
          !(tmp >= -65535L && tmp <= 65535L)) {
        std::fprintf(stderr, " *** invalid load address: '%s'\n", s);
        return -1;
      }
      tuneLoadAddress = tmp;
    }
    loadInputFile(inBuf, fileName.c_str(), zeroVolMode);
  }
  if (maxDur <= 0 || maxDur > envelopeMaxDur)
    maxDur = envelopeMaxDur;

  std::vector< unsigned char >  outBuf;

  if (optimizeParams) {
    std::vector< unsigned char >  prvOutBuf(envelopeBufSize, 0xFF);
    disableMessages = true;
    size_t  bestDataSize = 0x7FFFFFFF;
    size_t  bestFileSize = 0x7FFFFFFF;
    int     bestThreshold = threshold;
    int     bestMaxDur = maxDur;
    bool    thrSearch = true;
    bool    foundBetter = true;
    bool    finalPass = false;
    do {
      if (!foundBetter) {
        finalPass = true;
        enableTuneDataOpt2 = true;
        std::memset(&(prvOutBuf.front()), 0xFF, envelopeBufSize);
      }
      else {
        foundBetter = false;
        if (thrSearch)
          threshold = 1;
        else
          maxDur = 1;
      }
      while (true) {
        static const char hbChrBuf[16] = {
          ' ', ' ', ' ', '\0', '.', ' ', ' ', '\0',
          '.', '.', ' ', '\0', '.', '.', '.', '\0'
        };
        std::fprintf(stderr, "\rOptimizing%s ",
                     &(hbChrBuf[0]) + (((threshold + maxDur) & 3) << 2));
        int     tmp = convertFile(outBuf, inBuf, threshold, maxDur,
                                  tuneLoadAddress, envOffs8,
                                  &(prvOutBuf.front()));
        if (tmp > 0 &&
            ((optimizeParams == 1 &&
              (size_t(tmp) < bestDataSize ||
               (size_t(tmp) == bestDataSize &&
                outBuf.size() < bestFileSize))) ||
             (optimizeParams == 2 &&
              (outBuf.size() < bestFileSize ||
               (outBuf.size() == bestFileSize &&
                size_t(tmp) < bestDataSize))))) {
          foundBetter = true;
          bestDataSize = size_t(tmp);
          bestFileSize = outBuf.size();
          bestThreshold = threshold;
          bestMaxDur = maxDur;
        }
        if (bestDataSize != 0x7FFFFFFF) {
          std::fprintf(stderr,
                       "EnvThr: %2d, MaxDur:%3d, "
                       "Data Size: %5d, File Size: %5d",
                       bestThreshold, bestMaxDur,
                       int(bestDataSize), int(bestFileSize));
        }
        if (finalPass)
          break;
        if (thrSearch) {
          if (++threshold > 64) {
            threshold = bestThreshold;
            break;
          }
        }
        else {
          if (++maxDur > envelopeMaxDur) {
            maxDur = bestMaxDur;
            break;
          }
        }
      }
      thrSearch = !thrSearch;
    } while (!finalPass);
    std::fprintf(stderr, "\n");
  }

  enableTuneDataOpt2 = true;
  disableMessages = false;
  if (convertFile(outBuf, inBuf, threshold, maxDur,
                  tuneLoadAddress, envOffs8) < 0) {
    return -1;
  }

  std::FILE *f = std::fopen(argv[1], "wb");
  if (!f) {
    std::fprintf(stderr, " *** error opening output file '%s'\n", argv[1]);
    return -1;
  }
  if (std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(), f)
      != outBuf.size() ||
      std::fflush(f) != 0) {
    std::fclose(f);
    std::remove(argv[1]);
    std::fprintf(stderr, " *** error writing output file '%s'\n", argv[1]);
    return -1;
  }
  std::fclose(f);
  return 0;
}

