// PCLFileFormatMock.h - Mock implementation for FileFormat API
#ifndef __PCL_FileFormat_Mock_h
#define __PCL_FileFormat_Mock_h

#include <string>
#include <map>
#include <vector>
#include <cstring>

// Mock types
typedef void* meta_format_handle;
typedef void* format_handle;
typedef int api_bool;
typedef size_t size_type;
typedef uint32_t uint32;

// Mock file format structure
struct MockFileFormat {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> mimeTypes;
    uint32 version;
    std::string description;
    std::string implementation;
    bool canRead;
    bool canWrite;
    bool canReadIncrementally;
    bool canWriteIncrementally;
    bool canStore8Bit;
    bool canStore16Bit;
    bool canStore32Bit;
    bool canStore64Bit;
    bool canStoreFloat;
    bool canStoreDouble;
    bool canStoreComplex;
    bool canStoreDComplex;
    bool canStoreGrayscale;
    bool canStoreRGBColor;
    bool canStoreAlphaChannels;
    bool supportsCompression;
    bool supportsMultipleImages;
};

// Global mock file formats registry
class MockFileFormatRegistry {
public:
    static MockFileFormatRegistry& Instance() {
        static MockFileFormatRegistry instance;
        return instance;
    }
    
    void RegisterFormat(const MockFileFormat& format) {
        formats[format.name] = format;
        
        // Index by extensions
        for (const auto& ext : format.extensions) {
            extensionMap[ext] = format.name;
        }
        
        // Index by MIME types
        for (const auto& mime : format.mimeTypes) {
            mimeMap[mime] = format.name;
        }
    }
    
    MockFileFormat* FindByName(const std::string& name) {
        auto it = formats.find(name);
        return (it != formats.end()) ? &it->second : nullptr;
    }
    
    MockFileFormat* FindByExtension(const std::string& ext) {
        auto it = extensionMap.find(ext);
        if (it != extensionMap.end()) {
            return FindByName(it->second);
        }
        return nullptr;
    }
    
    MockFileFormat* FindByMime(const std::string& mime) {
        auto it = mimeMap.find(mime);
        if (it != mimeMap.end()) {
            return FindByName(it->second);
        }
        return nullptr;
    }
    
    std::vector<std::string> GetAllFormatNames() const {
        std::vector<std::string> names;
        for (const auto& pair : formats) {
            names.push_back(pair.first);
        }
        return names;
    }
    
private:
    MockFileFormatRegistry() {
        // Register some common formats
        
        // XISF format
        MockFileFormat xisf;
        xisf.name = "XISF";
        xisf.extensions = {".xisf"};
        xisf.mimeTypes = {"application/xisf"};
        xisf.version = 0x100;
        xisf.description = "Extensible Image Serialization Format";
        xisf.implementation = "XISF Format Support Module Version 1.0";
        xisf.canRead = true;
        xisf.canWrite = true;
        xisf.canReadIncrementally = true;
        xisf.canWriteIncrementally = true;
        xisf.canStore8Bit = true;
        xisf.canStore16Bit = true;
        xisf.canStore32Bit = true;
        xisf.canStore64Bit = true;
        xisf.canStoreFloat = true;
        xisf.canStoreDouble = true;
        xisf.canStoreComplex = true;
        xisf.canStoreDComplex = true;
        xisf.canStoreGrayscale = true;
        xisf.canStoreRGBColor = true;
        xisf.canStoreAlphaChannels = true;
        xisf.supportsCompression = true;
        xisf.supportsMultipleImages = true;
        RegisterFormat(xisf);
        
        // FITS format
        MockFileFormat fits;
        fits.name = "FITS";
        fits.extensions = {".fits", ".fit", ".fts"};
        fits.mimeTypes = {"application/fits", "image/fits"};
        fits.version = 0x100;
        fits.description = "Flexible Image Transport System";
        fits.implementation = "FITS Format Support Module Version 1.0";
        fits.canRead = true;
        fits.canWrite = true;
        fits.canReadIncrementally = false;
        fits.canWriteIncrementally = false;
        fits.canStore8Bit = true;
        fits.canStore16Bit = true;
        fits.canStore32Bit = true;
        fits.canStore64Bit = true;
        fits.canStoreFloat = true;
        fits.canStoreDouble = true;
        fits.canStoreComplex = false;
        fits.canStoreDComplex = false;
        fits.canStoreGrayscale = true;
        fits.canStoreRGBColor = true;
        fits.canStoreAlphaChannels = false;
        fits.supportsCompression = false;
        fits.supportsMultipleImages = true;
        RegisterFormat(fits);
        
        // TIFF format
        MockFileFormat tiff;
        tiff.name = "TIFF";
        tiff.extensions = {".tif", ".tiff"};
        tiff.mimeTypes = {"image/tiff"};
        tiff.version = 0x100;
        tiff.description = "Tagged Image File Format";
        tiff.implementation = "TIFF Format Support Module Version 1.0";
        tiff.canRead = true;
        tiff.canWrite = true;
        tiff.canReadIncrementally = true;
        tiff.canWriteIncrementally = true;
        tiff.canStore8Bit = true;
        tiff.canStore16Bit = true;
        tiff.canStore32Bit = true;
        tiff.canStore64Bit = false;
        tiff.canStoreFloat = true;
        tiff.canStoreDouble = false;
        tiff.canStoreComplex = false;
        tiff.canStoreDComplex = false;
        tiff.canStoreGrayscale = true;
        tiff.canStoreRGBColor = true;
        tiff.canStoreAlphaChannels = true;
        tiff.supportsCompression = true;
        tiff.supportsMultipleImages = true;
        RegisterFormat(tiff);
        
        // JPEG format
        MockFileFormat jpeg;
        jpeg.name = "JPEG";
        jpeg.extensions = {".jpg", ".jpeg", ".jpe"};
        jpeg.mimeTypes = {"image/jpeg"};
        jpeg.version = 0x100;
        jpeg.description = "Joint Photographic Experts Group";
        jpeg.implementation = "JPEG Format Support Module Version 1.0";
        jpeg.canRead = true;
        jpeg.canWrite = true;
        jpeg.canReadIncrementally = false;
        jpeg.canWriteIncrementally = false;
        jpeg.canStore8Bit = true;
        jpeg.canStore16Bit = false;
        jpeg.canStore32Bit = false;
        jpeg.canStore64Bit = false;
        jpeg.canStoreFloat = false;
        jpeg.canStoreDouble = false;
        jpeg.canStoreComplex = false;
        jpeg.canStoreDComplex = false;
        jpeg.canStoreGrayscale = true;
        jpeg.canStoreRGBColor = true;
        jpeg.canStoreAlphaChannels = false;
        jpeg.supportsCompression = true;
        jpeg.supportsMultipleImages = false;
        RegisterFormat(jpeg);
    }
    
