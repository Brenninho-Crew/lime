/*
 * Copyright © 2018  Ebrahim Byagowi
 * Copyright © 2026  Lime Contributors
 *
 * This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <hb-static.cc>
#include <hb-ot-color-cbdt-table.hh>
#include <hb-ot-color-colr-table.hh>
#include <hb-ot-color-cpal-table.hh>
#include <hb-ot-color-sbix-table.hh>
#include <hb-ot-color-svg-table.hh>

#include <hb-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <cairo.h>
#include <cairo-ft.h>
#include <cairo-svg.h>

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

// ============================================================================
// Configuration Constants
// ============================================================================

namespace Config {
    constexpr const char* OUTPUT_DIR = "out";
    constexpr int MAX_PATH_LENGTH = 1024;
    constexpr float MARGIN_FACTOR = 0.1f;  // 10% margin
    constexpr float OFFSET_FACTOR = 0.05f; // 5% offset
    constexpr int GZIP_HEADER_BYTE1 = 0x1F;
    constexpr int GZIP_HEADER_BYTE2 = 0x8B;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Ensures that the output directory exists.
 */
bool ensureOutputDirectoryExists() {
    try {
        if (!fs::exists(Config::OUTPUT_DIR)) {
            fs::create_directories(Config::OUTPUT_DIR);
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating output directory: " << e.what() << std::endl;
        return false;
    }
}

/**
 * Safely writes data to a file.
 */
bool writeDataToFile(const std::string& filepath, const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return false;
    }
    
    FILE* file = std::fopen(filepath.c_str(), "wb");
    if (!file) {
        std::cerr << "Error opening file for writing: " << filepath << std::endl;
        return false;
    }
    
    size_t bytesWritten = std::fwrite(data, 1, length, file);
    std::fclose(file);
    
    if (bytesWritten != length) {
        std::cerr << "Error: wrote " << bytesWritten << " of " << length << " bytes" << std::endl;
        return false;
    }
    
    return true;
}

/**
 * Builds a file path with proper formatting.
 */
std::string buildOutputPath(const std::string& basename, const std::string& extension) {
    return std::string(Config::OUTPUT_DIR) + "/" + basename + "." + extension;
}

/**
 * Checks if data is gzipped by examining the first two bytes.
 */
bool isGzipped(const uint8_t* data, size_t length) {
    return length >= 2 && 
           data[0] == Config::GZIP_HEADER_BYTE1 && 
           data[1] == Config::GZIP_HEADER_BYTE2;
}

// ============================================================================
// Callback Classes
// ============================================================================

/**
 * Base class for dump callbacks.
 */
class DumpCallback {
public:
    virtual ~DumpCallback() = default;
    virtual void operator()(const uint8_t* data, unsigned int length) = 0;
};

/**
 * Callback for CBDT (color bitmap data table) dumps.
 */
class CBDTCallback : public DumpCallback {
public:
    CBDTCallback(unsigned int group, unsigned int gid) 
        : m_group(group), m_gid(gid) {}
    
    void operator()(const uint8_t* data, unsigned int length) override {
        char filename[Config::MAX_PATH_LENGTH];
        std::snprintf(filename, sizeof(filename), "cbdt-%u-%u.png", m_group, m_gid);
        
        std::string filepath = std::string(Config::OUTPUT_DIR) + "/" + filename;
        if (!writeDataToFile(filepath, data, length)) {
            std::cerr << "Failed to write CBDT data for group " << m_group 
                      << ", glyph " << m_gid << std::endl;
        }
    }
    
private:
    unsigned int m_group;
    unsigned int m_gid;
};

/**
 * Callback for sbix (standard bitmap graphics) dumps.
 */
class SbixCallback : public DumpCallback {
public:
    SbixCallback(unsigned int group, unsigned int gid) 
        : m_group(group), m_gid(gid) {}
    
    void operator()(const uint8_t* data, unsigned int length) override {
        char filename[Config::MAX_PATH_LENGTH];
        std::snprintf(filename, sizeof(filename), "sbix-%u-%u.png", m_group, m_gid);
        
        std::string filepath = std::string(Config::OUTPUT_DIR) + "/" + filename;
        if (!writeDataToFile(filepath, data, length)) {
            std::cerr << "Failed to write sbix data for group " << m_group 
                      << ", glyph " << m_gid << std::endl;
        }
    }
    
private:
    unsigned int m_group;
    unsigned int m_gid;
};

/**
 * Callback for SVG table dumps.
 */
