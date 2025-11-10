# Custom Audio Files for M5 Pomodoro v2

This directory is for **optional** custom audio files. If you don't provide audio files here, the device will use embedded beep sounds from firmware (FLASH).

## Audio File Requirements

### Supported Files

Place these files in this directory (`/audio/` on SD card root):

1. **work_start.wav** - Played when work session begins
2. **rest_start.wav** - Played when short break begins
3. **long_rest_start.wav** - Played when long break begins
4. **warning.wav** - Played 30 seconds before session ends

**All 4 files must be present** for SD audio to work. If any file is missing, the device will fallback to embedded FLASH audio (beeps).

### Technical Specifications

**Format**: WAV (Waveform Audio File Format)

**Required Settings**:
- **Sample Rate**: 16000 Hz (16kHz)
- **Bit Depth**: 16-bit signed PCM
- **Channels**: 1 (mono)
- **Encoding**: PCM (uncompressed)

**Recommended Settings**:
- **Duration**: 1-3 seconds (keeps it short and non-intrusive)
- **File Size**: <100KB per file (larger files may fail to load into PSRAM)

**Not Supported**:
- ❌ MP3, AAC, OGG, FLAC (compressed formats)
- ❌ Stereo audio (will be converted to mono, wasting space)
- ❌ Sample rates >16kHz (will be resampled by hardware, no benefit)

## Creating Audio Files

### Option 1: Using Audacity (Free, Cross-platform)

**Download**: https://www.audacityteam.org/

**Steps**:
1. Open or record audio in Audacity
2. Convert to mono: **Tracks** > **Mix** > **Mix Stereo Down to Mono**
3. Set project rate: Bottom left, select **16000 Hz**
4. Trim to 1-3 seconds: Select portion, **Edit** > **Trim Audio**
5. Export:
   - **File** > **Export** > **Export Audio...**
   - Format: **WAV (Microsoft) signed 16-bit PCM**
   - Save to this directory with correct filename

### Option 2: Using FFmpeg (Command Line)

**Install**: https://ffmpeg.org/download.html

**Convert any audio file to correct format**:
```bash
ffmpeg -i input.mp3 -ar 16000 -ac 1 -sample_fmt s16 work_start.wav
```

**Explanation**:
- `-ar 16000`: Set sample rate to 16kHz
- `-ac 1`: Convert to mono (1 channel)
- `-sample_fmt s16`: Use 16-bit signed PCM

**Trim to 2 seconds**:
```bash
ffmpeg -i input.wav -ar 16000 -ac 1 -sample_fmt s16 -t 2 work_start.wav
```

**Batch convert all files**:
```bash
# Windows PowerShell
ffmpeg -i work_start_original.mp3 -ar 16000 -ac 1 -sample_fmt s16 work_start.wav
ffmpeg -i rest_start_original.mp3 -ar 16000 -ac 1 -sample_fmt s16 rest_start.wav
ffmpeg -i long_rest_start_original.mp3 -ar 16000 -ac 1 -sample_fmt s16 long_rest_start.wav
ffmpeg -i warning_original.mp3 -ar 16000 -ac 1 -sample_fmt s16 warning.wav
```

### Option 3: Online Converters

**Recommended Sites**:
- https://cloudconvert.com/wav-converter
- https://online-audio-converter.com/

**Settings to use**:
1. Upload your audio file
2. Format: **WAV**
3. Sample rate: **16000 Hz**
4. Channels: **Mono**
5. Bit depth: **16-bit**
6. Download and rename to correct filename

## Sound Suggestions

### work_start.wav
- **Purpose**: Signal beginning of focused work time
- **Tone**: Energetic, motivating
- **Examples**:
  - Ascending tones (do-mi-sol)
  - Bell sound
  - "Focus mode" voice
  - Positive chime

### rest_start.wav
- **Purpose**: Signal beginning of short break
- **Tone**: Relaxing, calming
- **Examples**:
  - Descending tones (sol-mi-do)
  - Wind chime
  - "Break time" voice
  - Soft bell

### long_rest_start.wav
- **Purpose**: Signal beginning of long break (after 4 work sessions)
- **Tone**: Celebratory, relaxing
- **Examples**:
  - Extended descending tones
  - Multiple chimes
  - "Long break earned!" voice
  - Celebratory sound

### warning.wav
- **Purpose**: Alert that session is ending in 30 seconds
- **Tone**: Gentle reminder, non-alarming
- **Examples**:
  - Double beep (beep... beep)
  - Soft tick-tock
  - "30 seconds remaining" voice
  - Short melody

## Free Sound Resources

### Royalty-Free Sound Libraries

