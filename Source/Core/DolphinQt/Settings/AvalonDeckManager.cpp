// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AvalonDeckManager.h"

#include <ranges>

#include <QAbstractTableModel>
#include <QCheckBox>
#include <QCollator>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

#include "Core/HW/Triforce/DeckReader.h"

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/Settings.h"

namespace
{

constexpr int COLUMN_CARD_NUMBER = 0;
constexpr int COLUMN_CARD_NAME_ENG = 1;
constexpr int COLUMN_CARD_NAME_JPN = 2;
constexpr int COLUMN_CARD_ATTRIBUTE = 3;
constexpr int COLUMN_CARD_MOVEMENT = 4;
constexpr int COLUMN_CARD_ATTACK = 5;
constexpr int COLUMN_CARD_DEFENSE = 6;
constexpr int COLUMN_CARD_QUANTITY = 7;
constexpr int COLUMN_COUNT = 8;

constexpr int MAXIMUM_DECK_SIZE = 30;

QColor GetColorForAvalonAttribChar(QChar c)
{
  static const QHash<QChar, QColor> pip_color_map = {
      {u'y', 0xffcc00}, {u'b', 0x003399}, {u'r', 0xcd212a},
      {u'g', 0x008c45}, {u'm', 0x942b9d}, {u'w', Qt::white},
  };

  return pip_color_map.value(c, Qt::gray);
}

// Natural number/color sorting and filtering by text string.
class NaturalSortFilterProxy final : public QSortFilterProxyModel
{
  using QSortFilterProxyModel::QSortFilterProxyModel;

public:
  void SetFilterText(QString text)
  {
    if (m_search_text == text)
      return;

    m_search_text = std::move(text);
    invalidateFilter();
  }

  void SetShowAllCards(bool show_all_cards)
  {
    if (m_show_all_cards == show_all_cards)
      return;

    m_show_all_cards = show_all_cards;
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int row, const QModelIndex& parent) const override
  {
    const auto* model = sourceModel();

    if (!m_show_all_cards &&
        model->data(model->index(row, COLUMN_CARD_QUANTITY, parent)).toInt() == 0)
    {
      return false;
    }

    if (m_search_text.isEmpty())
      return true;

    for (int column = 0; column != COLUMN_CARD_QUANTITY; ++column)
    {
      const QModelIndex index = model->index(row, column, parent);

      if (model->data(index, Qt::DisplayRole)
              .toString()
              .contains(m_search_text, Qt::CaseInsensitive))
      {
        return true;
      }
    }

    return false;
  }

  bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
  {
    const auto left_str = sourceModel()->data(left, sortRole()).toString();
    const auto right_str = sourceModel()->data(right, sortRole()).toString();

    if (left.column() == COLUMN_CARD_MOVEMENT || left.column() == COLUMN_CARD_ATTRIBUTE)
      return CompareMovementStrings(left_str, right_str);

    static QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    return collator.compare(left_str, right_str) < 0;
  }

private:
  static bool CompareMovementStrings(const QString& left, const QString& right)
  {
    for (const auto [a, b] : std::views::zip(left, right))
    {
      if (a != b)
        return RankAvalonColorChar(a) < RankAvalonColorChar(b);
    }

    return left.size() < right.size();
  }

  static int RankAvalonColorChar(QChar c)
  {
    // Sort the colors in Avalon's de-facto order.
    return int(QStringView{u"ybrgwms"}.indexOf(c));
  }

  QString m_search_text;
  bool m_show_all_cards{};
};

class DeckModel final : public QAbstractTableModel
{
public:
  explicit DeckModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

  void LoadDeck()
  {
    const auto card_database = Triforce::LoadCardDatabase();
    ResetModel(card_database, Triforce::LoadCardDeck(card_database));
  }

  void LoadDefaultDeck()
  {
    const auto card_database = Triforce::LoadCardDatabase();
    ResetModel(card_database, Triforce::LoadDefaultCardDeck(card_database));
  }

  void SaveDeck()
  {
    std::vector<Triforce::DeckEntry> deck;

    for (auto& card : m_data)
    {
      if (card.quantity != 0)
        deck.emplace_back(card.number.toStdString(), card.quantity);
    }

    Triforce::SaveCardDeck(deck);
  }