    std::map<std::string, MockFileFormat> formats;
    std::map<std::string, std::string> extensionMap;  // extension -> format name
    std::map<std::string, std::string> mimeMap;       // mime -> format name
};

// Mock API implementations
extern "C" {

// Get format name
api_bool API_FileFormat_GetFileFormatName(meta_format_handle handle, char* name, size_type* len)
{
    if (handle == nullptr || len == nullptr) {
        return 0;  // false
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    
    if (name == nullptr) {
        // Caller wants to know the required buffer size
        *len = format->name.length();
        return 1;  // true
    }
    
    // Copy the name
    size_type copyLen = std::min(*len - 1, format->name.length());
    std::strncpy(name, format->name.c_str(), copyLen);
    name[copyLen] = '\0';
    *len = copyLen;
    
    return 1;  // true
}

// Get file extensions
api_bool API_FileFormat_GetFileExtensions(meta_format_handle handle, char** extensions, size_type* count)
{
    if (handle == nullptr || count == nullptr) {
        return 0;
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    
    if (extensions == nullptr) {
        // Return count
        *count = format->extensions.size();
        return 1;
    }
    
    // Allocate and copy extensions
    for (size_t i = 0; i < std::min(*count, format->extensions.size()); ++i) {
        extensions[i] = strdup(format->extensions[i].c_str());
    }
    *count = format->extensions.size();
    
    return 1;
}

// Get MIME types
api_bool API_FileFormat_GetMimeTypes(meta_format_handle handle, char** mimes, size_type* count)
{
    if (handle == nullptr || count == nullptr) {
        return 0;
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    
    if (mimes == nullptr) {
        *count = format->mimeTypes.size();
        return 1;
    }
    
    for (size_t i = 0; i < std::min(*count, format->mimeTypes.size()); ++i) {
        mimes[i] = strdup(format->mimeTypes[i].c_str());
    }
    *count = format->mimeTypes.size();
    
    return 1;
}

// Get version
api_bool API_FileFormat_GetVersion(meta_format_handle handle, uint32* version)
{
    if (handle == nullptr || version == nullptr) {
        return 0;
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    *version = format->version;
    return 1;
}

// Get description
api_bool API_FileFormat_GetDescription(meta_format_handle handle, char* desc, size_type* len)
{
    if (handle == nullptr || len == nullptr) {
        return 0;
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    
    if (desc == nullptr) {
        *len = format->description.length();
        return 1;
    }
    
    size_type copyLen = std::min(*len - 1, format->description.length());
    std::strncpy(desc, format->description.c_str(), copyLen);
    desc[copyLen] = '\0';
    *len = copyLen;
    
    return 1;
}

// Get implementation
api_bool API_FileFormat_GetImplementation(meta_format_handle handle, char* impl, size_type* len)
{
    if (handle == nullptr || len == nullptr) {
        return 0;
    }
    
    MockFileFormat* format = static_cast<MockFileFormat*>(handle);
    
    if (impl == nullptr) {
        *len = format->implementation.length();
        return 1;
    }
    
    size_type copyLen = std::min(*len - 1, format->implementation.length());
    std::strncpy(impl, format->implementation.c_str(), copyLen);
    impl[copyLen] = '\0';
    *len = copyLen;
    
    return 1;
}

// Capability queries
api_bool API_FileFormat_CanRead(meta_format_handle handle, api_bool* canRead)
{
    if (handle == nullptr || canRead == nullptr) return 0;
    *canRead = static_cast<MockFileFormat*>(handle)->canRead;
    return 1;
}

api_bool API_FileFormat_CanWrite(meta_format_handle handle, api_bool* canWrite)
{
    if (handle == nullptr || canWrite == nullptr) return 0;
    *canWrite = static_cast<MockFileFormat*>(handle)->canWrite;
    return 1;
}

api_bool API_FileFormat_CanReadIncrementally(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canReadIncrementally;
    return 1;
}

api_bool API_FileFormat_CanWriteIncrementally(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canWriteIncrementally;
    return 1;
}

api_bool API_FileFormat_CanStore8Bit(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStore8Bit;
    return 1;
}

api_bool API_FileFormat_CanStore16Bit(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStore16Bit;
    return 1;
}

api_bool API_FileFormat_CanStore32Bit(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStore32Bit;
    return 1;
}

api_bool API_FileFormat_CanStoreFloat(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStoreFloat;
    return 1;
}

api_bool API_FileFormat_CanStoreDouble(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStoreDouble;
    return 1;
}

api_bool API_FileFormat_CanStoreGrayscale(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStoreGrayscale;
    return 1;
}

api_bool API_FileFormat_CanStoreRGBColor(meta_format_handle handle, api_bool* can)
{
    if (handle == nullptr || can == nullptr) return 0;
    *can = static_cast<MockFileFormat*>(handle)->canStoreRGBColor;
    return 1;
}

api_bool API_FileFormat_SupportsCompression(meta_format_handle handle, api_bool* supports)
{
    if (handle == nullptr || supports == nullptr) return 0;
    *supports = static_cast<MockFileFormat*>(handle)->supportsCompression;
    return 1;
}

api_bool API_FileFormat_SupportsMultipleImages(meta_format_handle handle, api_bool* supports)
{
    if (handle == nullptr || supports == nullptr) return 0;
    *supports = static_cast<MockFileFormat*>(handle)->supportsMultipleImages;
    return 1;
}

// Find format by name, extension, or MIME type
meta_format_handle API_FileFormat_Find(const char* nameExtOrMime, api_bool toRead, api_bool toWrite)
{
    if (nameExtOrMime == nullptr) {
        return nullptr;
    }
    
    std::string search(nameExtOrMime);
    MockFileFormat* format = nullptr;
    
    // Check if it's an extension (starts with '.')
    if (search[0] == '.') {
        format = MockFileFormatRegistry::Instance().FindByExtension(search);
    }
    // Check if it's a MIME type (contains '/')
    else if (search.find('/') != std::string::npos) {
        format = MockFileFormatRegistry::Instance().FindByMime(search);
    }
    // Otherwise, search by name
    else {
        format = MockFileFormatRegistry::Instance().FindByName(search);
    }
    
    if (format == nullptr) {
        return nullptr;
    }
    
    // Check read/write capabilities if requested
    if (toRead && !format->canRead) {
        return nullptr;
    }
    if (toWrite && !format->canWrite) {
        return nullptr;
    }
    
    return static_cast<meta_format_handle>(format);
}

// Get all formats
api_bool API_FileFormat_GetAllFormats(meta_format_handle** handles, size_type* count)
{
    if (count == nullptr) {
        return 0;
    }
    
    auto names = MockFileFormatRegistry::Instance().GetAllFormatNames();
    
    if (handles == nullptr) {
        *count = names.size();
        return 1;
    }
    
    // Allocate array
    *handles = new meta_format_handle[names.size()];
    
    for (size_t i = 0; i < names.size(); ++i) {
        MockFileFormat* fmt = MockFileFormatRegistry::Instance().FindByName(names[i]);
        (*handles)[i] = static_cast<meta_format_handle>(fmt);
    }
    
    *count = names.size();
    return 1;
}

} // extern "C"

#endif // __PCL_FileFormat_Mock_h
