/**
 * WAV Parser Unit Tests
 * 
 * Tests the WAV file parsing logic used by FataMorgana wavetable loader.
 * Runs on host system (not embedded) to validate parsing before deployment.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>

// ============================================================================
// WAV Parser (extracted from wave_cube.h for testability)
// ============================================================================

namespace wav_parser {

struct WavInfo {
    bool valid = false;
    uint16_t audio_format = 0;    // 1 = PCM
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    uint32_t data_offset = 0;     // Byte offset where audio data starts
    uint32_t data_size = 0;       // Size of audio data in bytes (from header)
    uint32_t file_size = 0;       // Actual file size
    std::string error;
};

// Parse WAV header from buffer (first 512 bytes of file)
WavInfo ParseWavHeader(const uint8_t* buf, size_t buf_size, uint32_t file_size) {
    WavInfo info;
    info.file_size = file_size;
    
    if (buf_size < 44) {
        info.error = "Buffer too small for WAV header";
        return info;
    }
    
    // Check RIFF header
    if (memcmp(buf, "RIFF", 4) != 0) {
        info.error = "Not a RIFF file";
        return info;
    }
    
    // Check WAVE format
    if (memcmp(buf + 8, "WAVE", 4) != 0) {
        info.error = "Not a WAVE file";
        return info;
    }
    
    // Scan chunks
    uint32_t offset = 12;  // Start after "RIFF....WAVE"
    bool fmt_found = false;
    bool data_found = false;
    
    while (offset < buf_size - 8 && !data_found) {
        char chunk_id[5] = {0};
        memcpy(chunk_id, buf + offset, 4);
        
        uint32_t chunk_size = buf[offset + 4] | 
                              (buf[offset + 5] << 8) |
                              (buf[offset + 6] << 16) |
                              (buf[offset + 7] << 24);
        
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            uint32_t fmt_offset = offset + 8;
            info.audio_format = buf[fmt_offset] | (buf[fmt_offset + 1] << 8);
            info.num_channels = buf[fmt_offset + 2] | (buf[fmt_offset + 3] << 8);
            info.sample_rate = buf[fmt_offset + 4] | (buf[fmt_offset + 5] << 8) |
                               (buf[fmt_offset + 6] << 16) | (buf[fmt_offset + 7] << 24);
            info.bits_per_sample = buf[fmt_offset + 14] | (buf[fmt_offset + 15] << 8);
            fmt_found = true;
            offset += 8 + chunk_size;
        }
        else if (memcmp(chunk_id, "data", 4) == 0) {
            info.data_size = chunk_size;
            info.data_offset = offset + 8;
            data_found = true;
        }
        else {
            // Skip unknown chunk
            offset += 8 + chunk_size;
            // Word-align
            if (chunk_size & 1) offset++;
        }
    }
    
    // Fallback for standard 44-byte header
    if (!data_found) {
        if (buf[36] == 'd' && buf[37] == 'a' && buf[38] == 't' && buf[39] == 'a') {
            info.data_size = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);
            info.data_offset = 44;
            data_found = true;
        }
        
        if (!fmt_found && buf[12] == 'f' && buf[13] == 'm' && buf[14] == 't' && buf[15] == ' ') {
            info.audio_format = buf[20] | (buf[21] << 8);
            info.num_channels = buf[22] | (buf[23] << 8);
            info.sample_rate = buf[24] | (buf[25] << 8) | (buf[26] << 16) | (buf[27] << 24);
            info.bits_per_sample = buf[34] | (buf[35] << 8);
            fmt_found = true;
        }
    }
    
    if (!fmt_found) {
        info.error = "No fmt chunk found";
        return info;
    }
    if (!data_found) {
        info.error = "No data chunk found";
        return info;
    }
    
    info.valid = true;
    return info;
}

// Calculate actual usable data bytes (handles incorrect WAV headers)
uint32_t GetUsableDataBytes(const WavInfo& info) {
    // Actual bytes available in file after data_offset
    uint32_t actual_data_bytes = 0;
    if (info.file_size > info.data_offset) {
        actual_data_bytes = info.file_size - info.data_offset;
    }
    
    // Use the smaller of: header data_size or actual file data
    uint32_t usable = actual_data_bytes;
    if (info.data_size < usable) {
        usable = info.data_size;
    }
    
    return usable;
}

// Calculate number of samples
uint32_t GetNumSamples(const WavInfo& info) {
    uint32_t usable_bytes = GetUsableDataBytes(info);
    uint32_t bytes_per_sample = info.bits_per_sample / 8;
    if (bytes_per_sample == 0) return 0;
    return usable_bytes / bytes_per_sample;
}

// Calculate how many sector reads are needed
struct ReadPlan {
    uint32_t samples_in_first_sector;  // Samples available in first 512-byte sector
    uint32_t bytes_remaining;          // Bytes to read after first sector
    uint32_t sectors_to_read;          // Number of additional sector reads
    uint32_t total_samples;            // Total samples available
};

ReadPlan CalculateReadPlan(const WavInfo& info, uint32_t sector_size = 512) {
    ReadPlan plan = {0, 0, 0, 0};
    
    if (!info.valid) return plan;
    
    uint32_t usable_bytes = GetUsableDataBytes(info);
    uint32_t bytes_per_sample = info.bits_per_sample / 8;
    if (bytes_per_sample == 0) return plan;
    
    // Samples in first sector (after header)
    uint32_t bytes_in_first_sector = 0;
    if (sector_size > info.data_offset) {
        bytes_in_first_sector = sector_size - info.data_offset;
    }
    if (bytes_in_first_sector > usable_bytes) {
        bytes_in_first_sector = usable_bytes;
    }
    plan.samples_in_first_sector = bytes_in_first_sector / bytes_per_sample;
    
    // Remaining bytes after first sector
    if (usable_bytes > bytes_in_first_sector) {
        plan.bytes_remaining = usable_bytes - bytes_in_first_sector;
    }
    
    // Sectors needed for remaining data
    plan.sectors_to_read = (plan.bytes_remaining + sector_size - 1) / sector_size;
    
    // Total samples
    plan.total_samples = usable_bytes / bytes_per_sample;
    
    return plan;
}

// Simulate the read loop to check for EOF issues
struct SimulationResult {
    bool success = false;
    uint32_t samples_read = 0;
    uint32_t sectors_read = 0;
    uint32_t file_pos_at_end = 0;
    bool would_read_past_eof = false;
    std::string error;
};

SimulationResult SimulateReadLoop(const WavInfo& info, uint32_t target_samples, uint32_t sector_size = 512) {
    SimulationResult result;
    
    if (!info.valid) {
        result.error = "Invalid WAV info";
        return result;
    }
    
    ReadPlan plan = CalculateReadPlan(info, sector_size);
    
    // Start after reading first sector (header)
    uint32_t file_pos = sector_size;
    uint32_t samples_read = plan.samples_in_first_sector;
    uint32_t sectors_read = 0;
    
    // Simulate the read loop
    while (samples_read < target_samples) {
        // Check if we would read past EOF
        if (file_pos >= info.file_size) {
            // This is expected - we've read all available data
            break;
        }
        
        // Check if trying to read would go past EOF
        if (file_pos + sector_size > info.file_size) {
            // Partial sector read - this is the last read
            uint32_t bytes_available = info.file_size - file_pos;
            uint32_t samples_in_sector = bytes_available / (info.bits_per_sample / 8);
            samples_read += samples_in_sector;
            file_pos += bytes_available;
            sectors_read++;
            break;
        }
        
        // Full sector read
        uint32_t samples_in_sector = sector_size / (info.bits_per_sample / 8);
        uint32_t remaining = target_samples - samples_read;
        if (samples_in_sector > remaining) {
            samples_in_sector = remaining;
        }
        
        samples_read += samples_in_sector;
        file_pos += sector_size;
        sectors_read++;
    }
    
    result.success = true;
    result.samples_read = samples_read;
    result.sectors_read = sectors_read;
    result.file_pos_at_end = file_pos;
    result.would_read_past_eof = (file_pos > info.file_size);
    
    return result;
}

}  // namespace wav_parser

// ============================================================================
// Test Fixtures
// ============================================================================

class WavParserTest : public ::testing::Test {
protected:
    std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return {};
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        
        return buffer;
    }
    
    std::string GetTestWavPath(int index) {
        return "../" + std::to_string(index) + ".wav";
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST_F(WavParserTest, ParseValidWavHeader) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty()) << "Could not read 1.wav";
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    
    EXPECT_TRUE(info.valid) << "Parse error: " << info.error;
    EXPECT_EQ(info.audio_format, 1) << "Expected PCM format";
    EXPECT_EQ(info.num_channels, 1) << "Expected mono";
    EXPECT_EQ(info.bits_per_sample, 16) << "Expected 16-bit";
    EXPECT_EQ(info.data_offset, 44) << "Expected standard header size";
    EXPECT_EQ(info.file_size, 32812) << "Unexpected file size";
}

TEST_F(WavParserTest, DetectIncorrectDataSize) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty());
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    ASSERT_TRUE(info.valid);
    
    // The WAV header claims 131072 bytes of data, but file only has 32768
    EXPECT_EQ(info.data_size, 131072) << "Header data_size should be 131072 (incorrect)";
    
    // But actual usable data should be based on file size
    uint32_t usable = wav_parser::GetUsableDataBytes(info);
    EXPECT_EQ(usable, 32768) << "Usable data should be 32768 (file_size - header)";
}

TEST_F(WavParserTest, CalculateCorrectSampleCount) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty());
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    ASSERT_TRUE(info.valid);
    
    uint32_t num_samples = wav_parser::GetNumSamples(info);
    EXPECT_EQ(num_samples, 16384) << "Expected 16384 samples (64 waveforms * 256 samples)";
}

TEST_F(WavParserTest, CalculateReadPlan) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty());
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    ASSERT_TRUE(info.valid);
    
    auto plan = wav_parser::CalculateReadPlan(info);
    
    // First sector: 512 bytes, header is 44 bytes, so 468 bytes = 234 samples
    EXPECT_EQ(plan.samples_in_first_sector, 234) << "First sector should have 234 samples";
    
    // Remaining: 32768 - 468 = 32300 bytes
    EXPECT_EQ(plan.bytes_remaining, 32300) << "Should have 32300 bytes remaining";
    
    // Sectors needed: ceil(32300 / 512) = 64 sectors
    EXPECT_EQ(plan.sectors_to_read, 64) << "Should need 64 more sector reads";
    
    // Total samples
    EXPECT_EQ(plan.total_samples, 16384) << "Total should be 16384 samples";
}

TEST_F(WavParserTest, SimulateReadLoopNoEOFOverrun) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty());
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    ASSERT_TRUE(info.valid);
    
    // Simulate reading 16384 samples (what the firmware needs)
    auto result = wav_parser::SimulateReadLoop(info, 16384);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.samples_read, 16384) << "Should read exactly 16384 samples";
    EXPECT_FALSE(result.would_read_past_eof) << "Should not read past EOF";
    EXPECT_LE(result.file_pos_at_end, info.file_size) << "File position should not exceed file size";
    
    std::cout << "Simulation results:" << std::endl;
    std::cout << "  Samples read: " << result.samples_read << std::endl;
    std::cout << "  Sectors read: " << result.sectors_read << std::endl;
    std::cout << "  Final file pos: " << result.file_pos_at_end << std::endl;
    std::cout << "  File size: " << info.file_size << std::endl;
}

TEST_F(WavParserTest, AllWavFilesValid) {
    for (int i = 1; i <= 8; i++) {
        auto data = ReadFile(GetTestWavPath(i));
        ASSERT_FALSE(data.empty()) << "Could not read " << i << ".wav";
        
        auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
        
        EXPECT_TRUE(info.valid) << i << ".wav parse error: " << info.error;
        EXPECT_EQ(info.audio_format, 1) << i << ".wav: Expected PCM";
        EXPECT_EQ(info.num_channels, 1) << i << ".wav: Expected mono";
        EXPECT_EQ(info.bits_per_sample, 16) << i << ".wav: Expected 16-bit";
        
        uint32_t num_samples = wav_parser::GetNumSamples(info);
        EXPECT_EQ(num_samples, 16384) << i << ".wav: Expected 16384 samples";
        
        auto result = wav_parser::SimulateReadLoop(info, 16384);
        EXPECT_TRUE(result.success) << i << ".wav: Simulation failed";
        EXPECT_FALSE(result.would_read_past_eof) << i << ".wav: Would read past EOF";
    }
}

TEST_F(WavParserTest, FilePositionTracking) {
    auto data = ReadFile(GetTestWavPath(1));
    ASSERT_FALSE(data.empty());
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    ASSERT_TRUE(info.valid);
    
    // Detailed position tracking
    uint32_t sector_size = 512;
    uint32_t file_pos = sector_size;  // After reading header sector
    uint32_t samples_read = 234;      // From first sector
    uint32_t target_samples = 16384;
    int sector_count = 0;
    
    std::cout << "Position tracking:" << std::endl;
    std::cout << "  Initial file_pos: " << file_pos << std::endl;
    std::cout << "  Initial samples: " << samples_read << std::endl;
    std::cout << "  File size: " << info.file_size << std::endl;
    
    while (samples_read < target_samples) {
        // This is the check that should prevent reading past EOF
        if (file_pos >= info.file_size) {
            std::cout << "  Stopping: file_pos >= file_size" << std::endl;
            break;
        }
        
        sector_count++;
        uint32_t bytes_to_read = sector_size;
        uint32_t bytes_available = info.file_size - file_pos;
        
        if (bytes_available < sector_size) {
            bytes_to_read = bytes_available;
            std::cout << "  Sector " << sector_count << ": partial read of " << bytes_to_read << " bytes" << std::endl;
        }
        
        uint32_t samples_in_read = bytes_to_read / 2;
        samples_read += samples_in_read;
        file_pos += bytes_to_read;
        
        if (sector_count <= 5 || sector_count >= 62) {
            std::cout << "  Sector " << sector_count << ": file_pos=" << file_pos 
                      << ", samples=" << samples_read << std::endl;
        } else if (sector_count == 6) {
            std::cout << "  ..." << std::endl;
        }
    }
    
    std::cout << "  Final: " << sector_count << " sectors, " << samples_read << " samples" << std::endl;
    
    EXPECT_EQ(samples_read, 16384);
    EXPECT_LE(file_pos, info.file_size);
}

TEST_F(WavParserTest, CorrectHeaderWavFile) {
    // wavetable.wav has a correct header (data_size matches actual data)
    auto data = ReadFile("../wavetable.wav");
    ASSERT_FALSE(data.empty()) << "Could not read wavetable.wav";
    
    auto info = wav_parser::ParseWavHeader(data.data(), std::min(data.size(), size_t(512)), data.size());
    
    EXPECT_TRUE(info.valid) << "Parse error: " << info.error;
    EXPECT_EQ(info.audio_format, 1) << "Expected PCM format";
    EXPECT_EQ(info.num_channels, 1) << "Expected mono";
    EXPECT_EQ(info.bits_per_sample, 16) << "Expected 16-bit";
    EXPECT_EQ(info.data_offset, 44) << "Expected standard header size";
    EXPECT_EQ(info.file_size, 32812) << "Unexpected file size";
    
    // This file should have CORRECT data_size in header
    EXPECT_EQ(info.data_size, 32768) << "Header data_size should be 32768 (correct)";
    
    // Usable data should match header
    uint32_t usable = wav_parser::GetUsableDataBytes(info);
    EXPECT_EQ(usable, 32768) << "Usable data should be 32768";
    
    // Sample count
    uint32_t num_samples = wav_parser::GetNumSamples(info);
    EXPECT_EQ(num_samples, 16384) << "Expected 16384 samples";
    
    // Simulation should work
    auto result = wav_parser::SimulateReadLoop(info, 16384);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.samples_read, 16384);
    EXPECT_FALSE(result.would_read_past_eof);
    
    std::cout << "wavetable.wav (correct header):" << std::endl;
    std::cout << "  data_size in header: " << info.data_size << std::endl;
    std::cout << "  actual data bytes: " << (info.file_size - info.data_offset) << std::endl;
    std::cout << "  samples: " << num_samples << std::endl;
}

TEST_F(WavParserTest, CompareCorrectVsIncorrectHeader) {
    // Compare 1.wav (incorrect header) with wavetable.wav (correct header)
    auto data1 = ReadFile(GetTestWavPath(1));
    auto data2 = ReadFile("../wavetable.wav");
    
    ASSERT_FALSE(data1.empty());
    ASSERT_FALSE(data2.empty());
    
    auto info1 = wav_parser::ParseWavHeader(data1.data(), std::min(data1.size(), size_t(512)), data1.size());
    auto info2 = wav_parser::ParseWavHeader(data2.data(), std::min(data2.size(), size_t(512)), data2.size());
    
    ASSERT_TRUE(info1.valid);
    ASSERT_TRUE(info2.valid);
    
    std::cout << "Comparison:" << std::endl;
    std::cout << "  1.wav header data_size: " << info1.data_size << " (incorrect)" << std::endl;
    std::cout << "  wavetable.wav header data_size: " << info2.data_size << " (correct)" << std::endl;
    std::cout << "  Both files actual size: " << info1.file_size << std::endl;
    
    // Both should yield same usable data
    EXPECT_EQ(wav_parser::GetUsableDataBytes(info1), wav_parser::GetUsableDataBytes(info2));
    EXPECT_EQ(wav_parser::GetNumSamples(info1), wav_parser::GetNumSamples(info2));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
