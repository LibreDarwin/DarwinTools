/*
 * Copyright (c) 2005 Finlay Dobbie
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Finlay Dobbie nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfpriv.h"

/* Values for the "mode" variable, i.e. which field a command line option
 * selects.  SW_VERS_ALL prints the usual multi-line report. */
enum {
	SW_VERS_ALL = 0,
	SW_VERS_PRODUCT_NAME,
	SW_VERS_PRODUCT_VERSION,
	SW_VERS_PRODUCT_VERSION_EXTRA,
	SW_VERS_BUILD_VERSION,
	SW_VERS_BUILD_ID,
	SW_VERS_XBS_VERSION,
	SW_VERS_RELEASE_TYPE
};

static const char usage_text[] =
	"Usage: sw_vers [--help|--productName|--productVersion|--productVersionExtra|--buildVersion]";

static const char *const load_error =
	"**** ERROR **** Unknown error loading system version plist: missing or malformed\n";

/* Each accepted long option exists in three spellings, so scripts written
 * against any of the historical casings keep working. */
static const struct option longopts[] = {
	{ "productName",         no_argument,       NULL, 'n' },
	{ "ProductName",         no_argument,       NULL, 'n' },
	{ "productname",         no_argument,       NULL, 'n' },
	{ "productVersion",      no_argument,       NULL, 'v' },
	{ "ProductVersion",      no_argument,       NULL, 'v' },
	{ "productversion",      no_argument,       NULL, 'v' },
	{ "productVersionExtra", no_argument,       NULL, 'e' },
	{ "ProductVersionExtra", no_argument,       NULL, 'e' },
	{ "productversionextra", no_argument,       NULL, 'e' },
	{ "buildVersion",        no_argument,       NULL, 'b' },
	{ "BuildVersion",        no_argument,       NULL, 'b' },
	{ "buildversion",        no_argument,       NULL, 'b' },
	{ "XBSVersion",          no_argument,       NULL, 'x' },
	{ "xbsversion",          no_argument,       NULL, 'x' },
	{ "releaseType",         no_argument,       NULL, 'r' },
	{ "ReleaseType",         no_argument,       NULL, 'r' },
	{ "releasetype",         no_argument,       NULL, 'r' },
	{ "buildID",             no_argument,       NULL, 'i' },
	{ "BuildID",             no_argument,       NULL, 'i' },
	{ "buildid",             no_argument,       NULL, 'i' },
	{ "plist",               required_argument, NULL, 'P' },
	{ "help",                no_argument,       NULL, 'h' },
	{ NULL,                  0,                 NULL, 0 }
};

static void
usage(int status)
{
	puts(usage_text);
	exit(status);
}

/* Copy a CFString into a malloc'd C string.  Returns 1 on success, in which
 * case *out is a string the caller owns. */
static int
copy_cfstring(CFStringRef str, char **out)
{
	const char *fast;
	char *buf;
	CFIndex length, needed = 0;

	*out = NULL;
	if (str == NULL)
		return 0;

	fast = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
	if (fast != NULL) {
		*out = strdup(fast);
		return *out != NULL;
	}

	/* Not representable in the internal encoding, so widen it by hand.
	 * Two passes are needed: one to measure, one to convert.  A single
	 * pass sized by the character count would truncate, because one
	 * character can turn into several UTF-8 bytes. */
	length = CFStringGetLength(str);
	CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingUTF8, 0,
	    false, NULL, 0, &needed);
	if (needed == 0 && length != 0)
		return 0;

	buf = calloc(1, (size_t)needed + 1);
	if (buf == NULL)
		return 0;
	if (CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingUTF8,
	    0, false, (UInt8 *)buf, needed, NULL) == 0) {
		free(buf);
		return 0;
	}
	*out = buf;
	return 1;
}

/* Look key up in dict and store a C copy of its value in *out.  A missing
 * key leaves *out NULL. */
static void
copy_value(CFDictionaryRef dict, CFStringRef key, char **out)
{
	CFTypeRef value = NULL;

	*out = NULL;
	if (dict != NULL && CFDictionaryGetValueIfPresent(dict, key, &value))
		copy_cfstring((CFStringRef)value, out);
}

/* Read a system version property list from path.  On failure returns 0 and
 * stores a malloc'd description of what went wrong in *errmsg. */
