
// midiconv: converts MIDI files to Enterprise midiplay format
// Copyright (C) 2017-2018 Istvan Varga <istvanv@users.sourceforge.net>
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
#include <exception>
#include <stdexcept>

#include "comprlib.cpp"
#include "compress2.cpp"
#include "daveplay.cpp"

struct MIDIEvent {
  static bool optimizeNoteEvents;
  // --------
  unsigned long t;
  unsigned char st;
  unsigned char d1;
  unsigned char d2;
  // --------
  inline void setTempo(unsigned int n)
  {
    d2 = (unsigned char) (n & 0xFF);
    d1 = (unsigned char) ((n >> 8) & 0xFF);
    st = (unsigned char) ((n >> 16) & 0x7F);
  }
  inline bool isTempo() const
  {
    return (!(st & 0x80));
  }
  inline unsigned int getTempo() const
  {
    return (((unsigned int) (st & 0x7F) << 16)
            | ((unsigned int) d1 << 8) | (unsigned int) d2);
  }
  inline int eventPriority() const
  {
    switch (st & 0xF0) {
    case 0xF0:
      return 1;
    case 0x80:
      return 2;
    case 0x90:
      return (d2 == 0 ? 2 : 6);
    case 0xC0:
      return 3;
    case 0xB0:
      return 4;
    case 0xE0:
      return 5;
    case 0xA0:
    case 0xD0:
      return 7;
    }
    return 0;
  }
  inline bool operator<(const MIDIEvent& r) const
  {
    if (t != r.t)
      return (t < r.t);
    int     p1 = eventPriority();
    int     p2 = r.eventPriority();
    if ((p1 & 3) == 2 && (p2 & 3) == 2) {
      if (((st ^ r.st) & 0x0F) == 0 && d1 == r.d1)
        return (this < &r);
      if (p1 == p2 && !optimizeNoteEvents) {
        if (p1 == 2)
          return ((st & 0x0F) > (r.st & 0x0F));
        return ((st & 0x0F) < (r.st & 0x0F));
      }
    }
    return (p1 < p2);
  }
};

bool MIDIEvent::optimizeNoteEvents = false;

static void errorMessage(const char *fmt, ...)
{
  char    msgBuf[1024];
  std::va_list  ap;
  va_start(ap, fmt);
  std::vsnprintf(msgBuf, 1024, fmt, ap);
  va_end(ap);
  msgBuf[1023] = '\0';
  throw std::runtime_error(std::string(msgBuf));
}

// ----------------------------------------------------------------------------

class File {
 protected:
  std::FILE *f;
 public:
  File(const char *fileName, const char *mode);
  virtual ~File();
  size_t size();
  void readBlock(std::vector< unsigned char >& buf, size_t nBytes);
  void writeBlock(const std::vector< unsigned char >& buf);
};

File::File(const char *fileName, const char *mode)
{
  if (!fileName || *fileName == '\0')
    errorMessage("invalid file name");
  if (!mode || *mode == '\0')
    errorMessage("invalid file open mode");
  f = std::fopen(fileName, mode);
  if (!f)
    errorMessage("error opening \"%s\"", fileName);
}

File::~File()
{
  std::fclose(f);
}

size_t File::size()
{
  long    curPos = std::ftell(f);
  long    fileSize = -1L;
  if (curPos < 0L)
    errorMessage("error seeking file");
  if (std::fseek(f, 0L, SEEK_END) >= 0)
    fileSize = std::ftell(f);
  if (std::fseek(f, curPos, SEEK_SET) < 0)
    fileSize = -1L;
  if (fileSize < 0L)
    errorMessage("error seeking file");
  return size_t(fileSize);
}

void File::readBlock(std::vector< unsigned char >& buf, size_t nBytes)
{
  buf.resize(buf.size() + nBytes);
  if (std::fread(&(buf.front()) + (buf.size() - nBytes),
                 sizeof(unsigned char), nBytes, f) != nBytes) {
    errorMessage("error reading input file");
  }
}

void File::writeBlock(const std::vector< unsigned char >& buf)
{
  if (buf.size() < 1)
    return;
  if (std::fwrite(&(buf.front()), sizeof(unsigned char), buf.size(), f)
      != buf.size() || std::fflush(f) != 0) {
    errorMessage("error writing output file");
  }
}

// ----------------------------------------------------------------------------

class Envelopes {
 public:
  static const size_t env_buf_size = 8192;
 protected:
  struct EnvelopeState {
    unsigned int  vol_l;
    int           vol_l_inc;
    unsigned char vol_l_mult;
    unsigned int  vol_r;
    int           vol_r_inc;
    unsigned char vol_r_mult;
    int           pb;
    int           pb_inc;
    unsigned char dist;
    unsigned char d;
  };
  std::vector< unsigned short > midi_pgm_layer2;
  std::vector< unsigned short > midi_drum_layer2;
  std::vector< unsigned short > pgm_env_offsets;
  std::vector< unsigned short > drum_env_offsets;
  std::vector< unsigned char >  envelope_data;
  std::vector< bool >   pgm_used;
  std::vector< bool >   drum_used;
  std::vector< unsigned char >  midiChnProgram;
  std::vector< unsigned char >  midiProgramMap;
  std::vector< unsigned char >  file_buf;
  size_t        file_buf_pos;
  EnvelopeState envState;
  // --------
  void stripSpace();
  char readChar();
  int readNumber();
  void updateEnvelope();
  void parseVolumeL();
  void parseVolumeR();
  void parsePitchBend(bool isDrum);
  void parseInstrLayer2(int n);
  void compileEnvelopes();
 public:
  Envelopes(const char *fileName);
  virtual ~Envelopes();
  void midiEvent(unsigned char st, unsigned char d1, unsigned char d2);
  void optimizeData(bool renumberPgm = false);
  void saveData(std::vector< unsigned char >& outBuf) const;
  inline unsigned char mapMIDIProgram(unsigned char pgm) const
  {
    return midiProgramMap[pgm & 0x7F];
  }
};