  int GetDeckSize() const
  {
    int total = 0;
    for (const auto& card : m_data)
      total += card.quantity;
    return total;
  }

  void ClearDeck()
  {
    int row = 0;
    for (auto& card : m_data)
    {
      if (std::exchange(card.quantity, 0) != 0)
      {
        const auto model_index = index(row, COLUMN_CARD_QUANTITY);
        emit dataChanged(model_index, model_index, {Qt::DisplayRole});
      }

      ++row;
    }
  }

  int rowCount(const QModelIndex&) const override { return static_cast<int>(m_data.size()); }

  int columnCount(const QModelIndex&) const override { return COLUMN_COUNT; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    static const QHash<QStringView, QString> attribute_names = {
        {QStringLiteral("y"), tr("Yellow")},
        {QStringLiteral("b"), tr("Blue")},
        {QStringLiteral("r"), tr("Red")},
        {QStringLiteral("g"), tr("Green")},
        // i18n: "The Key of Avalon" card attribute name. Japanese: マップ上魔法
        // Keep it concise. It's displayed many times in a table.
        {QStringLiteral("m"), tr("Magic")},
        // i18n: "The Key of Avalon" card attribute name. Japanese: 戦闘支援
        // Keep it concise. It's displayed many times in a table.
        {QStringLiteral("s"), tr("Support")},
    };

    if (role == Qt::TextAlignmentRole && index.column() == COLUMN_CARD_QUANTITY)
      return Qt::AlignCenter;

    if ((role != Qt::DisplayRole && role != Qt::EditRole))
      return {};

    const auto& card = m_data[index.row()];

    switch (index.column())
    {
    case COLUMN_CARD_NUMBER:
      return card.number;
    case COLUMN_CARD_NAME_ENG:
      return card.name_eng;
    case COLUMN_CARD_NAME_JPN:
      return card.name_jpn;
    case COLUMN_CARD_ATTRIBUTE:
      // Return an untranslated string for EditRole which we use for sorting and coloring.
      return (role == Qt::EditRole) ? card.attribute :
                                      attribute_names.value(card.attribute, card.attribute);
    case COLUMN_CARD_MOVEMENT:
      return card.movement;
    case COLUMN_CARD_ATTACK:
      return card.attack;
    case COLUMN_CARD_DEFENSE:
      return card.defense;
    case COLUMN_CARD_QUANTITY:
      // Display zero as empty string so cards not in the deck are distinctly visible.
      return (card.quantity != 0) ? QVariant{card.quantity} : QString{};
    default:
      return {};
    }
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if (role != Qt::DisplayRole)
      return {};

    if (orientation == Qt::Horizontal)
    {
      switch (section)
      {
      case COLUMN_CARD_NUMBER:
        return QStringLiteral("#");
      case COLUMN_CARD_NAME_ENG:
        return tr("English Name");
      case COLUMN_CARD_NAME_JPN:
        return tr("Japanese Name");
      case COLUMN_CARD_ATTRIBUTE:
        // i18n: Column header for "The Key of Avalon" card category. Japanese: 属性
        return tr("Attribute");
      case COLUMN_CARD_MOVEMENT:
        // i18n: Column header for "The Key of Avalon" card movement color pips.
        // The Japanese, 移動色, is more like "Moving Color", but keep it concise.
        // The column doesn't need to be overly wide and "Movement" is descriptive enough.
        return tr("Movement");
      case COLUMN_CARD_ATTACK:
        // i18n: Column header for "The Key of Avalon" card "attack" value. Japanese: 攻撃
        return tr("Atk");
      case COLUMN_CARD_DEFENSE:
        // i18n: Column header for "The Key of Avalon" card "defense" value. Japanese: 耐久
        return tr("Def");
      case COLUMN_CARD_QUANTITY:
        return tr("Quantity");
      default:
        return {};
      }
    }

    return {};
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    auto flags = QAbstractTableModel::flags(index);

    if (index.column() == COLUMN_CARD_QUANTITY)
      flags |= Qt::ItemIsEditable;

    return flags;
  }

