#include <graphics/Image.h>
#include <cassert>
#include <stdexcept>
#include <iostream>

namespace lime {

    // ============================================================================
    // Static Member Initialization
    // ============================================================================
    
    int Image::s_bufferFieldId = 0;
    int Image::s_heightFieldId = 0;
    int Image::s_offsetXFieldId = 0;
    int Image::s_offsetYFieldId = 0;
    int Image::s_widthFieldId = 0;
    bool Image::s_fieldsInitialized = false;
    
    // ============================================================================
    // Constructor
    // ============================================================================
    
    /**
     * Constructs an Image object from a Haxe value.
     * 
     * @param image The Haxe object containing image data
     * @throws std::runtime_error if the input is invalid or fields are missing
     */
    Image::Image(value image) {
        // Validate input
        if (image == nullptr) {
            throw std::runtime_error("Image constructor received null value");
        }
        
        // Initialize field IDs on first use
        initializeFieldIds();
        
        // Extract image properties from Haxe object
        try {
            width = extractIntField(image, id_width, "width");
            height = extractIntField(image, id_height, "height");
            offsetX = extractIntField(image, id_offsetX, "offsetX");
            offsetY = extractIntField(image, id_offsetY, "offsetY");
            
            // Create image buffer from the buffer field
            value bufferValue = val_field(image, id_buffer);
            if (bufferValue == nullptr) {
                throw std::runtime_error("Image buffer field is null");
            }
            
            buffer = new ImageBuffer(bufferValue);
            
        } catch (const std::exception& e) {
            // Clean up any partially allocated resources
            buffer = nullptr;
            
            // Re-throw with additional context
            throw std::runtime_error(std::string("Failed to construct Image: ") + e.what());
        }
    }
    
    // ============================================================================
    // Destructor
    // ============================================================================
    
    /**
     * Destructor - cleans up the image buffer.
     */
    Image::~Image() {
        cleanup();
    }
    
    // ============================================================================
    // Move Constructor
    // ============================================================================
    
    /**
     * Move constructor - transfers ownership of resources.
     */
    Image::Image(Image&& other) noexcept
        : width(other.width)
        , height(other.height)
        , offsetX(other.offsetX)
        , offsetY(other.offsetY)
        , buffer(other.buffer) {
        
        // Reset the source object
        other.width = 0;
        other.height = 0;
        other.offsetX = 0;
        other.offsetY = 0;
        other.buffer = nullptr;
    }
    
    // ============================================================================
    // Move Assignment Operator
    // ============================================================================
    
    /**
     * Move assignment operator - transfers ownership of resources.
     */
    Image& Image::operator=(Image&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            cleanup();
            
            // Transfer resources
            width = other.width;
            height = other.height;
            offsetX = other.offsetX;
            offsetY = other.offsetY;
            buffer = other.buffer;
            
            // Reset the source object
            other.width = 0;
            other.height = 0;
            other.offsetX = 0;
            other.offsetY = 0;
            other.buffer = nullptr;
        }
        
        return *this;
    }
    
    // ============================================================================
    // Copy Constructor (Deleted)
    // ============================================================================
    
    // Copy constructor is deleted because ImageBuffer may not be copyable
    // If copy is needed, implement deep copy here
    
    // ============================================================================
    // Private Methods
    // ============================================================================
    
    /**
     * Initializes Haxe field IDs for efficient property access.
     * This is called once when the first Image is constructed.
     */
    void Image::initializeFieldIds() {
        if (!s_fieldsInitialized) {
            s_bufferFieldId = val_id("buffer");
            s_heightFieldId = val_id("height");
            s_offsetXFieldId = val_id("offsetX");
            s_offsetYFieldId = val_id("offsetY");
            s_widthFieldId = val_id("width");
            
            s_fieldsInitialized = true;
            
            // Validate that all field IDs were successfully obtained
            assert(s_bufferFieldId != 0 && "Failed to get 'buffer' field ID");
            assert(s_heightFieldId != 0 && "Failed to get 'height' field ID");
            assert(s_offsetXFieldId != 0 && "Failed to get 'offsetX' field ID");
            assert(s_offsetYFieldId != 0 && "Failed to get 'offsetY' field ID");
            assert(s_widthFieldId != 0 && "Failed to get 'width' field ID");
        }
    }
    
    /**
     * Safely extracts an integer field from a Haxe object.
     * 
     * @param obj The Haxe object
     * @param fieldId The field ID to extract
     * @param fieldName The field name for error messages
     * @return The extracted integer value
     * @throws std::runtime_error if the field is missing or invalid
     */
    int Image::extractIntField(value obj, int fieldId, const char* fieldName) {
        value fieldValue = val_field(obj, fieldId);
        
        if (fieldValue == nullptr) {
            throw std::runtime_error(std::string("Missing required field: ") + fieldName);
        }
        
        if (!val_is_int(fieldValue)) {
            throw std::runtime_error(std::string("Field '") + fieldName + "' is not an integer");
        }
        
        return val_int(fieldValue);
    }
    
    /**
     * Cleans up allocated resources.
     */
    void Image::cleanup() {
        if (buffer != nullptr) {
            delete buffer;
            buffer = nullptr;
        }
    }
    
    // ============================================================================
    // Public Utility Methods
    // ============================================================================
    
    /**
     * Checks if the image has valid dimensions and buffer.
     * 
     * @return true if the image is valid, false otherwise
     */
    bool Image::isValid() const {
        return buffer != nullptr && 
               buffer->isValid() && 
               width > 0 && 
               height > 0;
    }
    
    /**
     * Gets the total number of pixels in the image.
     * 
     * @return The total pixel count
     */
    size_t Image::getPixelCount() const {
        return static_cast<size_t>(width) * static_cast<size_t>(height);
    }
    
    /**
     * Creates a deep copy of this image.
     * 
     * @return A new Image object that is a deep copy of this one
     */
    Image* Image::clone() const {
        if (!isValid()) {
            return nullptr;
        }
        
        // Create a new image buffer that is a copy of the current one
        ImageBuffer* newBuffer = buffer->clone();
        if (newBuffer == nullptr) {
            return nullptr;
        }
        
        // Create a new image with the copied buffer
        Image* newImage = new Image();
        newImage->width = width;
        newImage->height = height;
        newImage->offsetX = offsetX;
        newImage->offsetY = offsetY;
        newImage->buffer = newBuffer;
        
        return newImage;
    }
    
    // ============================================================================
    // Debugging Support
    // ============================================================================
    
    /**
     * Prints image information for debugging purposes.
     */
    void Image::dump() const {
        std::cout << "Image [width=" << width 
                  << ", height=" << height 
                  << ", offset=(" << offsetX << "," << offsetY << ")"
                  << ", buffer=" << (buffer ? "valid" : "null") << "]" 
                  << std::endl;
        
        if (buffer) {
            buffer->dump();
        }
    }

}