void Envelopes::stripSpace()
{
  if (file_buf.size() < 1)
    errorMessage("empty envelope file");
  unsigned char *s = &(file_buf.front());
  unsigned char *t = s;
  size_t  fsize = file_buf.size();
  bool    commentFlag = false;
  for ( ; fsize != 0; fsize--, s++) {
    unsigned char c = *s;
    if (commentFlag && c != '\r' && c != '\n' && c != '\0')
      continue;
    commentFlag = false;
    if (c <= (unsigned char) ' ')
      continue;
    if (c == '#') {
      commentFlag = true;
      continue;
    }
    *(t++) = c;
  }
  file_buf.resize(size_t((t + 1) - &(file_buf.front())));
  file_buf[file_buf.size() - 1] = '\0';
  file_buf_pos = 0;
}

char Envelopes::readChar()
{
  char    c = char(file_buf[file_buf_pos]);
  if ((c >= '0' && c <= '9') || c == '+' || c == '-')
    return '0';
  if (c && (file_buf_pos + 1) < file_buf.size())
    file_buf_pos++;
  return c;
}

int Envelopes::readNumber()
{
  int     n;
  bool    isNegative = false;
  char    c;
  c = char(file_buf[file_buf_pos]);
  if (c == '+' || c == '-') {
    isNegative = (c == '-');
    file_buf_pos++;
    c = char(file_buf[file_buf_pos]);
  }
  if (!c)
    errorMessage("unexpected end of envelope file");
  if (c < '0' || c > '9')
    errorMessage("invalid number format in envelope file");
  n = c - '0';
  while (true) {
    file_buf_pos++;
    c = char(file_buf[file_buf_pos]);
    if (c < '0' || c > '9')
      break;
    if (n >= 214748364)
      errorMessage("integer out of range in envelope file");
    n = n * 10 + (c - '0');
  }
  if (isNegative)
    n = -n;
  return n;
}

void Envelopes::updateEnvelope()
{
  if (envelope_data.size() > (env_buf_size - 4))
    errorMessage("envelope buffer overflow");
  envelope_data.push_back((unsigned char) (envState.vol_l >> 8));
  envelope_data.push_back((unsigned char) (envState.vol_r >> 8));
  unsigned int  pb = (unsigned int) envState.pb & 0x00FFFFFF;
  envelope_data.push_back((unsigned char) ((pb >> 8) & 0xFF));
  envelope_data.push_back(envState.dist | (unsigned char) ((pb >> 16) & 0x0F));
  envState.d--;
  envState.vol_l = envState.vol_l & 0x3FFF;
  if (envState.vol_l_mult)
    envState.vol_l = (envState.vol_l * envState.vol_l_mult + 64) >> 7;
  else
    envState.vol_l = envState.vol_l + envState.vol_l_inc;
  if (envState.vol_l & 0x8000)
    envState.vol_l = 0;
  else if (envState.vol_l >= 0x4000)
    envState.vol_l = 0x3FFF;
  envState.vol_r = envState.vol_r & 0x3FFF;
  if (envState.vol_r_mult)
    envState.vol_r = (envState.vol_r * envState.vol_r_mult + 64) >> 7;
  else
    envState.vol_r = envState.vol_r + envState.vol_r_inc;
  if (envState.vol_r & 0x8000)
    envState.vol_r = 0;
  else if (envState.vol_r >= 0x4000)
    envState.vol_r = 0x3FFF;
  envState.pb = envState.pb + envState.pb_inc;
}

void Envelopes::parseVolumeL()
{
  int     n;
  char    c = readChar();
  bool    multFlag = (c == '*');
  if (multFlag)
    c = readChar();
  if (c != '0')
    errorMessage("syntax error in envelope segment");
  n = readNumber();
  if (n < (multFlag ? 1 : 0) || n >= (multFlag ? 256 : 64))
    errorMessage("invalid left volume in envelope file");
  if (multFlag) {
    envState.vol_l_mult = (unsigned char) n;
  }
  else {
    envState.vol_l_mult = 0;
    if (!envState.d) {
      envState.vol_l = (envState.vol_l & 0xC000)
                       | ((unsigned int) n << 8) | 0x0080;
    }
    else {
      n = ((n << 8) | 0x0080) - int(envState.vol_l & 0x3FFF);
      if (n < 0)
        n = n - (envState.d >> 1);
      else
        n = n + (envState.d >> 1);
      envState.vol_l_inc = n / envState.d;
    }
  }
}

void Envelopes::parseVolumeR()
{
  int     n;
  char    c = readChar();
  bool    multFlag = (c == '*');
  if (multFlag)
    c = readChar();
  if (c != '0')
    errorMessage("syntax error in envelope segment");
  n = readNumber();
  if (n < (multFlag ? 1 : 0) || n >= (multFlag ? 256 : 64))
    errorMessage("invalid right volume in envelope file");
  if (multFlag) {
    envState.vol_r_mult = (unsigned char) n;
  }
  else {
    envState.vol_r_mult = 0;
    if (!envState.d) {
      envState.vol_r = ((unsigned int) n << 8) | 0x0080;
    }
    else {
      n = ((n << 8) | 0x0080) - int(envState.vol_r & 0x3FFF);
      if (n < 0)
        n = n - (envState.d >> 1);
      else
        n = n + (envState.d >> 1);
      envState.vol_r_inc = n / envState.d;
    }
  }
}

void Envelopes::parsePitchBend(bool isDrum)
{
  int     pb, n;
  if (readChar() != '0')
    errorMessage("syntax error in envelope segment");
  n = readNumber();
  if (n < -2048 || n > 2047 || (isDrum && n != 0))
    errorMessage("invalid pitch bend in envelope file");
  pb = (n << 8) | 0x0080;
  envState.pb_inc = 0;
  if (!envState.d) {
    envState.pb = pb;
  }
  else if (pb != envState.pb) {
    pb = pb - envState.pb;
    if (pb < 0)
      pb = pb - (envState.d >> 1);
    else
      pb = pb + (envState.d >> 1);
    envState.pb_inc = pb / envState.d;
  }
}

