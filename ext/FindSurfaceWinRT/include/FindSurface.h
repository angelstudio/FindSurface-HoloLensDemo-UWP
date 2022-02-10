#ifndef _FIND_SURFACE_H_
#define _FIND_SURFACE_H_

#ifndef __DECL__
	#if defined(_WIN32) || defined(_WIN64)
		#define __DECL__ __declspec( dllimport )
	#else
		#define __DECL__
	#endif /* defined(_WIN32) || defined(_WIN64) */
#endif /* __DECL__ */

#ifdef __cplusplus
extern "C" {
#endif

typedef void* FIND_SURFACE_CONTEXT;

typedef enum {
	FS_NO_ERROR            =  0,
	FS_OUT_OF_MEMORY       = -1,
	FS_INVALID_OPERATION   = -2,
	FS_INVALID_VALUE       = -3,
	FS_NOT_FOUND           = -100,
	FS_UNACCEPTABLE_RESULT = -101
} FS_ERROR;

typedef enum {
	FS_TYPE_ANY      = 0,
	FS_TYPE_PLANE    = 1,
	FS_TYPE_SPHERE   = 2,
	FS_TYPE_CYLINDER = 3,
	FS_TYPE_CONE     = 4,
	FS_TYPE_TORUS    = 5,
/*
	* > Reservation
	FS_TYPE_BOX      = 6,
	FS_TYPE_SOR      = 7
*/
	FS_TYPE_NONE     = 0xFFFFFFFF
} FS_FEATURE_TYPE;

typedef enum {
	FS_LEVEL_0 = 0, FS_LEVEL_OFF = 0,
	FS_LEVEL_1 = 1, FS_LEVEL_MODERATE = 1,
	FS_LEVEL_2 = 2,
	FS_LEVEL_3 = 3,
	FS_LEVEL_4 = 4,
	FS_LEVEL_5 = 5, FS_LEVEL_DEFAULT = 5,
	FS_LEVEL_6 = 6,
	FS_LEVEL_7 = 7,
	FS_LEVEL_8 = 8,
	FS_LEVEL_9 = 9,
	FS_LEVEL_10 = 10, FS_LEVEL_RADICAL = 10
} FS_SEARCH_LEVEL;

typedef enum {
	FS_SCO_NONE              = 0,
	FS_SCO_CONE_TO_CYLINDER  = (1<<0),
	FS_SCO_TORUS_TO_SPHERE   = (1<<1),
	FS_SCO_TORUS_TO_CYLINDER = (1<<2),
} FS_SMART_CONVERSION_OPTION;

typedef struct {
	FS_FEATURE_TYPE type;
	float           rms;

	union {
		float reserved[14];

		struct {
			float ll[3]; // lower left corner
			float lr[3]; // lower right corner
			float ur[3]; // upper right corner
			float ul[3]; // upper left corner
		}plane_param;

		struct {
			float c[3]; // center
			float r;    // radius of the sphere
		}sphere_param;

		struct {
			float b[3]; // center of bottom circle
			float t[3]; // center of top circle
			float r;    // radius of the cylinder
		}cylinder_param;

		struct {
			float b[3]; // center of bottom circle
			float t[3]; // center of top circle
			float br;   // bottom (larger) radius
			float tr;   // top (smaller) radius of the cone
		}cone_param;

		struct {
			float c[3]; // center
			float n[3]; // axis (normal vector)
			float mr;   // mean radius
			float tr;   // tube (circle) radius of the torus
		}torus_param;
	};
} FS_FEATURE_RESULT;

/**
 * createFindSurface
 *
 * Returns 0 for success, otherwise error
 */
__DECL__ FS_ERROR createFindSurface(FIND_SURFACE_CONTEXT *context);

/**
 * releaseFindSurface
 */
__DECL__ void releaseFindSurface(FIND_SURFACE_CONTEXT context);

/**
 * setPointCloud(Float|Double)
 *
 * pointer [in] : Specifies a offset of the first component of the point cloud data in the array.
 *                Number of components per each point cloud data must be 3 (x, y, z).
 * count   [in] : Specifies the number of the point cloud data
 * stride  [in] : Specifies the byte offset between consecutive point cloud data.
 *                If stride is 0, point cloud data are understood to be tightly packed in the array.
 *
 * Returns 0 for success, otherwise error
 */
__DECL__ FS_ERROR setPointCloudFloat(FIND_SURFACE_CONTEXT context, const void *pointer, unsigned int count, unsigned int stride);
__DECL__ FS_ERROR setPointCloudDouble(FIND_SURFACE_CONTEXT context, const void *pointer, unsigned int count, unsigned int stride);

/**
 * getPointCloudCount
 *
 * Returns the number of points currently set
 */
__DECL__ unsigned int getPointCloudCount(FIND_SURFACE_CONTEXT context);

/**
 * findSurface
 *
 * type        [in]  : Specifies what kind of surface to find. Symbolic constants FS_TYPE_ANY, FS_TYPE_PLANE, FS_TYPE_SPHERE, FS_TYPE_CYLINDER, FS_TYPE_CONE, FS_TYPE_TORUS and FS_TYPE_BOX are accepted.
 * start_index [in]  : Specifies the index of point cloud data to start find surface.
 * result      [out] : Pointer to a buffer that receives the feature parameters.
 *
 * Returns ...
 */
__DECL__ FS_ERROR findSurface(FIND_SURFACE_CONTEXT context, FS_FEATURE_TYPE type, unsigned int start_index, float touchRadius, FS_FEATURE_RESULT *result );

__DECL__ FS_ERROR findStripPlane(FIND_SURFACE_CONTEXT context, unsigned int index_1, unsigned int index_2, float touchRadius, FS_FEATURE_RESULT *result);
__DECL__ FS_ERROR findRodCylinder(FIND_SURFACE_CONTEXT context, unsigned int index_1, unsigned int index_2, float touchRadius, FS_FEATURE_RESULT *result);
__DECL__ FS_ERROR findDiskCylinder(FIND_SURFACE_CONTEXT context, unsigned int index_1, unsigned int index_2, unsigned int index_3, float touchRadius, FS_FEATURE_RESULT *result);
__DECL__ FS_ERROR findDiskCone(FIND_SURFACE_CONTEXT context, unsigned int index_1, unsigned int index_2, unsigned int index_3, float touchRadius, FS_FEATURE_RESULT *result);
__DECL__ FS_ERROR findThinRingTorus(FIND_SURFACE_CONTEXT context, unsigned int index_1, unsigned int index_2, unsigned int index_3, float touchRadius, FS_FEATURE_RESULT *result);

/**
 * getInOutlierFlags
 *
 * Returns the boolean array ( 0 for inliers, otherwise outliers ) of in/outlier of the input point cloud data.
 */
__DECL__ const unsigned char *getInOutlierFlags(FIND_SURFACE_CONTEXT context);

/**
 * getFindSurfaceErrorMessage
 * 
 */
__DECL__ const char *getFindSurfaceErrorMessage(FIND_SURFACE_CONTEXT context);


/**
 * cleanUpFindSurface
 */
__DECL__ void cleanUpFindSurface(FIND_SURFACE_CONTEXT context);

/**
 * Explicit Parameter Setter / Getter
 */
__DECL__ void setRadialExpansion(FIND_SURFACE_CONTEXT context, FS_SEARCH_LEVEL level);
__DECL__ FS_SEARCH_LEVEL getRadialExpansion(FIND_SURFACE_CONTEXT context);

__DECL__ void setLateralExtension(FIND_SURFACE_CONTEXT context, FS_SEARCH_LEVEL level);
__DECL__ FS_SEARCH_LEVEL getLateralExtension(FIND_SURFACE_CONTEXT context);

__DECL__ void setMeasurementAccuracy(FIND_SURFACE_CONTEXT context, float accuracy);
__DECL__ float getMeasurementAccuracy(FIND_SURFACE_CONTEXT context);

__DECL__ void setMeanDistance(FIND_SURFACE_CONTEXT context, float distance);
__DECL__ float getMeanDistance(FIND_SURFACE_CONTEXT context);

__DECL__ void setSmartConversionOptions(FIND_SURFACE_CONTEXT context, int options);
__DECL__ int getSmartConversionOptions(FIND_SURFACE_CONTEXT context);

#ifdef __cplusplus
}
#endif

#undef __DECL__

#endif /* _FIND_SURF_H_ */