class SVGTableCallback : public DumpCallback {
public:
    SVGTableCallback(unsigned int startGlyph, unsigned int endGlyph) 
        : m_startGlyph(startGlyph), m_endGlyph(endGlyph) {}
    
    void operator()(const uint8_t* data, unsigned int length) override {
        char filename[Config::MAX_PATH_LENGTH];
        
        if (m_startGlyph == m_endGlyph) {
            std::snprintf(filename, sizeof(filename), "svg-%u.svg", m_startGlyph);
        } else {
            std::snprintf(filename, sizeof(filename), "svg-%u-%u.svg", m_startGlyph, m_endGlyph);
        }
        
        // Add .gz extension if the content is gzipped
        if (isGzipped(data, length)) {
            std::strcat(filename, ".gz");
        }
        
        std::string filepath = std::string(Config::OUTPUT_DIR) + "/" + filename;
        if (!writeDataToFile(filepath, data, length)) {
            std::cerr << "Failed to write SVG data for glyph range " 
                      << m_startGlyph << "-" << m_endGlyph << std::endl;
        }
    }
    
private:
    unsigned int m_startGlyph;
    unsigned int m_endGlyph;
};

// ============================================================================
// Font Dumper Class
// ============================================================================

/**
 * Main class for dumping color font tables and rendering glyphs.
 */
class ColorFontDumper {
public:
    ColorFontDumper() = default;
    ~ColorFontDumper() = default;
    
    // Disable copy
    ColorFontDumper(const ColorFontDumper&) = delete;
    ColorFontDumper& operator=(const ColorFontDumper&) = delete;
    
    /**
     * Dumps all color tables from the specified font file.
     */
    bool dumpFont(const std::string& fontPath) {
        if (!ensureOutputDirectoryExists()) {
            return false;
        }
        
        // Create font objects
        auto blob = createBlob(fontPath);
        if (!blob) return false;
        
        auto face = createFace(blob.get());
        if (!face) return false;
        
        auto font = createFont(face.get());
        if (!font) return false;
        
        // Dump each table
        dumpCBDTTable(face.get());
        dumpSbixTable(face.get());
        dumpSVGTable(face.get());
        
        // Process COLR/CPAL tables
        auto colrBlob = getTableBlob<OT::COLR>(face.get());
        auto cpalBlob = getTableBlob<OT::CPAL>(face.get());
        
        if (colrBlob && cpalBlob) {
            processCOLRCPAL(face.get(), fontPath, 
                           colrBlob->as<OT::COLR>(), 
                           cpalBlob->as<OT::CPAL>());
        }
        
        return true;
    }
    
private:
    // ========================================================================
    // HarfBuzz Object Management
    // ========================================================================
    
    struct BlobDeleter {
        void operator()(hb_blob_t* blob) const { hb_blob_destroy(blob); }
    };
    
    struct FaceDeleter {
        void operator()(hb_face_t* face) const { hb_face_destroy(face); }
    };
    
    struct FontDeleter {
        void operator()(hb_font_t* font) const { hb_font_destroy(font); }
    };
    
    using BlobPtr = std::unique_ptr<hb_blob_t, BlobDeleter>;
    using FacePtr = std::unique_ptr<hb_face_t, FaceDeleter>;
    using FontPtr = std::unique_ptr<hb_font_t, FontDeleter>;
    
    BlobPtr createBlob(const std::string& path) {
        hb_blob_t* blob = hb_blob_create_from_file(path.c_str());
        if (!blob) {
            std::cerr << "Error: Could not load font file: " << path << std::endl;
            return nullptr;
        }
        return BlobPtr(blob);
    }
    
    FacePtr createFace(hb_blob_t* blob) {
        hb_face_t* face = hb_face_create(blob, 0);
        if (!face) {
            std::cerr << "Error: Could not create face from blob" << std::endl;
            return nullptr;
        }
        return FacePtr(face);
    }
    
    FontPtr createFont(hb_face_t* face) {
        hb_font_t* font = hb_font_create(face);
        if (!font) {
            std::cerr << "Error: Could not create font from face" << std::endl;
            return nullptr;
        }
        return FontPtr(font);
    }
    
    template<typename T>
    BlobPtr getTableBlob(hb_face_t* face) {
        hb_blob_t* blob = hb_sanitize_context_t().reference_table<T>(face);
        return BlobPtr(blob);
    }
    
    // ========================================================================
    // Table Dumping
    // ========================================================================
    
