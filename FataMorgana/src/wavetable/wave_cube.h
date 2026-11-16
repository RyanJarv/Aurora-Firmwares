/** Wave Cube Data Structures
 *  8x8x8 wavetable cube for 3D morphing synthesis.
 *  
 *  File format: 16-bit WAV, 256 samples per waveform, 64 waveforms per file.
 *  8 files (1.wav - 8.wav) populate the Z-axis of the cube.
 */
#pragma once
#ifndef WAVE_CUBE_H
#define WAVE_CUBE_H

#include "daisy_seed.h"
#include "fatfs.h"
#include <cstring>
#include <cmath>

namespace wavetable
{

// Wavetable dimensions
static constexpr int SAMPLES_PER_WAVE = 256;
static constexpr int WAVES_PER_FILE = 64;      // 8x8 grid per file
static constexpr int CUBE_SIZE = 8;            // 8x8x8 cube
static constexpr int TOTAL_WAVES = CUBE_SIZE * CUBE_SIZE * CUBE_SIZE;  // 512

// Memory footprint: 256 samples * 4 bytes * 512 waves = 512KB
// Fits comfortably in SDRAM (64MB available)

/** Wave cube stored in SDRAM for fast access.
 *  Layout: [z][y][x][sample] where x,y,z are 0-7
 */
struct WaveCube
{
    float data[CUBE_SIZE][CUBE_SIZE][CUBE_SIZE][SAMPLES_PER_WAVE];
    bool loaded;
    
    void Clear()
    {
        memset(data, 0, sizeof(data));
        loaded = false;
    }
};

// Global wave cube instance (allocated in SDRAM)
extern WaveCube DSY_SDRAM_BSS wave_cube;

// DMA-aligned sector buffer for USB reads (must be in DMA-accessible memory)
static constexpr uint32_t SECTOR_SIZE = 512;
extern uint8_t DMA_BUFFER_MEM_SECTION g_sector_buf[SECTOR_SIZE];

/** Load status for USB operations */
enum class LoadStatus
{
    NOT_STARTED,
    LOADING,
    SUCCESS,
    ERROR_NO_DRIVE,
    ERROR_FILE_NOT_FOUND,
    ERROR_READ_FAILED,
    ERROR_NOT_RIFF,           // File doesn't start with RIFF
    ERROR_NOT_WAVE,           // Not a WAVE file
    ERROR_NO_FMT_CHUNK,       // Missing fmt chunk
    ERROR_NO_DATA_CHUNK,      // Missing data chunk
    ERROR_NOT_PCM,            // Not PCM format (compressed audio)
    ERROR_NOT_MONO,           // Not mono (stereo files not supported)
    ERROR_NOT_16BIT,          // Not 16-bit audio
    ERROR_DATA_TOO_SMALL      // Data chunk too small for 64 waveforms
};

/** WAV chunk header */
struct ChunkHeader
{
    char     id[4];
    uint32_t size;
};

// ============================================================================
// Logging (must be before LoadWavFile)
// ============================================================================

// FatFs transfers the private sector buffer in FIL directly over USB.
static FIL DMA_BUFFER_MEM_SECTION g_log_file;
static FIL DMA_BUFFER_MEM_SECTION g_wav_file;
static bool g_log_open = false;

/** Build log file path */
inline void BuildLogPath(const char* usb_path, char* log_path)
{
    int i = 0;
    const char* p = usb_path;
    while (*p && i < 48) log_path[i++] = *p++;
    const char* log_name = "wavetable_log.txt";
    p = log_name;
    while (*p && i < 63) log_path[i++] = *p++;
    log_path[i] = '\0';
}

/** Write a line to the log (if open) */
inline void WriteLog(const char* text)
{
    if (!g_log_open) return;
    
    UINT written;
    f_write(&g_log_file, text, strlen(text), &written);
    f_sync(&g_log_file);
}

/** Open log file for writing */
inline bool OpenLog(const char* usb_path)
{
    if (g_log_open) return true;
    
    char log_path[64];
    BuildLogPath(usb_path, log_path);
    
    FRESULT res = f_open(&g_log_file, log_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res == FR_OK)
    {
        g_log_open = true;
        
        // Write header
        UINT written;
        const char* header = "=== FataMorgana Wavetable Loading Log ===\r\n";
        f_write(&g_log_file, header, strlen(header), &written);
        const char* format = "Required format: 16-bit mono PCM WAV, 16384 samples\r\n\r\n";
        f_write(&g_log_file, format, strlen(format), &written);
        
        // Log the USB path being used
        const char* path_msg = "USB path: ";
        f_write(&g_log_file, path_msg, strlen(path_msg), &written);
        f_write(&g_log_file, usb_path, strlen(usb_path), &written);
        f_write(&g_log_file, "\r\n\r\n", 4, &written);
        
        f_sync(&g_log_file);
        return true;
    }
    return false;
}

/** Close log file */
inline void CloseLog()
{
    if (g_log_open)
    {
        f_close(&g_log_file);
        g_log_open = false;
    }
}

/** WAV format chunk data (minimum 16 bytes) */
struct FmtChunk
{
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;   // 1 = mono
    uint32_t sample_rate;    // Sample rate (44100, 48000, etc.)
    uint32_t byte_rate;      // Bytes per second
    uint16_t block_align;    // Bytes per sample * channels
    uint16_t bits_per_sample;// 16 for this format
};

/** Load a single WAV file into a Z-slice of the cube.
 *  Uses sector-aligned reads for USB DMA compatibility.
 *  @param z_index Z-axis index (0-7)
 *  @param filename Path to WAV file (e.g., "1.wav")
 *  @return LoadStatus indicating success or error type
 */
inline LoadStatus LoadWavFile(int z_index, const char* filename)
{
    if (z_index < 0 || z_index >= CUBE_SIZE) return LoadStatus::ERROR_READ_FAILED;
    
    WriteLog("  Opening file...\r\n");
    
    // Files are loaded sequentially. Reuse a DMA-accessible file sector buffer.
    FIL& file = g_wav_file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK)
    {
        WriteLog("  ERROR: f_open failed\r\n");
        return LoadStatus::ERROR_FILE_NOT_FOUND;
    }
    
