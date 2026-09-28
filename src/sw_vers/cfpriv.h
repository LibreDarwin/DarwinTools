/*
 * Private CoreFoundation surface used by sw_vers.
 *
 * CFPriv.h is not shipped in the macOS SDK, so the handful of private
 * entry points the tool needs are declared here and linked against
 * libcfprivate.tbd next to this file.  See that file for why a stub is
 * needed at all.
 */

#ifndef SW_VERS_CFPRIV_H
#define SW_VERS_CFPRIV_H

#include <CoreFoundation/CoreFoundation.h>

CF_EXPORT CFDictionaryRef _CFCopySupplementalVersionDictionary(void);

CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionExtraKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;

#endif /* SW_VERS_CFPRIV_H */