void Envelopes::parseInstrLayer2(int n)
{
  int     c, p;
  if (readChar() != '0')
    errorMessage("syntax error in envelope file");
  c = readNumber();
  if (c < -15 || c > 15)
    errorMessage("invalid channel offset in envelope file");
  if (readChar() != ',' || readChar() != '0')
    errorMessage("syntax error in envelope file");
  p = readNumber();
  if (p < -127 || p > 127)
    errorMessage("invalid pitch offset in envelope file");
  if (n < 0)
    midi_drum_layer2[-n] = (unsigned short) (((p & 0x7F) << 8) | (c & 0x0F));
  else
    midi_pgm_layer2[n] = (unsigned short) (((p & 0x7F) << 8) | (c & 0x0F));
}

void Envelopes::compileEnvelopes()
{
  std::vector< int >  instrList;
  int     n;
  char    c;

  stripSpace();
  file_buf_pos = 0;
  for (size_t i = 0; i < 128; i++) {
    pgm_env_offsets[i] = 0x8000;
    drum_env_offsets[i] = 0x8000;
    midi_pgm_layer2[i] = 0xFFFF;
    midi_drum_layer2[i] = 0xFFFF;
  }
  envelope_data.clear();
  while ((c = readChar()) != '\0') {
    unsigned char envFlags = 0x20;      // 0x20 = no sustain, 0x10 = release
    bool    isDrum = false;
    instrList.clear();
    while (true) {
      n = readNumber();
      if (n < -127 || n > 127)
        errorMessage("invalid program number in envelope file");
      if (instrList.size() > 0 && (n < 0) != isDrum)
        errorMessage("instrument type error in envelope file");
      isDrum = (n < 0);
      c = readChar();
      instrList.push_back(n);
      {
        size_t  envPos = envelope_data.size() >> 1;
        if (n == 9)
          envPos |= 0x4000;
        while ((c | 0x20) == 'p' || (c | 0x20) == 'd') {
          if ((c | 0x20) == 'p')
            envPos ^= 0x4000;
          else
            envFlags = 0x30;
          c = readChar();
        }
        if (!isDrum)
          pgm_env_offsets[n] = (unsigned short) envPos;
        else
          drum_env_offsets[-n] = (unsigned short) envPos;
      }
      if (c == ':') {
        parseInstrLayer2(n);
        c = readChar();
      }
      if (c == '{')
        break;
      if (c != ',')
        errorMessage("syntax error in envelope file");
    }
    envState.vol_l = 0x0080;
    envState.vol_l_inc = 0;
    envState.vol_l_mult = 0;
    envState.vol_r = 0x0080;
    envState.vol_r_inc = 0;
    envState.vol_r_mult = 0;
    envState.pb = 0x0080;
    envState.pb_inc = 0;
    envState.dist = 0x00;
    envState.d = 0;
    while (true) {
      if (envState.d) {
        updateEnvelope();
        continue;
      }
      c = readChar();
      if (c == '}')
        break;
      // duration
      if (readChar() != '0')
        errorMessage("syntax error in envelope segment");
      n = readNumber();
      if (n < 0 || n > 255)
        errorMessage("invalid envelope segment duration");
      envState.d = (unsigned char) n;
      if (c != '0' && (unsigned char) n) {
        switch (c | 0x20) {
        case 'l':                       // begin loop
          if (envFlags != 0x20)
            errorMessage("invalid loop in envelope file");
          envState.vol_l |= 0x4000;
          envFlags = 0x00;
          c = '0';
          break;
        case 'r':                       // end loop, release
          if (envFlags == 0x10)
            errorMessage("invalid loop in envelope file");
          envState.vol_l |= (envFlags ? 0xC000 : 0x8000);
          envFlags = 0x10;
          c = '0';
          break;
        case 's':                       // hold single frame, release
          if (envFlags != 0x20)
            errorMessage("invalid loop in envelope file");
          // flags are not set for compatibility
          envState.vol_l |= 0xC000;
          c = '0';
          break;
        }
      }
      if (c != '0' || readChar() != ',')
        errorMessage("syntax error in envelope segment");
      parseVolumeL();
      if (readChar() != ',')
        errorMessage("syntax error in envelope segment");
      parseVolumeR();
      if (readChar() != ',')
        errorMessage("syntax error in envelope segment");
      parsePitchBend(isDrum);
      // distortion
      if (readChar() != ',')
        errorMessage("syntax error in envelope segment");
      if (readChar() != '0')
        errorMessage("syntax error in envelope segment");
      n = readNumber();
      if (n < 0 || n > 255 || (n & (isDrum ? 0x00 : 0x0F)) != 0)
        errorMessage("invalid distortion in envelope file");
      envState.dist = (unsigned char) n;
      if (readChar() != ';')
        errorMessage("syntax error in envelope segment");
    }
    if (envelope_data.size() > (env_buf_size - 2))
      errorMessage("envelope buffer overflow");
    envelope_data.push_back(0x80);
    envelope_data.push_back(!envFlags ? 0x00 : 0xFF);
    for (size_t i = 0; i < instrList.size(); i++) {
      n = instrList[i];
      if (n >= 0)
        pgm_env_offsets[n] |= ((unsigned short) envFlags << 8);
      else
        drum_env_offsets[-n] |= ((unsigned short) envFlags << 8);
    }
  }
}

