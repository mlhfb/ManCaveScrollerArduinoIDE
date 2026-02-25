#ifndef RSS_SOURCES_H
#define RSS_SOURCES_H

#include <Arduino.h>

#include "AppTypes.h"

size_t buildRssSources(const AppSettings& settings, RssSource* outSources,
                       size_t maxSources);
bool hasEnabledRssSources(const AppSettings& settings);

#endif
