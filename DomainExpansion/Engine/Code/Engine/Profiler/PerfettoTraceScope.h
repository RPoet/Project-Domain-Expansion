#pragma once

#if defined(_WIN32)
#if defined(min)
#pragma push_macro("min")
#undef min
#define DOMAINEXPANSION_RESTORE_PERFETTO_MIN_MACRO
#endif
#if defined(max)
#pragma push_macro("max")
#undef max
#define DOMAINEXPANSION_RESTORE_PERFETTO_MAX_MACRO
#endif
#endif

#include "ThirdParty/Perfetto/sdk/perfetto.h"

#if defined(_WIN32)
#ifdef DOMAINEXPANSION_RESTORE_PERFETTO_MAX_MACRO
#pragma pop_macro("max")
#undef DOMAINEXPANSION_RESTORE_PERFETTO_MAX_MACRO
#endif
#ifdef DOMAINEXPANSION_RESTORE_PERFETTO_MIN_MACRO
#pragma pop_macro("min")
#undef DOMAINEXPANSION_RESTORE_PERFETTO_MIN_MACRO
#endif
#endif

PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE(
	DomainExpansionPerfetto,
	perfetto::Category("startup").SetDescription("Editor startup and bootstrap phases"),
	perfetto::Category("framework").SetDescription("Framework lifecycle and update phases"),
	perfetto::Category("world_load").SetDescription("World asset loading phases"),
	perfetto::Category("asset").SetDescription("Asset document and binary loading"),
	perfetto::Category("mesh").SetDescription("Mesh component and mesh asset loading"),
	perfetto::Category("xml").SetDescription("XML read and parse phases"),
	perfetto::Category("disk").SetDescription("Disk path resolution and file open phases"),
	perfetto::Category("render").SetDescription("Render-world startup and first-frame phases"));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(DomainExpansionPerfetto);