Envelopes::Envelopes(const char *fileName)
  : midi_pgm_layer2(128, 0xFFFF),
    midi_drum_layer2(128, 0xFFFF),
    pgm_env_offsets(128, 0x8000),
    drum_env_offsets(128, 0x8000),
    pgm_used(128, false),
    drum_used(128, false),
    midiChnProgram(16, 0),
    midiProgramMap(128, 0),
    file_buf_pos(0)
{
  for (size_t i = 0; i < midiProgramMap.size(); i++)
    midiProgramMap[i] = (unsigned char) i;
  bool    isBinary = false;
  {
    File    f(fileName, "rb");
    f.readBlock(file_buf, f.size());
    bool    haveNUL = false;
    bool    haveCRLF = false;
    bool    have8080 = false;
    bool    haveFFFF = false;
    for (size_t i = 0; i < file_buf.size(); i++) {
      switch (file_buf[i]) {
      case '\0':
        haveNUL = true;
        break;
      case '\n':
      case '\r':
        haveCRLF = true;
        break;
      case 0x80:
        if ((i + 1) < file_buf.size() && file_buf[i + 1] == 0x80)
          have8080 = true;
        break;
      case 0xFF:
        if ((i + 1) < file_buf.size() && file_buf[i + 1] == 0xFF)
          haveFFFF = true;
        break;
      }
    }
    isBinary =
        (!haveCRLF || (int(haveNUL) + int(have8080) + int(haveFFFF)) >= 2);
    if (isBinary && !haveFFFF)
      errorMessage("invalid binary envelope file format");
  }
  if (isBinary) {
    if (file_buf.size() < (1024 + 6) ||
        file_buf.size() > (1024 + env_buf_size) || (file_buf.size() & 1) != 0) {
      errorMessage("invalid binary envelope file size");
    }
    if (!(file_buf[file_buf.size() - 2] & 0x80))
      errorMessage("invalid binary envelope file format");
    for (size_t i = 1024; i < file_buf.size(); i++)
      envelope_data.push_back(file_buf[i]);
    for (size_t i = 0; i < 128; i++) {
      midi_pgm_layer2[i] = (unsigned short) file_buf[i << 1]
                           | ((unsigned short) file_buf[(i << 1) + 1] << 8);
      midi_drum_layer2[i] = (unsigned short) file_buf[(i << 1) + 256]
                            | ((unsigned short) file_buf[(i << 1) + 257] << 8);
      pgm_env_offsets[i] = (unsigned short) file_buf[(i << 1) + 512]
                           | ((unsigned short) file_buf[(i << 1) + 513] << 8);
      drum_env_offsets[i] = (unsigned short) file_buf[(i << 1) + 768]
                            | ((unsigned short) file_buf[(i << 1) + 769] << 8);
      if ((midi_pgm_layer2[i] & 0xFF) == 0xFF)
        midi_pgm_layer2[i] = 0xFFFF;
      else
        midi_pgm_layer2[i] = midi_pgm_layer2[i] & 0x7F0F;
      if ((midi_drum_layer2[i] & 0xFF) == 0xFF)
        midi_drum_layer2[i] = 0xFFFF;
      else
        midi_drum_layer2[i] = midi_drum_layer2[i] & 0x7F0F;
      if (pgm_env_offsets[i] & 0x8000)
        pgm_env_offsets[i] = 0x8000;
      if (drum_env_offsets[i] & 0x8000)
        drum_env_offsets[i] = 0x8000;
      if (((pgm_env_offsets[i] & 0x0FFF) << 1) >= envelope_data.size() ||
          ((drum_env_offsets[i] & 0x0FFF) << 1) >= envelope_data.size()) {
        errorMessage("invalid envelope data offset in binary file");
      }
    }
  }
  else {
    compileEnvelopes();
  }
}

Envelopes::~Envelopes()
{
}

void Envelopes::midiEvent(unsigned char st, unsigned char d1, unsigned char d2)
{
  d1 = d1 & 0x7F;
  d2 = d2 & 0x7F;
  if ((st & 0xF0) == 0xC0) {            // program change
    midiChnProgram[st & 0x0F] = d1;
  }
  else if ((st & 0xF0) == 0x90) {       // note on
    if (!d2)
      return;
    unsigned short  l2 = 0xFFFF;
    unsigned char   c = st & 0x0F;
    bool    isDrum = (c == 9);
    if (!isDrum) {
      unsigned char pgm = midiChnProgram[c];
      pgm_used[pgm] = true;
      l2 = midi_pgm_layer2[pgm];
    }
    else {
      drum_used[d1] = true;
      l2 = midi_drum_layer2[d1];
    }
    if ((l2 & 0xFF) != 0xFF) {
      c = (c + (unsigned char) (l2 & 0xFF)) & 0x0F;
      d1 = (d1 + (unsigned char) (l2 >> 8)) & 0x7F;
      isDrum = (c == 9);
      if (!isDrum)
        pgm_used[midiChnProgram[c]] = true;
      else
        drum_used[d1] = true;
    }
  }
}

void Envelopes::optimizeData(bool renumberPgm)
{
  std::vector< unsigned short > pgm_l2_new(128, 0xFFFF);
  std::vector< unsigned short > drum_l2_new(128, 0xFFFF);
  std::vector< unsigned short > pgm_env_new(128, 0x8000);
  std::vector< unsigned short > drum_env_new(128, 0x8000);
  std::vector< unsigned char >  env_data_new;
  unsigned char pgm = 0;
  for (size_t i = 0; i < 128; i++) {
    if (!pgm_used[i])
      continue;
    if (!renumberPgm)
      pgm = i;
    midiProgramMap[i] = pgm;
    pgm_l2_new[pgm] = midi_pgm_layer2[i];
    if (pgm_env_offsets[i] & 0x8000) {
      pgm_env_new[pgm] = 0x8000;
      pgm++;
      continue;
    }
    bool    foundMatch = false;
    for (size_t j = 0; j < i; j++) {
      if (pgm_used[j] &&
          !((pgm_env_offsets[i] ^ pgm_env_offsets[j]) & 0x8FFF)) {
        pgm_env_new[pgm] = (pgm_env_offsets[i] & 0xF000)
                           | (pgm_env_new[midiProgramMap[j]] & 0x0FFF);
        foundMatch = true;
        break;
      }
    }
    if (foundMatch) {
      pgm++;
      continue;
    }
    pgm_env_new[pgm] = (pgm_env_offsets[i] & 0xF000)
                       | (unsigned short) (env_data_new.size() >> 1);
    for (size_t j = (pgm_env_offsets[i] & 0x0FFF) << 1;
         (j + 2) <= envelope_data.size();
         j = j + 4) {
      unsigned char b0 = envelope_data[j];
      unsigned char b1 = envelope_data[j + 1];
      env_data_new.push_back(b0);
      env_data_new.push_back(b1);
      if (((b0 & 0x80) != 0 && b1 == 0xFF) ||
          ((pgm_env_offsets[i] & 0x3000) == 0 && b0 == 0x80 && b1 == 0x00)) {
        break;
      }
      if ((j + 2) < envelope_data.size())
        env_data_new.push_back(envelope_data[j + 2]);
      if ((j + 3) < envelope_data.size())
        env_data_new.push_back(envelope_data[j + 3]);
    }
    pgm++;
  }
  for (size_t i = 0; i < 128; i++) {
    if (!drum_used[i])
      continue;
    drum_l2_new[i] = midi_drum_layer2[i];
    if (drum_env_offsets[i] & 0x8000) {
      drum_env_new[i] = 0x8000;
      continue;
    }
    bool    foundMatch = false;
    for (size_t j = 0; j < i; j++) {
      if (drum_used[j] &&
          !((drum_env_offsets[i] ^ drum_env_offsets[j]) & 0x8FFF)) {
        drum_env_new[i] = (drum_env_offsets[i] & 0xF000)
                          | (drum_env_new[j] & 0x0FFF);
        foundMatch = true;
        break;
      }
    }
    if (foundMatch)
      continue;
    drum_env_new[i] = (drum_env_offsets[i] & 0xF000)
                      | (unsigned short) (env_data_new.size() >> 1);
    for (size_t j = (drum_env_offsets[i] & 0x0FFF) << 1;
         (j + 2) <= envelope_data.size();
         j = j + 4) {
      unsigned char b0 = envelope_data[j];
      unsigned char b1 = envelope_data[j + 1];
      env_data_new.push_back(b0);
      env_data_new.push_back(b1);
      if (((b0 & 0x80) != 0 && b1 == 0xFF) ||
          ((drum_env_offsets[i] & 0x3000) == 0 && b0 == 0x80 && b1 == 0x00)) {
        break;
      }
      if ((j + 2) < envelope_data.size())
        env_data_new.push_back(envelope_data[j + 2]);
      if ((j + 3) < envelope_data.size())
        env_data_new.push_back(envelope_data[j + 3]);
    }
  }
  midi_pgm_layer2.clear();
  midi_pgm_layer2.insert(midi_pgm_layer2.end(),
                         pgm_l2_new.begin(), pgm_l2_new.end());
  midi_drum_layer2.clear();
  midi_drum_layer2.insert(midi_drum_layer2.end(),
                          drum_l2_new.begin(), drum_l2_new.end());
  pgm_env_offsets.clear();
  pgm_env_offsets.insert(pgm_env_offsets.end(),
                         pgm_env_new.begin(), pgm_env_new.end());
  drum_env_offsets.clear();
  drum_env_offsets.insert(drum_env_offsets.end(),
                          drum_env_new.begin(), drum_env_new.end());
  envelope_data.clear();
  envelope_data.insert(envelope_data.end(),
                       env_data_new.begin(), env_data_new.end());
}

