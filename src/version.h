//
// src/version.h
//
// Bare Metal Sega Genesis
// Project version — the single source of truth. Keep BMSG_VERSION plain
// MAJOR.MINOR.PATCH: tools/mkdist.sh parses it to name release packages, and
// test_version checks its shape. To release: bump it here, add a CHANGELOG.md
// entry, and tag the release commit v<version>. Pure header; no Circle deps.
//

#ifndef _version_h
#define _version_h

#define BMSG_VERSION       "0.1.0"
#define BMSG_VERSION_LABEL "v" BMSG_VERSION   // display form, e.g. "v0.1.0"

#endif
