#ifndef RIVE_CONVERTER_SERIALIZER_DIAGNOSTICS_HPP
#define RIVE_CONVERTER_SERIALIZER_DIAGNOSTICS_HPP

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace rive_converter
{

/**
 * Serializer diagnostics for tracking chunk boundaries, offsets, and alignment.
 * Enable with environment variable: RIVE_SERIALIZE_VERBOSE=1
 */
class SerializerDiagnostics
{
private:
    bool m_Enabled;
    std::string m_Indent;
    
    struct ChunkInfo
    {
        std::string name;
        size_t startOffset;
        size_t endOffset;
        size_t expectedSize;
    };
    
    std::vector<ChunkInfo> m_Chunks;
    
public:
    SerializerDiagnostics() : m_Enabled(false), m_Indent("")
    {
        const char* env = std::getenv("RIVE_SERIALIZE_VERBOSE");
        if (env && std::string(env) == "1")
        {
            m_Enabled = true;
        }
    }
    
    bool isEnabled() const { return m_Enabled; }
    
    void beginChunk(const std::string& name, size_t offset, size_t expectedSize = 0)
    {
        if (!m_Enabled) return;
        
        ChunkInfo info;
        info.name = name;
        info.startOffset = offset;
        info.endOffset = 0;  // Will be set by endChunk
        info.expectedSize = expectedSize;
        m_Chunks.push_back(info);
        
        std::cout << m_Indent << "📦 " << name << " @ offset " << offset;
        if (expectedSize > 0)
        {
            std::cout << " (expect ~" << expectedSize << " bytes)";
        }
        std::cout << std::endl;
        
        m_Indent += "  ";
    }
    
    void endChunk(const std::string& name, size_t offset)
    {
        if (!m_Enabled) return;
        
        if (m_Indent.size() >= 2)
        {
            m_Indent.resize(m_Indent.size() - 2);
        }
        
        if (!m_Chunks.empty() && m_Chunks.back().name == name)
        {
            m_Chunks.back().endOffset = offset;
            size_t actualSize = offset - m_Chunks.back().startOffset;
            
            std::cout << m_Indent << "✅ " << name << " complete: " << actualSize << " bytes";
            
            if (m_Chunks.back().expectedSize > 0)
            {
                size_t expected = m_Chunks.back().expectedSize;
                int diff = static_cast<int>(actualSize) - static_cast<int>(expected);
                if (diff != 0)
                {
                    std::cout << " (diff: " << (diff > 0 ? "+" : "") << diff << ")";
                }
            }
            
            std::cout << std::endl;
        }
    }
    
    void logOffset(const std::string& label, size_t offset)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "  📍 " << label << ": offset " << offset << std::endl;
    }
    
    void logAlignment(const std::string& what, size_t offset, size_t alignment)
    {
        if (!m_Enabled) return;
        
        size_t remainder = offset % alignment;
        if (remainder == 0)
        {
            std::cout << m_Indent << "  ✓ " << what << " aligned to " << alignment 
                     << " bytes (offset " << offset << ")" << std::endl;
        }
        else
        {
            std::cout << m_Indent << "  ⚠ " << what << " NOT aligned to " << alignment 
                     << " bytes (offset " << offset << ", remainder " << remainder << ")" << std::endl;
        }
    }
    
    void checkAlignment(const std::string& what, size_t offset, size_t alignment)
    {
        if (!m_Enabled) return;
        logAlignment(what, offset, alignment);
    }
    
    void logCount(const std::string& what, size_t count)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "  🔢 " << what << ": " << count << std::endl;
    }
    
    void logProperty(const std::string& name, uint16_t key, const std::string& type)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "    • " << name << " (key " << key << ", " << type << ")" << std::endl;
    }
    
    void warn(const std::string& message)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "  ⚠️  WARNING: " << message << std::endl;
    }
    
    void error(const std::string& message)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "  ❌ ERROR: " << message << std::endl;
    }
    
    void info(const std::string& message)
    {
        if (!m_Enabled) return;
        std::cout << m_Indent << "  ℹ️  " << message << std::endl;
    }
    
    void printSummary()
    {
        if (!m_Enabled || m_Chunks.empty()) return;
        
        std::cout << "\n" << "═══════════════════════════════════════════════════════════" << std::endl;
        std::cout << "Serialization Summary" << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════" << std::endl;
        
        size_t totalSize = m_Chunks.empty() ? 0 : m_Chunks.back().endOffset;
        
        for (const auto& chunk : m_Chunks)
        {
            size_t size = chunk.endOffset - chunk.startOffset;
            float pct = (totalSize > 0) ? (static_cast<float>(size) / totalSize * 100.0f) : 0.0f;
            
            std::cout << chunk.name << ": " << size << " bytes (" << pct << "%)" << std::endl;
        }
        
        std::cout << "\nTotal: " << totalSize << " bytes" << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════\n" << std::endl;
    }
};

} // namespace rive_converter

#endif // RIVE_CONVERTER_SERIALIZER_DIAGNOSTICS_HPP