void Envelopes::saveData(std::vector< unsigned char >& outBuf) const
{
  for (size_t i = 0; i < midi_pgm_layer2.size(); i++) {
    outBuf.push_back((unsigned char) (midi_pgm_layer2[i] & 0xFF));
    outBuf.push_back((unsigned char) (midi_pgm_layer2[i] >> 8));
  }
  for (size_t i = 0; i < midi_drum_layer2.size(); i++) {
    outBuf.push_back((unsigned char) (midi_drum_layer2[i] & 0xFF));
    outBuf.push_back((unsigned char) (midi_drum_layer2[i] >> 8));
  }
  for (size_t i = 0; i < pgm_env_offsets.size(); i++) {
    outBuf.push_back((unsigned char) (pgm_env_offsets[i] & 0xFF));
    outBuf.push_back((unsigned char) (pgm_env_offsets[i] >> 8));
  }
  for (size_t i = 0; i < drum_env_offsets.size(); i++) {
    outBuf.push_back((unsigned char) (drum_env_offsets[i] & 0xFF));
    outBuf.push_back((unsigned char) (drum_env_offsets[i] >> 8));
  }
  for (size_t i = 0; i < envelope_data.size(); i++)
    outBuf.push_back(envelope_data[i]);
}

// ----------------------------------------------------------------------------

class MIDIFile {
 protected:
  std::vector< MIDIEvent >  evtBuf;
  std::vector< unsigned char >  buf;
  size_t  bufPos;
  size_t  nTracks;
  size_t  trackBytesLeft;
  int     dTime;
  bool    noTempo;
  // --------
  inline unsigned char readByte()
  {
    if (bufPos >= buf.size())
      errorMessage("unexpected end of MIDI file");
    if (trackBytesLeft < 1)
      errorMessage("unexpected end of track in MIDI file");
    unsigned char c = buf[bufPos];
    bufPos++;
    trackBytesLeft--;
    return c;
  }
  inline unsigned int readUInt16()
  {
    unsigned int  n = readByte();
    n = (n << 8) | readByte();
    return n;
  }
  inline unsigned int readUInt24()
  {
    unsigned int  n = readByte();
    n = (n << 8) | readByte();
    n = (n << 8) | readByte();
    return n;
  }
  inline unsigned int readUInt32()
  {
    unsigned int  n = readByte();
    n = (n << 8) | readByte();
    n = (n << 8) | readByte();
    n = (n << 8) | readByte();
    return n;
  }
  inline unsigned int readUIntVLen()
  {
    unsigned int  n = readByte();
    if (n & 0x80) {
      unsigned char c = readByte();
      n = ((n & 0x7F) << 7) | (c & 0x7F);
      if (c & 0x80) {
        c = readByte();
        n = (n << 7) | (c & 0x7F);
        if (c & 0x80) {
          c = readByte();
          if (c & 0x80)
            errorMessage("invalid MIDI file data");
          n = (n << 7) | (c & 0x7F);
        }
      }
    }
    return n;
  }
  void sortEvents();
  double calculateTickTime(unsigned int usPerBeat, double irqFreq,
                           int quantizeTPQN) const;
 public:
  MIDIFile(const char *fileName);
  virtual ~MIDIFile();
  void getRawData(std::vector< unsigned char >& outBuf,
                  double irqFreq, const Envelopes *env,
                  int roundingBias = 64, int quantizeTPQN = 0) const;
  void getAllData(std::vector< unsigned char >& outBuf,
                  const char *envFile, double irqFreq,
                  bool renumberPgm = false, int roundingBias = 64,
                  int quantizeTPQN = 0) const;
};

