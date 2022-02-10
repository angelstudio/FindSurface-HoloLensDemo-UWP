#ifndef _FIND_SURFACE_HPP_
#define _FIND_SURFACE_HPP_

//#if !defined(__cplusplus) || __cplusplus < 201103L
//#error "c++ 11+ is required"
//#endif

#include "FindSurface.h"
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

namespace FindSurfaceException
{
    class InvalidArgument : public std::logic_error
    {
    public:
        InvalidArgument(const std::string &r) : std::logic_error(r) {}
        InvalidArgument(const char *r) : std::logic_error(r) {}
    };

    class InvalidOperation : public std::logic_error
    {
    public:
        InvalidOperation(const std::string &r) : std::logic_error(r) {}
        InvalidOperation(const char *r) : std::logic_error(r) {}
    };

    class OutOfMemory : public std::runtime_error
    {
    public:
        OutOfMemory(const std::string &r) : std::runtime_error(r) {}
        OutOfMemory(const char *r) : std::runtime_error(r) {}
    };
};

class FindSurfaceInlierFlags
{
    friend class FindSurface;
private:
    std::vector<unsigned char> m_vecFlags;

private:
    FindSurfaceInlierFlags( const unsigned char *flags, size_t size ) : m_vecFlags( flags, flags + size ) {}

public:
    inline size_t size() const { return m_vecFlags.size(); }
    inline bool isInlierAt(int index)  const { return m_vecFlags[index] == 0; }
    inline bool isOutlierAt(int index) const { return m_vecFlags[index] != 0; }
};

class FindSurfaceResult : public FS_FEATURE_RESULT
{
    friend class FindSurface;
private:
    std::unique_ptr<const FindSurfaceInlierFlags> m_pFlags;

private:
    FindSurfaceResult( const FS_FEATURE_RESULT& r, std::unique_ptr<const FindSurfaceInlierFlags>& flags ) : m_pFlags(std::move(flags)) {
        *reinterpret_cast<FS_FEATURE_RESULT *>(this) = r;
    }

public:
    ~FindSurfaceResult() { m_pFlags.reset(); }

public:
    inline FS_FEATURE_TYPE getType()     const { return type; }
    inline float           getRMSError() const { return rms; }

    inline const FindSurfaceInlierFlags* getInlierFlags() const { return m_pFlags.get(); }
};

class FindSurface
{
private:
    FIND_SURFACE_CONTEXT m_hCtx;

private:
    FindSurface() : m_hCtx(nullptr) { createFindSurface( &m_hCtx ); }
    ~FindSurface() { releaseFindSurface(m_hCtx); }

public:
    static FindSurface *getInstance() {
        static FindSurface instance;
        return instance.m_hCtx ? &instance : nullptr;
    }

public: // Setter
    inline void setSmartConversionOptions(int options) { ::setSmartConversionOptions(m_hCtx, options); }
    inline void setRadialExpansion(FS_SEARCH_LEVEL level) { ::setRadialExpansion(m_hCtx, level); }
    inline void setLateralExtension(FS_SEARCH_LEVEL level) { ::setLateralExtension(m_hCtx, level); }
    inline void setMeasurementAccuracy(float val) { ::setMeasurementAccuracy(m_hCtx, val); }
    inline void setMeanDistance(float val) { ::setMeanDistance(m_hCtx, val); }

public: // Getter
    inline int getSmartConversionOptions()       const { return ::getSmartConversionOptions(m_hCtx); }
    inline FS_SEARCH_LEVEL getRadialExpansion()  const { return ::getRadialExpansion(m_hCtx); }
    inline FS_SEARCH_LEVEL getLateralExtension() const { return ::getLateralExtension(m_hCtx); }
    inline float getMeasurementAccuracy()        const { return ::getMeasurementAccuracy(m_hCtx); }
    inline float getMeanDistance()               const { return ::getMeanDistance(m_hCtx); }

public:
    inline FS_ERROR setPointCloudDataFloat( const void *data, unsigned int count, unsigned int stride = 0 ) {
        return ::setPointCloudFloat( m_hCtx, data, count, stride );
    }

    inline FS_ERROR setPointCloudDataDouble( const void *data, unsigned int count, unsigned int stride = 0 ) {
        return ::setPointCloudDouble( m_hCtx, data, count, stride );
    }

    inline void cleanUp() { ::cleanUpFindSurface(m_hCtx); }

public:
    std::unique_ptr<const FindSurfaceResult> findSurface( FS_FEATURE_TYPE type, unsigned int seedIndex, float seedRadius, bool requestInlierFlags = false ) throw( FindSurfaceException::InvalidArgument, FindSurfaceException::InvalidOperation ) {
        FS_FEATURE_RESULT result = { FS_TYPE_NONE, };
        int ret = ::findSurface( m_hCtx, type, seedIndex, seedRadius, &result );
        if( ret ) {
            if( ret == FS_INVALID_VALUE ) {
                throw FindSurfaceException::InvalidArgument(::getFindSurfaceErrorMessage(m_hCtx));
            }
            else if( ret == FS_INVALID_OPERATION ) {
                throw FindSurfaceException::InvalidOperation(::getFindSurfaceErrorMessage(m_hCtx));
            }
            return nullptr; // just not found
        }

        std::unique_ptr<const FindSurfaceInlierFlags> flags(nullptr);
        if( requestInlierFlags ) {
            const FindSurfaceInlierFlags *pFlags = new FindSurfaceInlierFlags( ::getInOutlierFlags(m_hCtx), static_cast<size_t>(::getPointCloudCount(m_hCtx)) );
            if(pFlags == nullptr ) {
                throw FindSurfaceException::OutOfMemory("Inlier Flags: Memory allocation failed");
            }
            flags = std::unique_ptr<const FindSurfaceInlierFlags>( pFlags );
        }

        const FindSurfaceResult *pResult = new FindSurfaceResult( result, flags );
        if( pResult == nullptr ) {
            flags.reset();
            throw FindSurfaceException::OutOfMemory("Result Buffer: Memory allocation failed");
        }

        return std::unique_ptr<const FindSurfaceResult>( pResult );
    }
};

#endif // _FIND_SURFACE_HPP_