1. **Freesound** (https://freesound.org/)
   - Huge library, Creative Commons licensed
   - Search: "chime", "bell", "beep", "notification"

2. **ZapSplat** (https://www.zapsplat.com/)
   - Free sound effects
   - No attribution required for personal use

3. **Sound Bible** (http://soundbible.com/)
   - Public domain and royalty-free sounds
   - Easy download, no signup

4. **BBC Sound Effects** (https://sound-effects.bbcrewind.co.uk/)
   - 16,000+ sound effects from BBC archive
   - Free for personal, educational, research use

### Text-to-Speech (Voice Announcements)

1. **Google TTS** (https://cloud.google.com/text-to-speech)
   - Free tier: 1 million characters/month
   - Natural-sounding voices

2. **Amazon Polly** (https://aws.amazon.com/polly/)
   - Free tier: 5 million characters/month
   - Multiple languages and voices

3. **Microsoft Azure TTS** (https://azure.microsoft.com/en-us/services/cognitive-services/text-to-speech/)
   - Free tier: 5 million characters/month
   - Neural voices available

## Verifying Audio Files

### Using FFmpeg
```bash
# Check file properties
ffmpeg -i work_start.wav

# Should show:
# Duration: 00:00:02.00 (or similar)
# Stream #0:0: Audio: pcm_s16le, 16000 Hz, 1 channels, s16, 256 kb/s
```

### Using Audacity
1. Open file in Audacity
2. Check bottom left corner: Should show **16000 Hz**
3. Check track header: Should show **Mono** (not Stereo)
4. **Edit** > **Preferences** > **Quality**: Should show **16-bit**

### Using MediaInfo (GUI)
**Download**: https://mediaarea.net/en/MediaInfo

Open file and verify:
- Format: PCM
- Bit rate mode: Constant
- Sampling rate: 16.0 kHz
- Bit depth: 16 bits
- Channel(s): 1 channel

## Testing

### On Device
1. Copy all 4 WAV files to SD card `/audio/` directory
2. Insert SD card into M5Stack Core2
3. Power on device
4. Check serial monitor (115200 baud) for:
   ```
   [AudioPlayer] Attempting to load audio from SD card...
   [AudioPlayer] Loaded /audio/work_start.wav (XXXX bytes) to PSRAM
   [AudioPlayer] Loaded /audio/rest_start.wav (XXXX bytes) to PSRAM
   [AudioPlayer] Loaded /audio/long_rest_start.wav (XXXX bytes) to PSRAM
   [AudioPlayer] Loaded /audio/warning.wav (XXXX bytes) to PSRAM
   [AudioPlayer] Loaded 4 audio files from SD (total: XXXXX bytes)
   [AudioPlayer] Audio source: SD_CARD
   ```
5. Start a timer and listen for custom sounds

### Fallback to FLASH
If audio files don't load (missing, wrong format, too large), device will show:
```
[AudioPlayer] Failed to load audio from SD card
[AudioPlayer] Falling back to FLASH audio
[AudioPlayer] Audio source: FLASH
```

Embedded FLASH audio is always available as backup (simple beep tones).

## Troubleshooting

### "Audio source: FLASH" instead of "SD_CARD"

**Possible causes**:
1. **Missing files** - All 4 files must be present
2. **Wrong directory** - Files must be in `/audio/` at SD root (not `/sd/audio/` or `/config/audio/`)
3. **Wrong file names** - Names are case-sensitive, must exactly match
4. **Wrong format** - Must be WAV 16kHz 16-bit mono
5. **Files too large** - Each file should be <100KB

### "Failed to load X.wav"

**Solutions**:
1. Check file exists: `dir E:\audio\` (Windows) or `ls /Volumes/SDCARD/audio/` (Mac)
2. Verify file size: Should be 5KB-100KB for 1-3 second audio
3. Re-export from Audacity with correct settings
4. Try simpler audio (shorter duration, no effects)

### Distorted or Garbled Audio

**Solutions**:
1. Verify sample rate is exactly 16000 Hz (not 16001 or 15999)
2. Ensure bit depth is 16-bit (not 8-bit or 24-bit)
3. Check file is not corrupted (try playing on PC first)
4. Reduce volume in Audacity before exporting

### Audio Cuts Off Early

**Solution**:
1. Check M5.Speaker volume setting (Settings screen)
2. Verify WAV file plays fully on PC
3. Reduce background noise/silence at end of file

## Memory Usage

Each audio file is loaded into PSRAM (or heap if PSRAM unavailable):

- **Typical sizes**:
  - 1 second @ 16kHz 16-bit mono = ~32KB
  - 2 seconds = ~64KB
  - 3 seconds = ~96KB

- **Total for 4 files** (2-second average):
  - 4 files × 64KB = **256KB** allocated from PSRAM

- **M5Core2 PSRAM**: 8MB total
  - Audio uses ~250KB (3% of PSRAM)
  - Plenty of space remaining for other features

## License Considerations

When using sounds from online libraries:

1. **Check license** - Some require attribution
2. **Personal use** - Usually free for personal projects
3. **Commercial use** - May require license or royalty payment
4. **Public domain** - Free to use without restrictions

**Recommended for M5 Pomodoro v2**:
- Use Creative Commons CC0 (public domain)
- Use your own recordings
- Use sounds from BBC, Freesound with proper license

**Do not use**:
- Copyrighted music or sounds without permission
- Sounds from movies, TV shows, games (usually copyrighted)

---

**Last Updated**: 2025-01-10
**Version**: 2.0.0
**Related**: MP-71 (AudioPlayer SD support)