void MIDIFile::sortEvents()
{
  bool    doneFlag;
  do {
    doneFlag = true;
    for (size_t i = 0; (i + 1) < evtBuf.size(); i++) {
      if (evtBuf[i] < evtBuf[i + 1])
        continue;
      if (!(evtBuf[i + 1] < evtBuf[i])) {
        if (evtBuf[i].st == evtBuf[i + 1].st ||
            (i > 0 && evtBuf[i - 1].st == evtBuf[i].st) ||
            ((i + 2) < evtBuf.size() && evtBuf[i + 1].st == evtBuf[i + 2].st)) {
          continue;
        }
        if (!((i > 0 && evtBuf[i - 1].st == evtBuf[i + 1].st) ||
              ((i + 2) < evtBuf.size() && evtBuf[i].st == evtBuf[i + 2].st))) {
          continue;
        }
        if (evtBuf[i].isTempo())
          continue;
      }
      MIDIEvent tmp = evtBuf[i];
      evtBuf[i] = evtBuf[i + 1];
      evtBuf[i + 1] = tmp;
      doneFlag = false;
    }
  } while (!doneFlag);
}

MIDIFile::MIDIFile(const char *fileName)
  : bufPos(14),
    trackBytesLeft(0x7FFFFFFF),
    noTempo(false)
{
  buf.clear();
  File    f(fileName, "rb");
  f.readBlock(buf, f.size());
  if (buf.size() < 24)
    errorMessage("invalid input file \"%s\"", fileName);
  if (buf[0] != 'M' || buf[1] != 'T' || buf[2] != 'h' || buf[3] != 'd' ||
      buf[4] != 0x00 || buf[5] != 0x00 || buf[6] != 0x00 || buf[7] != 0x06 ||
      buf[8] != 0x00 || (buf[9] & 0xFE) != 0x00) {
    errorMessage("invalid input file format, must be MIDI file type 0 or 1");
  }
  if (!buf[11])
    errorMessage("invalid time code in MIDI file");
  nTracks = (size_t(buf[10]) << 8) | buf[11];
  if (!nTracks)
    errorMessage("invalid number of tracks in MIDI file");
  dTime = (int(buf[12] & 0x7F) << 8) | buf[13];
  MIDIEvent e;
  e.t = 0UL;
  e.setTempo(500000U);                  // default to 120 BPM
  if (buf[12] & 0x80) {
    dTime = dTime & 0xFF;
    noTempo = true;
    double  t = double(256 - int(buf[12]));
    if (buf[12] == 0xE3)
      t = 29.97;
    e.setTempo((unsigned int) int(1000000.0 / t + 0.5));
  }
  evtBuf.push_back(e);
  for (size_t t = 0; t < nTracks; t++) {
    trackBytesLeft = 8;
    if (readUInt32() != 0x4D54726BU)    // "MTrk"
      errorMessage("invalid MIDI file track header (track %d)", int(t));
    trackBytesLeft = size_t(readUInt32());
    unsigned long curTime = 0UL;
    unsigned char savedStatus = 0x00;
    bool    endOfTrack = false;
    do {
      unsigned int  dt = readUIntVLen();
      curTime = curTime + dt;
      e.t = curTime;
      unsigned char st = readByte();
      if (st == 0xFF) {                 // meta events
        st = readByte();
        size_t  evtBytes = size_t(readUIntVLen());
        switch (st) {
        case 0x2F:                      // end of track
          endOfTrack = true;
          break;
        case 0x51:                      // set tempo
          if (evtBytes != 3)
            errorMessage("invalid tempo event in MIDI file track %d", int(t));
          e.setTempo(readUInt24());
          evtBytes = evtBytes - 3;
          if (!noTempo)
            evtBuf.push_back(e);
          break;
        }
        for ( ; evtBytes > 0; evtBytes--)
          (void) readByte();
      }
      else if (st >= 0xF0) {
        savedStatus = 0x00;
        if (st == 0xF0 || st == 0xF7) {
          size_t  evtBytes = size_t(readUIntVLen());
          for ( ; evtBytes > 0; evtBytes--)
            (void) readByte();
        }
        else {
          errorMessage("invalid event data in MIDI file track %d", int(t));
        }
      }
      else {
        if (st < 0x80) {
          if (savedStatus < 0x80)
            errorMessage("invalid event data in MIDI file track %d", int(t));
          st = savedStatus;
          bufPos--;
          trackBytesLeft++;
        }
        unsigned char d1 = readByte();
        unsigned char d2 = 0x00;
        if (d1 >= 0x80)
          errorMessage("invalid event data in MIDI file track %d", int(t));
        if ((st & 0xE0) != 0xC0) {
          d2 = readByte();
          if (d2 >= 0x80)
            errorMessage("invalid event data in MIDI file track %d", int(t));
        }
        savedStatus = st;
        if ((st & 0xF0) == 0x80) {
          // Note Off -> Note On with velocity == 0
          st = st | 0x10;
          d2 = 0x00;
        }
        if ((st & 0xF0) != 0xB0 ||
            d1 == 7 || d1 == 10 || d1 == 70 || d1 == 71 ||
            d1 == 76 || d1 == 77 || d1 == 120 || d1 == 121 || d1 == 123) {
          e.st = st;
          e.d1 = d1;
          e.d2 = d2;
          evtBuf.push_back(e);
        }
      }
    } while (!endOfTrack);
    for ( ; trackBytesLeft > 0; trackBytesLeft--)
      (void) readByte();
  }
  sortEvents();
}

MIDIFile::~MIDIFile()
{
}

double MIDIFile::calculateTickTime(unsigned int usPerBeat, double irqFreq,
                                   int quantizeTPQN) const
{
  if (quantizeTPQN > 0) {
    double  irqPerBeat = double(int(usPerBeat)) * irqFreq / 1000000.0;
    int     n = int((irqPerBeat / double(quantizeTPQN)) + 0.5);
    n = (n > 1 ? n : 1);
    irqPerBeat = double(n * quantizeTPQN);
    return ((irqPerBeat / irqFreq) / double(dTime));
  }
  return (double(int(usPerBeat)) / (double(dTime) * 1000000.0));
}

