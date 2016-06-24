#ifndef _ofxMetadata_h_
#define _ofxMetadata_h_

#include "ofxCore.h"
#include "ofxParam.h"
#include "ofxClip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OfxClipMetaDataSuiteV1

/** 
 * @brief String value used to handle a list of metadata.
 * @todo have a new property type, which is another property set (so property sets can be contained in property sets)
 * - Type - String X N
 * - Property Set - plugin descriptor (read/write)
 * - Default - empty string X 0
 * - Values - A array of metadata (key1=value1, key2=value2...).
 * 
 */
#define kTuttleOfxPropMetadata "kTuttleOfxPropMetadata"

{
	OfxStatus clipMetaDataGetParameterSet( const OfxClipHandle* clip, OfxParamSetHandle* paramSet );
} OfxClipMetaDataSuiteV1;

/*

// Need to exist ?
typedef struct OfxImageMetaDataSuiteV1 {

 OfxStatus imageMetaDataGetParameterSet(const OfxImageHandle* image, OfxParamSetHandle *paramSet);

} OfxImageMetaDataSuiteV1;

*/

#ifdef __cplusplus
}
#endif

#endif

