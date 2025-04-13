/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#include <string>
#include <assert.h>
#include <zlib.h>

// Helper function to decompress gzip-compressed data using zlib.
// Included to remove the boost iostreams dependency
std::string decompress_gzip(const std::string &compressed_data) {
    // Initialize zlib stream
    z_stream zs = {};
    zs.next_in =
        reinterpret_cast<Bytef *>(const_cast<char *>(compressed_data.data()));
    zs.avail_in = compressed_data.size();

    // Use gzip mode (MAX_WBITS + 16)
    int result = inflateInit2(&zs, 16 + MAX_WBITS);
    assert(result == Z_OK && "Failed to initialize zlib for gzip decompression");

    // Initial output buffer size - we'll resize as needed
    const size_t chunk_size = 16384; // 16KB
    std::string decompressed;
    char outbuffer[chunk_size];

    // Decompress until no more data
    do {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = chunk_size;

        result = inflate(&zs, Z_NO_FLUSH);
        assert(result != Z_STREAM_ERROR &&
          "Zlib stream error during decompression");

        switch (result) {
            case Z_NEED_DICT:
                result = Z_DATA_ERROR;
            [[fallthrough]];
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
              inflateEnd(&zs);
            assert(false && "Zlib decompression error");
            break;
        }

        // Append decompressed data
        size_t bytes_decompressed = chunk_size - zs.avail_out;
        if (bytes_decompressed > 0) {
            decompressed.append(outbuffer, bytes_decompressed);
        }
    } while (zs.avail_out == 0);

    // Clean up
    inflateEnd(&zs);
    return decompressed;
}