void MIDIFile::getRawData(std::vector< unsigned char >& outBuf,
                          double irqFreq, const Envelopes *env,
                          int roundingBias, int quantizeTPQN) const
{
  double  tickTime = calculateTickTime(500000U, irqFreq, quantizeTPQN);
  double  curTime = 0.0;
  long    prvTick = 0L;
  long    prvIRQCnt = 0L;
  unsigned char prvStatus = 0xFF;
  for (size_t i = 0; i < evtBuf.size(); i++) {
    long    curTick = long(evtBuf[i].t);
    curTime = curTime + (tickTime * (curTick - prvTick));
    if (evtBuf[i].isTempo()) {
      tickTime = calculateTickTime(evtBuf[i].getTempo(), irqFreq, quantizeTPQN);
    }
    else {
      long    irqCnt = long(curTime * irqFreq + (double(roundingBias) / 256.0));
      unsigned int  dt = (unsigned int) (irqCnt - prvIRQCnt);
      if (!dt && evtBuf[i].st == prvStatus && (prvStatus & 0xF0) >= 0xA0 &&
          ((prvStatus & 0xE0) == 0xC0 ||
           evtBuf[i].d1 == outBuf[outBuf.size() - 2])) {
        // delete redundant events
        outBuf.resize(outBuf.size() - ((prvStatus & 0xE0) == 0xC0 ? 1 : 2));
      }
      else {
        if (dt >= 0x4000U)
          outBuf.push_back((unsigned char) (((dt >> 14) & 0x7F) | 0x80));
        if (dt >= 0x80U)
          outBuf.push_back((unsigned char) (((dt >> 7) & 0x7F) | 0x80));
        outBuf.push_back((unsigned char) (dt & 0x7F));
      }
      if (evtBuf[i].st != prvStatus) {
        prvStatus = evtBuf[i].st;
        outBuf.push_back(prvStatus);
      }
      if (env && (prvStatus & 0xF0) == 0xC0)
        outBuf.push_back(env->mapMIDIProgram(evtBuf[i].d1));
      else
        outBuf.push_back(evtBuf[i].d1);
      if ((prvStatus & 0xE0) != 0xC0)
        outBuf.push_back(evtBuf[i].d2);
      prvIRQCnt = irqCnt;
    }
    prvTick = curTick;
  }
}

void MIDIFile::getAllData(std::vector< unsigned char >& outBuf,
                          const char *envFile, double irqFreq, bool renumberPgm,
                          int roundingBias, int quantizeTPQN) const
{
  size_t  envSize;
  outBuf.resize(16, 0x00);
  outBuf[1] = 'm';
  {
    Envelopes env(envFile);
    for (size_t i = 0; i < evtBuf.size(); i++) {
      if (!evtBuf[i].isTempo())
        env.midiEvent(evtBuf[i].st, evtBuf[i].d1, evtBuf[i].d2);
    }
    env.optimizeData(renumberPgm);
    env.saveData(outBuf);
    envSize = outBuf.size() - 16;
    if (envSize < (1024 + 6) || envSize > (1024 + Envelopes::env_buf_size))
      errorMessage("\"%s\": invalid envelope file size", envFile);
    getRawData(outBuf, irqFreq, &env, roundingBias, quantizeTPQN);
  }
  outBuf[2] = (unsigned char) ((outBuf.size() - 16) & 0xFF);
  outBuf[3] = (unsigned char) ((outBuf.size() - 16) >> 8);
  outBuf[4] = (unsigned char) (envSize & 0xFF);
  outBuf[5] = (unsigned char) (envSize >> 8);
  outBuf[6] = (unsigned char) ((outBuf.size() - (envSize + 16)) & 0xFF);
  outBuf[7] = (unsigned char) ((outBuf.size() - (envSize + 16)) >> 8);
}

// ----------------------------------------------------------------------------

static void renderDaveData(std::vector< unsigned char >& outBuf)
{
  size_t    envSize = size_t(outBuf[4]) | (size_t(outBuf[5]) << 8);
  DavePlay  *davePlay = new DavePlay();
  davePlay->loadEnvelopes(&(outBuf.front()) + 16, envSize);
  std::vector< unsigned char >  tmpBuf;
  unsigned char daveRegs[16];
  unsigned int  dTime = 0;
  unsigned char prvStatus = 0x00;
  for (size_t i = envSize + 16; i < outBuf.size(); ) {
    dTime = outBuf[i];
    i++;
    if (dTime >= 0x80) {
      if (i >= outBuf.size())
        break;
      dTime = ((dTime & 0x7F) << 7) | (outBuf[i] & 0x7F);
      i++;
      if (outBuf[i - 1] >= 0x80) {
        if (i >= outBuf.size())
          break;
        dTime = (dTime << 7) | (outBuf[i] & 0x7F);
        i++;
      }
    }
    for ( ; dTime > 0; dTime--) {
      davePlay->update(daveRegs);
      for (size_t j = 0; j < 16; j++)
        tmpBuf.push_back(daveRegs[j]);
    }
    if (i >= outBuf.size())
      break;
    unsigned char st = outBuf[i];
    i++;
    unsigned char d1 = 0x00;
    unsigned char d2 = 0x00;
    if (st < 0x80) {
      d1 = st;
      st = prvStatus;
    }
    else if (st < 0xF0) {
      prvStatus = st;
      if (i >= outBuf.size())
        break;
      d1 = outBuf[i] & 0x7F;
      i++;
    }
    if (st < 0xC0 || (st & 0xF0) == 0xE0) {
      if (i >= outBuf.size())
        break;
      d2 = outBuf[i] & 0x7F;
      i++;
    }
    davePlay->midiEvent(st, d1, d2);
  }
  delete davePlay;
  outBuf.clear();
  outBuf.insert(outBuf.end(), tmpBuf.begin(), tmpBuf.end());
}

