/**
 * Client dashboard look
 *
 * @brief Turns Client::Theme and Client::Density into a stylesheet and spacing.
 * @date 30-09-2026
 */

#include "client/gui/chat_style.h"

namespace {

struct Palette {
  const char *window;
  const char *header;
  const char *headerBorder;
  const char *title;
  const char *muted;
  const char *ghostBorder;
  const char *ghostText;
  const char *ghostHover;
  const char *primary;
  const char *primaryHover;
  const char *primaryDisabled;
  const char *chipBg;
  const char *chipBorder;
  const char *chipText;
  const char *chipHover;
  const char *card;
  const char *cardBorder;
  const char *field;
  const char *fieldBorder;
  const char *ink;
  const char *selection;
  const char *selectedBg;
  const char *selectedFg;
  const char *dialog;
};

const Palette kLight = {
    "#eef1f6", "#ffffff", "#e2e8f0", "#0f172a", "#64748b", "#cbd5e1", "#334155", "#f8fafc",
    "#4f46e5", "#4338ca", "#c7d2fe", "#eef2ff", "#c7d2fe", "#3730a3", "#e0e7ff", "#ffffff",
    "#e2e8f0", "#f8fafc", "#e2e8f0", "#0f172a", "#c7d2fe", "#eef2ff", "#3730a3", "#ffffff",
};

const Palette kDark = {
    "#0b1220", "#111827", "#1f2937", "#e2e8f0", "#94a3b8", "#334155", "#e2e8f0", "#1e293b",
    "#6366f1", "#4f46e5", "#312e81", "#1e1b4b", "#4338ca", "#c7d2fe", "#312e81", "#111827",
    "#1f2937", "#0f172a", "#1f2937", "#e2e8f0", "#4338ca", "#1e1b4b", "#c7d2fe", "#111827",
};

const char *kSheet = R"(
    QMainWindow { background: {{window}}; }
    QWidget#Page { background: {{window}}; }
    QDialog, QMessageBox { background: {{dialog}}; color: {{ink}}; }
    QMessageBox QLabel { color: {{ink}}; }

    QWidget#Header {
      background: {{header}};
      border-bottom: 1px solid {{headerBorder}};
    }

    QWidget#PopUpWindow { background: {{dialog}}; color: {{ink}}; }

    QLabel { color: {{ink}}; }
    QLabel#PageTitle { font-size: 20px; font-weight: 700; color: {{title}}; }
    QLabel#PageSubtitle { font-size: 12px; color: {{muted}}; }
    QLabel#SectionTitle {
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.6px;
      color: {{muted}};
    }
    QLabel#RoomTitle { font-size: 16px; font-weight: 700; color: {{title}}; }

    QPushButton#GhostButton {
      background: transparent;
      border: 1px solid {{ghostBorder}};
      border-radius: 8px;
      padding: 8px 14px;
      color: {{ghostText}};
      font-weight: 600;
    }
    QPushButton#GhostButton:hover { background: {{ghostHover}}; }

    QPushButton#PrimaryButton {
      background: {{primary}};
      border: none;
      border-radius: 8px;
      padding: 8px 16px;
      color: #ffffff;
      font-weight: 600;
    }
    QPushButton#PrimaryButton:hover { background: {{primaryHover}}; }
    QPushButton#PrimaryButton:disabled { background: {{primaryDisabled}}; color: #e2e8f0; }

    QPushButton#UserChip {
      background: {{chipBg}};
      border: 1px solid {{chipBorder}};
      border-radius: 18px;
      padding: 6px 14px;
      color: {{chipText}};
      font-weight: 600;
    }
    QPushButton#UserChip:hover { background: {{chipHover}}; }

    QWidget#SideCard, QWidget#ChatCard {
      background: {{card}};
      border: 1px solid {{cardBorder}};
      border-radius: 14px;
    }

    QListWidget, QPlainTextEdit, QLineEdit, QComboBox {
      background: {{field}};
      border: 1px solid {{fieldBorder}};
      border-radius: 10px;
      padding: 8px;
      color: {{ink}};
      selection-background-color: {{selection}};
    }
    QComboBox QAbstractItemView {
      background: {{field}};
      color: {{ink}};
      selection-background-color: {{selectedBg}};
      selection-color: {{selectedFg}};
    }

    QListWidget::item { padding: 8px; border-radius: 8px; }
    QListWidget::item:selected { background: {{selectedBg}}; color: {{selectedFg}}; }
)";

const char *kCompact = R"(
    QLabel#PageTitle { font-size: 16px; }
    QLabel#PageSubtitle { font-size: 11px; }
    QLabel#SectionTitle { font-size: 11px; }
    QLabel#RoomTitle { font-size: 14px; }
    QPushButton#GhostButton, QPushButton#PrimaryButton {
      border-radius: 6px;
      padding: 4px 10px;
    }
    QPushButton#UserChip { padding: 2px 10px; }
    QListWidget, QPlainTextEdit, QLineEdit, QComboBox { padding: 4px; font-size: 12px; }
    QListWidget::item { padding: 2px 6px; }
)";

QString paint(const char *sheet, const Palette &palette) {
  QString out = QString::fromUtf8(sheet);
  const struct {
    const char *token;
    const char *value;
  } replacements[] = {
      {"{{window}}", palette.window},
      {"{{header}}", palette.header},
      {"{{headerBorder}}", palette.headerBorder},
      {"{{title}}", palette.title},
      {"{{muted}}", palette.muted},
      {"{{ghostBorder}}", palette.ghostBorder},
      {"{{ghostText}}", palette.ghostText},
      {"{{ghostHover}}", palette.ghostHover},
      {"{{primary}}", palette.primary},
      {"{{primaryHover}}", palette.primaryHover},
      {"{{primaryDisabled}}", palette.primaryDisabled},
      {"{{chipBg}}", palette.chipBg},
      {"{{chipBorder}}", palette.chipBorder},
      {"{{chipText}}", palette.chipText},
      {"{{chipHover}}", palette.chipHover},
      {"{{card}}", palette.card},
      {"{{cardBorder}}", palette.cardBorder},
      {"{{field}}", palette.field},
      {"{{fieldBorder}}", palette.fieldBorder},
      {"{{ink}}", palette.ink},
      {"{{selection}}", palette.selection},
      {"{{selectedBg}}", palette.selectedBg},
      {"{{selectedFg}}", palette.selectedFg},
      {"{{dialog}}", palette.dialog},
  };
  for (const auto &entry : replacements) {
    out.replace(QString::fromUtf8(entry.token), QString::fromUtf8(entry.value));
  }
  return out;
}

} // namespace

QString chatStyleSheet(ChatLook::Theme theme, ChatLook::Density density) {
  const Palette &palette = theme == ChatLook::Theme::Dark ? kDark : kLight;
  QString sheet = paint(kSheet, palette);
  if (density == ChatLook::Density::Compact) {
    sheet += QString::fromUtf8(kCompact);
  }
  return sheet;
}

ChatSpacing spacingFor(ChatLook::Density density) {
  if (density == ChatLook::Density::Compact) {
    return {8, 8, 8, 6, 220, 12, 8, 8};
  }
  return {20, 16, 16, 10, 280, 24, 16, 16};
}