  bool setData(const QModelIndex& index, const QVariant& value, int role) override
  {
    if (index.column() != COLUMN_CARD_QUANTITY || role != Qt::EditRole)
      return false;

    m_data[index.row()].quantity = value.toInt();

    emit dataChanged(index, index, {Qt::DisplayRole});
    return true;
  }

private:
  void ResetModel(const Triforce::CardDatabase& card_database,
                  const std::optional<Triforce::CardDeck>& card_deck)
  {
    beginResetModel();

    m_data.clear();
    m_data.resize(card_database.size());

    std::size_t row = 0;
    for (const auto& [card_number, card_details] : card_database)
    {
      auto& card = m_data[row++];

      card.number = QString::fromUtf8(card_number);
      card.name_eng = QString::fromUtf8(card_details.name_eng);
      card.name_jpn = QString::fromUtf8(card_details.name_jpn);
      card.attribute = QString::fromUtf8(card_details.attribute);
      card.movement = QString::fromUtf8(card_details.movement);

      if (card_details.attack)
        card.attack = QString::number(*card_details.attack);
      if (card_details.defense)
        card.defense = QString::number(*card_details.defense);

      if (card_deck)
        card.quantity = int(std::ranges::count(*card_deck, card_details.card_id));
    }

    endResetModel();
  }

  struct CardData
  {
    QString number;
    QString name_eng;
    QString name_jpn;
    QString attribute;
    QString movement;
    QString attack;
    QString defense;
    int quantity{};
  };

  std::vector<CardData> m_data;
};

// Custom drawing for attribute column.
class CardAttributeDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QStyledItemDelegate::paint(painter, opt, index);

    const auto text = index.data(Qt::EditRole).toString();
    if (text.isEmpty())
      return;

    painter->save();

    QPen pen{GetColorForAvalonAttribChar(text.front())};

    pen.setWidth(2);
    painter->setPen(pen);

    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());

    painter->restore();
  }
};

// Custom drawing for movement color pips.
class CardMovementDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    const QString text = index.data(Qt::DisplayRole).toString();

    painter->save();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const int pip_radius = GetPipRadius(option);
    const int pip_spacing = GetPipSpacing(option);

    int x = option.rect.x() + pip_spacing + pip_spacing + pip_radius;
    const int y = option.rect.center().y();

    for (const QChar ch : text)
    {
      // White pips need a dark outline in light themes.
      const bool use_alt_pen = (ch == u'w' && !Settings::Instance().IsThemeDark());
      QColor pen = option.palette.color(use_alt_pen ? QPalette::Text : QPalette::Base);
      pen.setAlphaF(0.75f);
      painter->setPen(pen);

      painter->setBrush(GetColorForAvalonAttribChar(ch));

      painter->drawEllipse(QPoint(x, y), pip_radius, pip_radius);

      x += pip_radius + pip_radius + pip_spacing;
    }

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
  {
    const int pip_diameter = GetPipRadius(option) * 2;
    const int pip_spacing = GetPipSpacing(option);

    const auto text_size = int(index.data(Qt::DisplayRole).toString().size());
    const int width = (pip_spacing * 3) + (text_size * (pip_diameter + pip_spacing));

    return {width, -1};
  }

private:
  static int GetPipRadius(const QStyleOptionViewItem& option)
  {
    return (QFontMetrics{option.font}.height() * 3 / 8) + 1;
  }

  static int GetPipSpacing(const QStyleOptionViewItem& option)
  {
    return option.widget->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, &option,
                                               option.widget);
  }
};

class CardQuantityDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                        const QModelIndex& /*index*/) const override
  {
    class SpinBox final : public QSpinBox
    {
    public:
      using QSpinBox::QSpinBox;

      void fixup(QString& input) const override
      {
        // Clearing the spinbox removes the card.
        if (input.isEmpty())
          input = QStringLiteral("0");
      }

      void focusInEvent(QFocusEvent* event) override
      {
        QSpinBox::focusInEvent(event);
        lineEdit()->selectAll();
      }
    };

    auto* const spin_box = new SpinBox{parent};

    spin_box->setMinimum(0);
    spin_box->setMaximum(MAXIMUM_DECK_SIZE);

    return spin_box;
  }
};

}  // namespace