static int
load_plist(const char *path, CFDictionaryRef *out_dict, char **errmsg)
{
	CFStringRef cfpath = NULL;
	CFURLRef url = NULL;
	CFDataRef data = NULL;
	CFErrorRef error = NULL;
	Boolean ok;

	*errmsg = NULL;

	cfpath = CFStringCreateWithCString(kCFAllocatorDefault, path,
	    kCFStringEncodingUTF8);
	if (cfpath == NULL) {
		*errmsg = strdup("Could not load plist from disk");
		return 0;
	}

	url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfpath,
	    false, false);
	CFRelease(cfpath);
	if (url == NULL) {
		*errmsg = strdup("Could not load plist from disk");
		return 0;
	}

	ok = CFURLCreateDataAndPropertiesFromResource(NULL, url, &data, NULL,
	    NULL, NULL);
	CFRelease(url);
	if (!ok) {
		*errmsg = strdup("Could not load plist from disk");
		return 0;
	}

	*out_dict = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListImmutable, NULL, &error);
	CFRelease(data);

	if (*out_dict == NULL) {
		CFStringRef desc = (error != NULL) ?
		    CFErrorCopyDescription(error) : NULL;
		if (desc == NULL || !copy_cfstring(desc, errmsg))
			*errmsg = strdup("Unknown error");
		if (desc != NULL)
			CFRelease(desc);
		if (error != NULL)
			CFRelease(error);
		return 0;
	}

	if (error != NULL)
		CFRelease(error);
	return 1;
}

int
main(int argc, char *argv[])
{
	CFDictionaryRef dict = NULL;
	char *plist_path = NULL;
	char *product_name = NULL, *product_version = NULL, *version_extra = NULL;
	char *build_version = NULL, *build_id = NULL, *release_type = NULL;
	int mode = SW_VERS_ALL;
	int ch;

	while ((ch = getopt_long_only(argc, argv, "nvebxriP:h", longopts,
	    NULL)) != -1) {
		switch (ch) {
		case 'n':
			mode = SW_VERS_PRODUCT_NAME;
			break;
		case 'v':
			mode = SW_VERS_PRODUCT_VERSION;
			break;
		case 'e':
			mode = SW_VERS_PRODUCT_VERSION_EXTRA;
			break;
		case 'b':
			mode = SW_VERS_BUILD_VERSION;
			break;
		case 'i':
			mode = SW_VERS_BUILD_ID;
			break;
		case 'x':
			mode = SW_VERS_XBS_VERSION;
			break;
		case 'r':
			mode = SW_VERS_RELEASE_TYPE;
			break;
		case 'P':
			free(plist_path);
			plist_path = strdup(optarg);
			break;
		case 'h':
			usage(0);
			break;
		default:
			usage(1);
			break;
		}
	}

	if (plist_path != NULL) {
		char *errmsg = NULL;

		if (!load_plist(plist_path, &dict, &errmsg)) {
			fprintf(stderr, "**** ERROR **** Unable to load system "
			    "version from path '%s': %s\n", plist_path, errmsg);
			free(errmsg);
			exit(1);
		}
		free(plist_path);
	} else {
		dict = _CFCopySupplementalVersionDictionary();
	}

	if (dict == NULL) {
		fputs(load_error, stderr);
		exit(1);
	}

	copy_value(dict, _kCFSystemVersionProductNameKey, &product_name);
	copy_value(dict, _kCFSystemVersionProductVersionKey, &product_version);
	copy_value(dict, _kCFSystemVersionProductVersionExtraKey, &version_extra);
	copy_value(dict, _kCFSystemVersionBuildVersionKey, &build_version);
	copy_value(dict, CFSTR("ReleaseType"), &release_type);
	copy_value(dict, CFSTR("BuildID"), &build_id);
	CFRelease(dict);

	switch (mode) {
	case SW_VERS_ALL:
		printf("ProductName:\t\t%s\n", product_name);
		printf("ProductVersion:\t\t%s\n", product_version);
		if (version_extra != NULL)
			printf("ProductVersionExtra:\t%s\n", version_extra);
		printf("BuildVersion:\t\t%s\n", build_version);
		if (release_type != NULL)
			printf("ReleaseType:\t\t%s\n", release_type);
		break;
	case SW_VERS_PRODUCT_NAME:
		puts(product_name);
		break;
	case SW_VERS_PRODUCT_VERSION:
		puts(product_version);
		break;
	case SW_VERS_PRODUCT_VERSION_EXTRA:
		if (version_extra != NULL)
			puts(version_extra);
		break;
	case SW_VERS_BUILD_VERSION:
		puts(build_version);
		break;
	case SW_VERS_BUILD_ID:
		if (build_id != NULL)
			puts(build_id);
		break;
	case SW_VERS_XBS_VERSION:
		break;
	case SW_VERS_RELEASE_TYPE:
		if (release_type != NULL)
			puts(release_type);
		break;
	}

	free(product_name);
	free(product_version);
	free(version_extra);
	free(build_version);
	free(build_id);
	free(release_type);
	return 0;
}