    void dumpCBDTTable(hb_face_t* face) {
        std::cout << "Dumping CBDT table..." << std::endl;
        
        OT::CBDT::accelerator_t cbdt;
        cbdt.init(face);
        
        cbdt.dump([](const uint8_t* data, unsigned int length,
                     unsigned int group, unsigned int gid) {
            CBDTCallback callback(group, gid);
            callback(data, length);
        });
        
        cbdt.fini();
        std::cout << "CBDT dump complete" << std::endl;
    }
    
    void dumpSbixTable(hb_face_t* face) {
        std::cout << "Dumping sbix table..." << std::endl;
        
        OT::sbix::accelerator_t sbix;
        sbix.init(face);
        
        sbix.dump([](const uint8_t* data, unsigned int length,
                     unsigned int group, unsigned int gid) {
            SbixCallback callback(group, gid);
            callback(data, length);
        });
        
        sbix.fini();
        std::cout << "sbix dump complete" << std::endl;
    }
    
    void dumpSVGTable(hb_face_t* face) {
        std::cout << "Dumping SVG table..." << std::endl;
        
        OT::SVG::accelerator_t svg;
        svg.init(face);
        
        svg.dump([](const uint8_t* data, unsigned int length,
                    unsigned int startGlyph, unsigned int endGlyph) {
            SVGTableCallback callback(startGlyph, endGlyph);
            callback(data, length);
        });
        
        svg.fini();
        std::cout << "SVG dump complete" << std::endl;
    }
    
    // ========================================================================
    // COLR/CPAL Processing
    // ========================================================================
    
    /**
     * Creates a Cairo font face from a FreeType face.
     */
    cairo_font_face_t* createCairoFontFace(const std::string& fontPath) {
        FT_Library library;
        if (FT_Init_FreeType(&library) != 0) {
            std::cerr << "Error: Could not initialize FreeType" << std::endl;
            return nullptr;
        }
        
        FT_Face ftface;
        if (FT_New_Face(library, fontPath.c_str(), 0, &ftface) != 0) {
            std::cerr << "Error: Could not create FreeType face" << std::endl;
            FT_Done_FreeType(library);
            return nullptr;
        }
        
        return cairo_ft_font_face_create_for_ft_face(ftface, 0);
    }
    
    /**
     * Renders COLR/CPAL glyphs to SVG files.
     */
    void processCOLRCPAL(hb_face_t* face, const std::string& fontPath,
                         const OT::COLR* colr, const OT::CPAL* cpal) {
        if (!colr || !cpal) {
            std::cout << "No COLR/CPAL tables found" << std::endl;
            return;
        }
        
        std::cout << "Processing COLR/CPAL tables..." << std::endl;
        
        unsigned int numGlyphs = hb_face_get_glyph_count(face);
        unsigned int upem = hb_face_get_upem(face);
        
        cairo_font_face_t* cairoFace = createCairoFontFace(fontPath);
        if (!cairoFace) {
            std::cerr << "Error: Could not create Cairo font face" << std::endl;
            return;
        }
        
        for (unsigned int glyphId = 0; glyphId < numGlyphs; ++glyphId) {
            renderCOLRGlyph(cairoFace, upem, glyphId, colr, cpal);
        }
        
        cairo_font_face_destroy(cairoFace);
        std::cout << "COLR/CPAL processing complete" << std::endl;
    }
    
    /**
     * Renders a single COLR glyph with all available palettes.
     */
    void renderCOLRGlyph(cairo_font_face_t* cairoFace, unsigned int upem,
                         unsigned int glyphId, const OT::COLR* colr, 
                         const OT::CPAL* cpal) {
        unsigned int firstLayerIndex = 0;
        unsigned int numLayers = 0;
        
        if (!colr->get_base_glyph_record(glyphId, &firstLayerIndex, &numLayers)) {
            return; // No COLR layers for this glyph
        }
        
        // Measure glyph extents
        auto extents = measureGlyphLayers(cairoFace, upem, firstLayerIndex, numLayers, colr);
        
        // Add margin
        extents.width += extents.width * Config::MARGIN_FACTOR;
        extents.height += extents.height * Config::MARGIN_FACTOR;
        extents.xBearing -= extents.width * Config::OFFSET_FACTOR;
        extents.yBearing -= extents.height * Config::OFFSET_FACTOR;
        
        // Render for each palette
        unsigned int paletteCount = cpal->get_palette_count();
        for (unsigned int palette = 0; palette < paletteCount; ++palette) {
            renderCOLRPalette(cairoFace, upem, glyphId, palette, paletteCount,
                             firstLayerIndex, numLayers, extents, colr, cpal);
        }
    }
    