static void compressOutputData(std::vector< unsigned char >& outBuf,
                               int compressLevel, bool rawFormat)
{
  std::vector< unsigned char >  tmpBuf;
  if (rawFormat) {
    tmpBuf.insert(tmpBuf.end(), outBuf.begin(), outBuf.end());
    outBuf.clear();
    Ep128Compress::Compressor_M2  compressor(outBuf);
    compressor.setCompressionLevel(compressLevel);
    compressor.compressData(tmpBuf, 0xFFFFFFFFU, true, true);
    return;
  }
  std::vector< unsigned char >  tmpBuf2;
  size_t  envSize = size_t(outBuf[4]) | (size_t(outBuf[5]) << 8);
  tmpBuf.insert(tmpBuf.end(),
                outBuf.begin() + 16, outBuf.begin() + 16 + envSize);
  {
    Ep128Compress::Compressor_M2  compressor(tmpBuf2);
    compressor.setCompressionLevel(compressLevel);
    compressor.compressData(tmpBuf, 0xFFFFFFFFU, true, true);
  }
  tmpBuf.clear();
  tmpBuf.insert(tmpBuf.end(), outBuf.begin() + 16 + envSize, outBuf.end());
  outBuf.resize(16);
  outBuf.insert(outBuf.end(), tmpBuf2.begin(), tmpBuf2.end());
  outBuf[9] = 0x02;
  outBuf[10] = (unsigned char) (tmpBuf2.size() & 0xFF);
  outBuf[11] = (unsigned char) (tmpBuf2.size() >> 8);
  tmpBuf2.clear();
  {
    Ep128Compress::Compressor_M2  compressor(tmpBuf2);
    compressor.setCompressionLevel(compressLevel);
    compressor.compressData(tmpBuf, 0xFFFFFFFFU, true, true);
  }
  outBuf.insert(outBuf.end(), tmpBuf2.begin(), tmpBuf2.end());
  outBuf[2] = (unsigned char) ((outBuf.size() - 16) & 0xFF);
  outBuf[3] = (unsigned char) ((outBuf.size() - 16) >> 8);
}

int main(int argc, char **argv)
{
  try {
    if (argc < 4) {
      std::fprintf(stderr,
                   "Usage: midiconv INFILE.MID OUTFILE.BIN "
                   "ENVELOPE.TXT|ENVELOPE.BIN|-raw [OPTIONS]\n");
      std::fprintf(stderr, "       midiconv ENVELOPE.TXT ENVELOPE.BIN -env\n");
      std::fprintf(stderr, "Options:\n");
      std::fprintf(stderr, "    IRQFREQ (Hz, default = 50.0363257)\n");
      std::fprintf(stderr, "    -optsort\n");
      std::fprintf(stderr, "    -renumber\n");
      std::fprintf(stderr, "    -quantN (quantize IRQ / quarter note, "
                           "N = 0 to 9)\n");
      std::fprintf(stderr, "    -biasN (N = 0 to 99, default = 25)\n");
      std::fprintf(stderr, "    -0..9 (compression level)\n");
      std::fprintf(stderr, "    -render\n");
      errorMessage("invalid number of arguments");
    }
    double  irqFreq = 17734475.0 / (4.0 * 284.0 * 312.0);
    int     quantizeTPQN = 0;
    int     roundingBias = 64;
    int     compressLevel = 0;
    bool    optSort = false;
    bool    renumberPgm = false;
    bool    rawFormat = true;
    bool    renderDaveOutput = false;
    for (int i = 4; i < argc; i++) {
      if (std::strcmp(argv[i], "-optsort") == 0) {
        optSort = true;
      }
      else if (std::strcmp(argv[i], "-no-optsort") == 0) {
        optSort = false;
      }
      else if (std::strcmp(argv[i], "-renumber") == 0) {
        renumberPgm = true;
      }
      else if (std::strcmp(argv[i], "-no-renumber") == 0) {
        renumberPgm = false;
      }
      else if (std::strncmp(argv[i], "-quant", 6) == 0 &&
               argv[i][6] >= '0' && argv[i][6] <= '9' && argv[i][7] == '\0') {
        quantizeTPQN = int(argv[i][6] - '0');
      }
      else if (std::strcmp(argv[i], "-no-quant") == 0) {
        quantizeTPQN = 0;
      }
      else if (std::strncmp(argv[i], "-bias", 5) == 0 &&
               argv[i][5] >= '0' && argv[i][5] <= '9' &&
               (argv[i][6] == '\0' ||
                (argv[i][6] >= '0' && argv[i][6] <= '9' &&
                 argv[i][7] == '\0'))) {
        roundingBias = int(argv[i][5] - '0');
        if (argv[i][6])
          roundingBias = (roundingBias * 10) + int(argv[i][6] - '0');
        roundingBias = ((roundingBias << 8) + 50) / 100;
      }
      else if (std::strcmp(argv[i], "-render") == 0) {
        renderDaveOutput = true;
      }
      else if (std::strcmp(argv[i], "-no-render") == 0) {
        renderDaveOutput = false;
      }
      else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9' &&
               argv[i][2] == '\0') {
        compressLevel = int(argv[i][1] - '0');
      }
      else {
        char    *endp = (char *) 0;
        irqFreq = std::strtod(argv[i], &endp);
        if (!endp || endp == argv[i] || *endp != '\0')
          errorMessage("invalid option: '%s'", argv[i]);
        if (!(irqFreq >= 10.0 && irqFreq <= 10000.0))
          errorMessage("invalid IRQ frequency");
      }
    }
    MIDIEvent::optimizeNoteEvents = optSort;
    std::vector< unsigned char >  outBuf;
    if (std::strcmp(argv[3], "-env") == 0) {
      Envelopes env(argv[1]);
      env.saveData(outBuf);
      if (outBuf.size() < (1024 + 6) ||
          outBuf.size() > (1024 + Envelopes::env_buf_size)) {
        errorMessage("\"%s\": invalid envelope file size", argv[1]);
      }
    }
    else {
      MIDIFile  midiFile(argv[1]);
      if (std::strcmp(argv[3], "-raw") == 0) {
        midiFile.getRawData(outBuf, irqFreq, (Envelopes *) 0,
                            roundingBias, quantizeTPQN);
      }
      else {
        rawFormat = false;
        midiFile.getAllData(outBuf, argv[3], irqFreq, renumberPgm,
                            roundingBias, quantizeTPQN);
      }
    }
    if (renderDaveOutput) {
      if (rawFormat)
        errorMessage("-render requires a MIDI and an envelope file");
      rawFormat = true;
      renderDaveData(outBuf);
    }
    if (compressLevel > 0)
      compressOutputData(outBuf, compressLevel, rawFormat);
    File    f(argv[2], "wb");
    f.writeBlock(outBuf);
  }
  catch (std::exception& e) {
    std::fprintf(stderr, " *** %s: %s\n", argv[0], e.what());
    return -1;
  }
  return 0;
}

