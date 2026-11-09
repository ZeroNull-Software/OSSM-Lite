#ifndef UI_TEXT_PAGES_H
#define UI_TEXT_PAGES_H

#include "DisplayTypes.h"
#include "Strings.h"

namespace ui {
    namespace pages {

        using namespace ui::strings;

        static const TextPage errorPage = {
            .title = error,
        };

        static const TextPage wifiDisconnectedPage = {
            .title = wifiSetup,
            .body = wifiBody,
            .bottomText = wifiBottom,
            .qrUrl = wifiQr,
        };

        static const TextPage wifiConnectedPage = {
            .title = wifiSetup,
            .body = wifiConnected,
            .bottomText = longPressReset,
        };

    }  // namespace pages
}  // namespace ui

#endif  // UI_TEXT_PAGES_H