    WriteLog("  Reading header...\r\n");
    
    UINT bytes_read;
    
    // Read first sector containing WAV header
    res = f_read(&file, g_sector_buf, SECTOR_SIZE, &bytes_read);
    if (res != FR_OK || bytes_read != SECTOR_SIZE)
    {
        WriteLog("  ERROR: Failed to read header sector\r\n");
        f_close(&file);
        return LoadStatus::ERROR_READ_FAILED;
    }
    
    // Validate RIFF header
    if (memcmp(g_sector_buf, "RIFF", 4) != 0)
    {
        WriteLog("  ERROR: Not RIFF\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NOT_RIFF;
    }
    
    if (memcmp(g_sector_buf + 8, "WAVE", 4) != 0)
    {
        WriteLog("  ERROR: Not WAVE\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NOT_WAVE;
    }
    
    WriteLog("  Scanning chunks...\r\n");
    
    // Parse chunks from the header sector
    // Standard WAV: RIFF(4) + size(4) + WAVE(4) + fmt chunk + data chunk
    // fmt chunk typically at offset 12, data chunk follows
    
    uint32_t offset = 12;  // Start after "RIFF....WAVE"
    bool fmt_found = false;
    bool data_found = false;
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
    
    uint16_t audio_format = 0;
    uint16_t num_channels = 0;
    uint16_t bits_per_sample = 0;
    
    // Scan chunks within the first sector
    while (offset < SECTOR_SIZE - 8 && !data_found)
    {
        // Read chunk ID and size
        char chunk_id[4];
        memcpy(chunk_id, g_sector_buf + offset, 4);
        uint32_t chunk_size = g_sector_buf[offset + 4] | 
                              (g_sector_buf[offset + 5] << 8) |
                              (g_sector_buf[offset + 6] << 16) |
                              (g_sector_buf[offset + 7] << 24);
        
        if (memcmp(chunk_id, "fmt ", 4) == 0)
        {
            WriteLog("  Found fmt chunk\r\n");
            // Parse format data (starts at offset + 8)
            uint32_t fmt_offset = offset + 8;
            audio_format = g_sector_buf[fmt_offset] | (g_sector_buf[fmt_offset + 1] << 8);
            num_channels = g_sector_buf[fmt_offset + 2] | (g_sector_buf[fmt_offset + 3] << 8);
            bits_per_sample = g_sector_buf[fmt_offset + 14] | (g_sector_buf[fmt_offset + 15] << 8);
            fmt_found = true;
            offset += 8 + chunk_size;
        }
        else if (memcmp(chunk_id, "data", 4) == 0)
        {
            WriteLog("  Found data chunk, size=");
            data_size = chunk_size;
            data_offset = offset + 8;
            data_found = true;
            
            // Log data_size
            char sz_buf[16];
            uint32_t nn = data_size;
            int ii = 0;
            if (nn == 0) sz_buf[ii++] = '0';
            else { char tmp[16]; int t = 0; while (nn > 0) { tmp[t++] = '0' + (nn % 10); nn /= 10; } while (t > 0) sz_buf[ii++] = tmp[--t]; }
            sz_buf[ii] = '\0';
            WriteLog(sz_buf);
            WriteLog("\r\n");
        }
        else
        {
            // Skip unknown chunk
            offset += 8 + chunk_size;
            // Word-align
            if (chunk_size & 1) offset++;
        }
    }
    
    // If data chunk not in first sector, need to seek and read more
    // For standard WaveEdit files, data starts at offset 44 (within first sector)
    if (!data_found)
    {
        // Fallback: assume standard 44-byte header
        if (g_sector_buf[36] == 'd' && g_sector_buf[37] == 'a' &&
            g_sector_buf[38] == 't' && g_sector_buf[39] == 'a')
        {
            data_size = g_sector_buf[40] | (g_sector_buf[41] << 8) |
                        (g_sector_buf[42] << 16) | (g_sector_buf[43] << 24);
            data_offset = 44;
            data_found = true;
            
            // Also grab fmt if not found
            if (!fmt_found && g_sector_buf[12] == 'f' && g_sector_buf[13] == 'm' &&
                g_sector_buf[14] == 't' && g_sector_buf[15] == ' ')
            {
                audio_format = g_sector_buf[20] | (g_sector_buf[21] << 8);
                num_channels = g_sector_buf[22] | (g_sector_buf[23] << 8);
                bits_per_sample = g_sector_buf[34] | (g_sector_buf[35] << 8);
                fmt_found = true;
            }
        }
    }
    
    // Validate format
    if (!fmt_found)
    {
        WriteLog("  ERROR: No fmt chunk found\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NO_FMT_CHUNK;
    }
    if (!data_found)
    {
        WriteLog("  ERROR: No data chunk found\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NO_DATA_CHUNK;
    }
    
    WriteLog("  Validating format...\r\n");
    
    if (audio_format != 1)
    {
        WriteLog("  ERROR: Not PCM\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NOT_PCM;
    }
    if (num_channels != 1)
    {
        WriteLog("  ERROR: Not mono\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NOT_MONO;
    }
    if (bits_per_sample != 16)
    {
        WriteLog("  ERROR: Not 16-bit\r\n");
        f_close(&file);
        return LoadStatus::ERROR_NOT_16BIT;
    }
    
    // Check data size: need at least 64 waveforms * 256 samples * 2 bytes
    uint32_t required_bytes = WAVES_PER_FILE * SAMPLES_PER_WAVE * sizeof(int16_t);
    if (data_size < required_bytes)
    {
        WriteLog("  ERROR: Data too small\r\n");
        f_close(&file);
        return LoadStatus::ERROR_DATA_TOO_SMALL;
    }
    
    WriteLog("  Reading samples...\r\n");
    
    // Read samples into the wave_cube data array
    // The memory layout [z][y][x][sample] is contiguous, so we can write directly
    uint32_t total_samples_needed = WAVES_PER_FILE * SAMPLES_PER_WAVE;  // 16384
    uint32_t samples_read = 0;
    float* flat_buf = &wave_cube.data[z_index][0][0][0];
    
    // Process samples from first sector (after header)
    uint32_t samples_in_first_sector = (SECTOR_SIZE - data_offset) / 2;
    
    if (data_offset < SECTOR_SIZE && samples_in_first_sector > 0)
    {
        const int16_t* first_samples = reinterpret_cast<const int16_t*>(g_sector_buf + data_offset);
        uint32_t to_copy = samples_in_first_sector;
        if (to_copy > total_samples_needed) to_copy = total_samples_needed;
        
        for (uint32_t i = 0; i < to_copy; i++)
        {
            flat_buf[i] = static_cast<float>(first_samples[i]) / 32768.f;
        }
        samples_read = to_copy;
    }
    
    // Get actual file size to determine how many full sectors we can read
    // USB DMA requires sector-aligned reads - partial sector reads hang
    uint32_t file_size = f_size(&file);
    uint32_t bytes_after_header = (file_size > SECTOR_SIZE) ? (file_size - SECTOR_SIZE) : 0;
    uint32_t full_sectors_remaining = bytes_after_header / SECTOR_SIZE;
    
    // Read only full 512-byte sectors
    for (uint32_t sector = 0; sector < full_sectors_remaining && samples_read < total_samples_needed; sector++)
    {
        res = f_read(&file, g_sector_buf, SECTOR_SIZE, &bytes_read);
        
        if (res != FR_OK || bytes_read != SECTOR_SIZE)
        {
            WriteLog("  ERROR: sector read failed\r\n");
            f_close(&file);
            return LoadStatus::ERROR_READ_FAILED;
        }
        
        const int16_t* samples = reinterpret_cast<const int16_t*>(g_sector_buf);
        uint32_t samples_in_sector = SECTOR_SIZE / 2;  // 256 samples per sector
        uint32_t remaining = total_samples_needed - samples_read;
        if (samples_in_sector > remaining) samples_in_sector = remaining;
        
        for (uint32_t i = 0; i < samples_in_sector; i++)
        {
            flat_buf[samples_read + i] = static_cast<float>(samples[i]) / 32768.f;
        }
        samples_read += samples_in_sector;
    }
    
    // Note: We skip the final partial sector (USB DMA cannot read non-sector-aligned)
    // This loses at most 255 samples (1.5%) which is acceptable
    // The data is already in the right place since we wrote to flat_buf which points to data[z][0][0][0]
    
    WriteLog("  Done.\r\n");
    f_close(&file);
    return LoadStatus::SUCCESS;
}

/** Get human-readable error message for LoadStatus */
inline const char* GetStatusMessage(LoadStatus status)
{
    switch (status)
    {
        case LoadStatus::SUCCESS:           return "OK";
        case LoadStatus::NOT_STARTED:       return "Not started";
        case LoadStatus::LOADING:           return "Loading";
        case LoadStatus::ERROR_NO_DRIVE:    return "No USB drive detected";
        case LoadStatus::ERROR_FILE_NOT_FOUND: return "File not found";
        case LoadStatus::ERROR_READ_FAILED: return "Read failed";
        case LoadStatus::ERROR_NOT_RIFF:    return "Not a valid file (missing RIFF header)";
        case LoadStatus::ERROR_NOT_WAVE:    return "Not a WAV file (missing WAVE marker)";
        case LoadStatus::ERROR_NO_FMT_CHUNK: return "Invalid WAV (missing fmt chunk)";
        case LoadStatus::ERROR_NO_DATA_CHUNK: return "Invalid WAV (missing data chunk)";
        case LoadStatus::ERROR_NOT_PCM:     return "Compressed audio not supported (must be PCM)";
        case LoadStatus::ERROR_NOT_MONO:    return "Stereo not supported (must be mono)";
        case LoadStatus::ERROR_NOT_16BIT:   return "Wrong bit depth (must be 16-bit)";
        case LoadStatus::ERROR_DATA_TOO_SMALL: return "File too short (need 16384 samples)";
        default:                            return "Unknown error";
    }
}

/** Write loading result to log file */
inline void WriteLogEntry(const char* filename, LoadStatus status)
{
    if (!g_log_open) return;
    
    UINT written;
    f_write(&g_log_file, filename, strlen(filename), &written);
    f_write(&g_log_file, ": ", 2, &written);
    const char* msg = GetStatusMessage(status);
    f_write(&g_log_file, msg, strlen(msg), &written);
    f_write(&g_log_file, "\r\n", 2, &written);
    f_sync(&g_log_file);
}

/** Test if USB filesystem is accessible by trying to open a file */
inline bool TestFilesystemAccess(const char* usb_path)
{
    // Build test path for 1.wav
    char test_path[32];
    int i = 0;
    const char* p = usb_path;
    while (*p && i < 28) test_path[i++] = *p++;
    test_path[i++] = '1';
    test_path[i++] = '.';
    test_path[i++] = 'w';
    test_path[i++] = 'a';
    test_path[i++] = 'v';
    test_path[i] = '\0';
    
    // Try to open the file
    FIL file;
    FRESULT res = f_open(&file, test_path, FA_READ);
    if (res == FR_OK)
    {
        f_close(&file);
        return true;
    }
    return false;
}

/** Load all 8 WAV files from USB drive.
 *  Expects files named "1.wav" through "8.wav" in root directory.
 *  Writes results to wavetable_log.txt on the USB drive.
 *  @param usb_path USB drive path from FatFSInterface::GetUSBPath() (e.g., "1:/")
 *  @return LoadStatus indicating overall success or first error encountered
 */
inline LoadStatus LoadWaveCube(const char* usb_path = "1:/")
{
    wave_cube.Clear();
    
    // Test filesystem access first
    if (!TestFilesystemAccess(usb_path))
    {
        // Try alternate path
        const char* alt_path = (usb_path[0] == '0') ? "1:/" : "0:/";
        if (TestFilesystemAccess(alt_path))
        {
            usb_path = alt_path;
        }
        else
        {
            return LoadStatus::ERROR_NO_DRIVE;
        }
    }
    
    // Open log file
    if (!OpenLog(usb_path))
    {
        // Continue without logging if log file can't be opened
    }
    
    WriteLog("Starting wavetable load...\r\n\r\n");
    
    char filename[32];
    LoadStatus first_error = LoadStatus::SUCCESS;
    
    for (int z = 0; z < CUBE_SIZE; z++)
    {
        // Build full path: "0:/1.wav", "0:/2.wav", etc.
        int i = 0;
        
        // Copy USB path prefix
        const char* p = usb_path;
        while (*p && i < 28)
        {
            filename[i++] = *p++;
        }
        
        // Append filename
        filename[i++] = '1' + z;
        filename[i++] = '.';
        filename[i++] = 'w';
        filename[i++] = 'a';
        filename[i++] = 'v';
        filename[i] = '\0';
        
        // Log which file we're loading
        char short_name[8];
        short_name[0] = '1' + z;
        short_name[1] = '.';
        short_name[2] = 'w';
        short_name[3] = 'a';
        short_name[4] = 'v';
        short_name[5] = '\0';
        
        WriteLog("Loading ");
        WriteLog(short_name);
        WriteLog("...\r\n");
        
        LoadStatus status = LoadWavFile(z, filename);
        
        // Log result for this file
        WriteLogEntry(short_name, status);
        
        if (status != LoadStatus::SUCCESS && first_error == LoadStatus::SUCCESS)
        {
            first_error = status;
        }
    }
    
    // Write summary
    WriteLog("\r\n");
    if (first_error == LoadStatus::SUCCESS)
    {
        WriteLog("Result: SUCCESS\r\n");
        wave_cube.loaded = true;
    }
    else
    {
        WriteLog("Result: FAILED - ");
        WriteLog(GetStatusMessage(first_error));
        WriteLog("\r\n");
    }
    
    CloseLog();
    
    return first_error;
}

/** Generate default sine wave cube for testing when no USB files present.
 *  Creates harmonically related waveforms across the cube.
 */
inline void GenerateDefaultCube()
{
    wave_cube.Clear();
    
    for (int z = 0; z < CUBE_SIZE; z++)
    {
        for (int y = 0; y < CUBE_SIZE; y++)
        {
            for (int x = 0; x < CUBE_SIZE; x++)
            {
                // Harmonic content increases with x, y, z
                int harmonics = 1 + x + y + z;
                
                for (int s = 0; s < SAMPLES_PER_WAVE; s++)
                {
                    float phase = static_cast<float>(s) / SAMPLES_PER_WAVE;
                    float sample = 0.f;
                    
                    // Additive synthesis with decreasing amplitude per harmonic
                    for (int h = 1; h <= harmonics && h <= 16; h++)
                    {
                        float amp = 1.f / static_cast<float>(h);
                        sample += amp * sinf(2.f * 3.14159265f * phase * h);
                    }
                    
                    // Normalize
                    wave_cube.data[z][y][x][s] = sample * 0.5f;
                }
            }
        }
    }
    
    wave_cube.loaded = true;
}

/** Trilinear interpolation to read from the wave cube.
 *  @param x X-axis position (0.0 to 7.0)
 *  @param y Y-axis position (0.0 to 7.0)
 *  @param z Z-axis position (0.0 to 7.0)
 *  @param phase Phase position within waveform (0.0 to 1.0)
 *  @param morph_x Enable X-axis interpolation (false = nearest neighbor)
 *  @param morph_y Enable Y-axis interpolation (false = nearest neighbor)
 *  @param morph_z Enable Z-axis interpolation (false = nearest neighbor)
 *  @return Interpolated sample value
 */
inline float ReadCube(float x, float y, float z, float phase,
                      bool morph_x = true, bool morph_y = true, bool morph_z = true)
{
    // Clamp coordinates to valid range
    x = fmaxf(0.f, fminf(x, 7.f));
    y = fmaxf(0.f, fminf(y, 7.f));
    z = fmaxf(0.f, fminf(z, 7.f));
    
    // Get integer indices and fractional parts
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int z0 = static_cast<int>(z);
    
    float fx = morph_x ? (x - x0) : 0.f;
    float fy = morph_y ? (y - y0) : 0.f;
    float fz = morph_z ? (z - z0) : 0.f;
    
    // Clamp upper indices
    int x1 = (x0 < 7) ? x0 + 1 : x0;
    int y1 = (y0 < 7) ? y0 + 1 : y0;
    int z1 = (z0 < 7) ? z0 + 1 : z0;
    
    // Calculate sample index with linear interpolation
    float sample_pos = phase * (SAMPLES_PER_WAVE - 1);
    int s0 = static_cast<int>(sample_pos);
    int s1 = (s0 < SAMPLES_PER_WAVE - 1) ? s0 + 1 : 0;  // Wrap for seamless looping
    float fs = sample_pos - s0;
    
    // Fetch 8 corner samples (with sample interpolation)
    auto sample_interp = [&](int xi, int yi, int zi) -> float {
        float v0 = wave_cube.data[zi][yi][xi][s0];
        float v1 = wave_cube.data[zi][yi][xi][s1];
        return v0 + fs * (v1 - v0);
    };
    
    // Trilinear interpolation
    float c000 = sample_interp(x0, y0, z0);
    float c100 = sample_interp(x1, y0, z0);
    float c010 = sample_interp(x0, y1, z0);
    float c110 = sample_interp(x1, y1, z0);
    float c001 = sample_interp(x0, y0, z1);
    float c101 = sample_interp(x1, y0, z1);
    float c011 = sample_interp(x0, y1, z1);
    float c111 = sample_interp(x1, y1, z1);
    
    // Interpolate along X
    float c00 = c000 + fx * (c100 - c000);
    float c10 = c010 + fx * (c110 - c010);
    float c01 = c001 + fx * (c101 - c001);
    float c11 = c011 + fx * (c111 - c011);
    
    // Interpolate along Y
    float c0 = c00 + fy * (c10 - c00);
    float c1 = c01 + fy * (c11 - c01);
    
    // Interpolate along Z
    return c0 + fz * (c1 - c0);
}

}  // namespace wavetable

#endif  // WAVE_CUBE_H
