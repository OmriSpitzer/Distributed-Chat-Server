/**
 * Client dashboard look
 *
 * @brief Stylesheet and spacing for the theme and density Client already chose.
 * @date 30-09-2026
 */

#pragma once
#include "client/appearance.h"
#include <QString>

struct ChatSpacing {
  int bodyMargin;
  int rowSpacing;
  int cardMargin;
  int cardSpacing;
  int sideWidth;
  int headerMarginH;
  int headerMarginV;
  int headerSpacing;
};

// full application stylesheet for one theme and one density
QString chatStyleSheet(ChatLook::Theme theme, ChatLook::Density density);

// layout numbers for comfortable or compact
ChatSpacing spacingFor(ChatLook::Density density);