    /**
     * Structure for glyph extents.
     */
    struct GlyphExtents {
        double xBearing = 0.0;
        double yBearing = 0.0;
        double width = 0.0;
        double height = 0.0;
        double xAdvance = 0.0;
        double yAdvance = 0.0;
    };
    
    /**
     * Measures the extents of a set of glyph layers.
     */
    GlyphExtents measureGlyphLayers(cairo_font_face_t* cairoFace, unsigned int upem,
                                    unsigned int firstLayerIndex, unsigned int numLayers,
                                    const OT::COLR* colr) {
        // Create a temporary surface for measurement
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cairo_t* cr = cairo_create(surface);
        cairo_set_font_face(cr, cairoFace);
        cairo_set_font_size(cr, upem);
        
        // Build glyph array
        std::vector<cairo_glyph_t> glyphs(numLayers);
        for (unsigned int j = 0; j < numLayers; ++j) {
            hb_codepoint_t glyphId;
            unsigned int colorIndex;
            colr->get_layer_record(firstLayerIndex + j, &glyphId, &colorIndex);
            glyphs[j].index = glyphId;
        }
        
        // Measure
        cairo_text_extents_t cairoExtents;
        cairo_glyph_extents(cr, glyphs.data(), numLayers, &cairoExtents);
        
        // Cleanup
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        
        // Convert to our struct
        GlyphExtents extents;
        extents.xBearing = cairoExtents.x_bearing;
        extents.yBearing = cairoExtents.y_bearing;
        extents.width = cairoExtents.width;
        extents.height = cairoExtents.height;
        extents.xAdvance = cairoExtents.x_advance;
        extents.yAdvance = cairoExtents.y_advance;
        
        return extents;
    }
    
    /**
     * Renders a single COLR glyph with a specific palette.
     */
    void renderCOLRPalette(cairo_font_face_t* cairoFace, unsigned int upem,
                           unsigned int glyphId, unsigned int palette,
                           unsigned int paletteCount, unsigned int firstLayerIndex,
                           unsigned int numLayers, const GlyphExtents& extents,
                           const OT::COLR* colr, const OT::CPAL* cpal) {
        // Generate output filename
        char filename[Config::MAX_PATH_LENGTH];
        if (paletteCount == 1) {
            std::snprintf(filename, sizeof(filename), "colr-%u.svg", glyphId);
        } else {
            std::snprintf(filename, sizeof(filename), "colr-%u-%u.svg", glyphId, palette);
        }
        
        std::string filepath = std::string(Config::OUTPUT_DIR) + "/" + filename;
        
        // Create SVG surface
        cairo_surface_t* surface = cairo_svg_surface_create(filepath.c_str(), 
                                                            extents.width, 
                                                            extents.height);
        cairo_t* cr = cairo_create(surface);
        cairo_set_font_face(cr, cairoFace);
        cairo_set_font_size(cr, upem);
        
        // Render each layer
        for (unsigned int j = 0; j < numLayers; ++j) {
            hb_codepoint_t layerGlyphId;
            unsigned int colorIndex;
            colr->get_layer_record(firstLayerIndex + j, &layerGlyphId, &colorIndex);
            
            // Get and set color
            uint32_t color = cpal->get_color_record_argb(colorIndex, palette);
            double alpha = (color & 0xFF) / 255.0;
            double r = ((color >> 8) & 0xFF) / 255.0;
            double g = ((color >> 16) & 0xFF) / 255.0;
            double b = ((color >> 24) & 0xFF) / 255.0;
            
            cairo_set_source_rgba(cr, r, g, b, alpha);
            
            // Position and render glyph
            cairo_glyph_t glyph;
            glyph.index = layerGlyphId;
            glyph.x = -extents.xBearing;
            glyph.y = -extents.yBearing;
            
            cairo_show_glyphs(cr, &glyph, 1);
        }
        
        // Cleanup
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
};

// ============================================================================
// Main Function
// ============================================================================

/**
 * Program entry point.
 */
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " font-file.ttf" << std::endl;
        return EXIT_FAILURE;
    }
    
    try {
        ColorFontDumper dumper;
        
        if (dumper.dumpFont(argv[1])) {
            std::cout << "Successfully dumped all color font tables to '"
                      << Config::OUTPUT_DIR << "' directory" << std::endl;
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Failed to dump font tables" << std::endl;
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return EXIT_FAILURE;
    }
}