AvalonDeckManager::AvalonDeckManager(QWidget* parent) : QDialog{parent}
{
  setWindowTitle(tr("The Key of Avalon - Deck Manager"));

  auto* const deck_model = new DeckModel{this};

  auto* const proxy = new NaturalSortFilterProxy{this};
  proxy->setSourceModel(deck_model);
  proxy->setSortRole(Qt::EditRole);

  auto* const table_view = new QTableView;
  table_view->setModel(proxy);

  table_view->verticalHeader()->hide();
  table_view->setSelectionMode(QAbstractItemView::SingleSelection);

  auto* const table_view_header = table_view->horizontalHeader();
  table_view_header->setSectionResizeMode(QHeaderView::ResizeToContents);

  table_view->setSortingEnabled(true);
  table_view->sortByColumn(COLUMN_CARD_NUMBER, Qt::SortOrder::AscendingOrder);

  table_view->setItemDelegateForColumn(COLUMN_CARD_ATTRIBUTE,
                                       new CardAttributeDelegate(table_view));
  table_view->setItemDelegateForColumn(COLUMN_CARD_MOVEMENT, new CardMovementDelegate(table_view));
  table_view->setItemDelegateForColumn(COLUMN_CARD_QUANTITY, new CardQuantityDelegate(table_view));

  auto* const main_layout = new QVBoxLayout{this};

  auto* const top_row = new QHBoxLayout;
  main_layout->addLayout(top_row);

  auto* const show_all_cards = new QCheckBox{tr("Show All Cards")};
  top_row->addWidget(show_all_cards);
  top_row->addStretch(1);

  connect(show_all_cards, &QCheckBox::toggled, proxy, &NaturalSortFilterProxy::SetShowAllCards);

  auto* const search_textbox = new QLineEdit;
  search_textbox->setPlaceholderText(tr("Search cards..."));

  connect(search_textbox, &QLineEdit::textChanged, proxy, &NaturalSortFilterProxy::SetFilterText);

  main_layout->addWidget(search_textbox);
  main_layout->addWidget(table_view);

  auto* const button_box = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};

  auto* const load_default_button = new QPushButton(tr("Default"));
  connect(load_default_button, &QPushButton::clicked, deck_model, &DeckModel::LoadDefaultDeck);

  auto* const clear_button = new QPushButton(tr("Clear"));
  connect(clear_button, &QPushButton::clicked, deck_model, &DeckModel::ClearDeck);

  button_box->addButton(load_default_button, QDialogButtonBox::ActionRole);
  button_box->addButton(clear_button, QDialogButtonBox::ActionRole);

  connect(button_box, &QDialogButtonBox::accepted, this, &AvalonDeckManager::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &AvalonDeckManager::reject);

  connect(this, &AvalonDeckManager::accepted, deck_model, &DeckModel::SaveDeck);

  auto* const deck_size_label = new QLabel;
  top_row->addWidget(deck_size_label);

  const auto update_label_text = [=]() {
    deck_size_label->setText(
        // i18n: Label for current count and limit of a deck of cards.
        tr("Deck Size: %1 / %2").arg(deck_model->GetDeckSize()).arg(MAXIMUM_DECK_SIZE));
  };
  connect(deck_model, &QAbstractItemModel::dataChanged, this, update_label_text);
  connect(deck_model, &QAbstractItemModel::modelReset, this, update_label_text);

  main_layout->addWidget(button_box);

  // Adjust the window size, accounting for the column widths, before loading the data.
  table_view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  table_view->setMinimumHeight(QFontMetrics{font()}.height() * 30);
  QtUtils::AdjustSizeWithinScreen(this);
  table_view->setMinimumHeight(0);

  deck_model->LoadDeck();

  table_view_header->setSectionResizeMode(COLUMN_CARD_NAME_ENG, QHeaderView::Stretch);
  table_view_header->setSectionResizeMode(COLUMN_CARD_NAME_JPN, QHeaderView::Stretch);

  // If the deck contains any cards, only show those cards by default.
  show_all_cards->setChecked(deck_model->GetDeckSize() == 0);
}
