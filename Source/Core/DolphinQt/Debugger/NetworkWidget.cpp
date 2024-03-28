// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/NetworkWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

namespace
{
QTableWidgetItem* GetSocketDomain(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr sa;
  socklen_t sa_len = sizeof(sa);
  const int ret = getsockname(host_fd, &sa, &sa_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (sa.sa_family)
  {
  case 2:
    return new QTableWidgetItem(QStringLiteral("AF_INET"));
  case 23:
    return new QTableWidgetItem(QStringLiteral("AF_INET6"));
  default:
    return new QTableWidgetItem(QString::number(sa.sa_family));
  }
}

QTableWidgetItem* GetSocketType(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  int so_type;
  socklen_t opt_len = sizeof(so_type);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&so_type), &opt_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (so_type)
  {
  case 1:
    return new QTableWidgetItem(QStringLiteral("SOCK_STREAM"));
  case 2:
    return new QTableWidgetItem(QStringLiteral("SOCK_DGRAM"));
  default:
    return new QTableWidgetItem(QString::number(so_type));
  }
}

QTableWidgetItem* GetSocketState(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) == 0)
    return new QTableWidgetItem(QTableWidget::tr("Connected"));

  int so_accept = 0;
  socklen_t opt_len = sizeof(so_accept);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&so_accept), &opt_len);
  if (ret == 0 && so_accept > 0)
    return new QTableWidgetItem(QTableWidget::tr("Listening"));
  return new QTableWidgetItem(QTableWidget::tr("Unbound"));
}

static QTableWidgetItem* GetSocketBlocking(const IOS::HLE::WiiSockMan& socket_manager, s32 wii_fd)
{
  if (socket_manager.GetHostSocket(wii_fd) < 0)
    return new QTableWidgetItem();
  const bool is_blocking = socket_manager.IsSocketBlocking(wii_fd);
  return new QTableWidgetItem(is_blocking ? QTableWidget::tr("Yes") : QTableWidget::tr("No"));
}

static QString GetAddressAndPort(const sockaddr_in& addr)
{
  char buffer[16];
  const char* addr_str = inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
  if (!addr_str)
    return {};

  return QStringLiteral("%1:%2").arg(QString::fromLatin1(addr_str)).arg(ntohs(addr.sin_port));
}

QTableWidgetItem* GetSocketName(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in sock_addr;
  socklen_t sock_addr_len = sizeof(sockaddr_in);
  if (getsockname(host_fd, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len) != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  const QString sock_name = GetAddressAndPort(sock_addr);
  if (sock_name.isEmpty())
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) != 0)
    return new QTableWidgetItem(sock_name);

  const QString peer_name = GetAddressAndPort(peer_addr);
  if (peer_name.isEmpty())
    return new QTableWidgetItem(sock_name);

  return new QTableWidgetItem(QStringLiteral("%1->%2").arg(sock_name).arg(peer_name));
}
}  // namespace

NetworkWidget::NetworkWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Network"));
  setObjectName(QStringLiteral("network"));

  setHidden(!Settings::Instance().IsNetworkVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("networkwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("networkwidget/floating")).toBool());

  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &NetworkWidget::Update);

  connect(&Settings::Instance(), &Settings::NetworkVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsNetworkVisible());
  });
}

NetworkWidget::~NetworkWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("networkwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("networkwidget/floating"), isFloating());
}

void NetworkWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetNetworkVisible(false);
}

void NetworkWidget::showEvent(QShowEvent* event)
{
  Update();
}

void NetworkWidget::CreateWidgets()
{
  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);
  layout->addWidget(CreateSocketTableGroup());
  layout->addWidget(CreateSSLContextGroup());
  layout->addWidget(CreateDumpOptionsGroup());
  layout->addWidget(CreateSecurityOptionsGroup());
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
  setWidget(widget);

  Update();
}

void NetworkWidget::ConnectWidgets()
{
  connect(m_dump_format_combo, &QComboBox::currentIndexChanged, this,
          &NetworkWidget::OnDumpFormatComboChanged);
  connect(m_dump_ssl_read_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_READ, state == Qt::Checked);
  });
  connect(m_dump_ssl_write_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_WRITE, state == Qt::Checked);
  });
  connect(m_dump_root_ca_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA, state == Qt::Checked);
  });
  connect(m_dump_peer_cert_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT, state == Qt::Checked);
  });
  connect(m_verify_certificates_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES, state == Qt::Checked);
  });
  connect(m_dump_bba_checkbox, &QCheckBox::stateChanged, [](int state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_DUMP_BBA, state == Qt::Checked);
  });
  connect(m_open_dump_folder, &QPushButton::pressed, [] {
    const std::string location = File::GetUserPath(D_DUMPSSL_IDX);
    const QUrl url = QUrl::fromLocalFile(QString::fromStdString(location));
    QDesktopServices::openUrl(url);
  });
}

void NetworkWidget::Update()
{
  if (!isVisible())
    return;

  auto& system = Core::System::GetInstance();
  if (Core::GetState(system) != Core::State::Paused)
  {
    m_socket_table->setDisabled(true);
    m_ssl_table->setDisabled(true);
    return;
  }

  m_socket_table->setDisabled(false);
  m_ssl_table->setDisabled(false);

  // needed because there's a race condition on the IOS instance otherwise
  const Core::CPUThreadGuard guard(system);

  auto* ios = system.GetIOS();
  if (!ios)
    return;

  auto socket_manager = ios->GetSocketManager();
  if (!socket_manager)
    return;

  m_socket_table->setRowCount(0);
  for (s32 wii_fd = 0; wii_fd < IOS::HLE::WII_SOCKET_FD_MAX; wii_fd++)
  {
    m_socket_table->insertRow(wii_fd);
    const s32 host_fd = socket_manager->GetHostSocket(wii_fd);
    m_socket_table->setItem(wii_fd, 0, new QTableWidgetItem(QString::number(wii_fd)));
    m_socket_table->setItem(wii_fd, 1, GetSocketDomain(host_fd));
    m_socket_table->setItem(wii_fd, 2, GetSocketType(host_fd));
    m_socket_table->setItem(wii_fd, 3, GetSocketState(host_fd));
    m_socket_table->setItem(wii_fd, 4, GetSocketBlocking(*socket_manager, wii_fd));
    m_socket_table->setItem(wii_fd, 5, GetSocketName(host_fd));
  }
  m_socket_table->resizeColumnsToContents();

  m_ssl_table->setRowCount(0);
  for (s32 ssl_id = 0; ssl_id < IOS::HLE::NET_SSL_MAXINSTANCES; ssl_id++)
  {
    m_ssl_table->insertRow(ssl_id);
    s32 host_fd = -1;
    if (IOS::HLE::IsSSLIDValid(ssl_id))
    {
      const auto& ssl = IOS::HLE::NetSSLDevice::_SSL[ssl_id];
      host_fd = ssl.hostfd;
      m_ssl_table->setItem(ssl_id, 5, new QTableWidgetItem(QString::fromStdString(ssl.hostname)));
    }
    m_ssl_table->setItem(ssl_id, 0, new QTableWidgetItem(QString::number(ssl_id)));
    m_ssl_table->setItem(ssl_id, 1, GetSocketDomain(host_fd));
    m_ssl_table->setItem(ssl_id, 2, GetSocketType(host_fd));
    m_ssl_table->setItem(ssl_id, 3, GetSocketState(host_fd));
    m_ssl_table->setItem(ssl_id, 4, GetSocketName(host_fd));
  }
  m_ssl_table->resizeColumnsToContents();

  const bool is_pcap = Config::Get(Config::MAIN_NETWORK_DUMP_AS_PCAP);
  const bool is_ssl_read = Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ);
  const bool is_ssl_write = Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE);

  m_dump_ssl_read_checkbox->setChecked(is_ssl_read);
  m_dump_ssl_write_checkbox->setChecked(is_ssl_write);
  m_dump_root_ca_checkbox->setChecked(Config::Get(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA));
  m_dump_peer_cert_checkbox->setChecked(Config::Get(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT));
  m_verify_certificates_checkbox->setChecked(
      Config::Get(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES));

  const int combo_index = int([is_pcap, is_ssl_read, is_ssl_write]() -> FormatComboId {
    if (is_pcap)
      return FormatComboId::PCAP;
    else if (is_ssl_read && is_ssl_write)
      return FormatComboId::BinarySSL;
    else if (is_ssl_read)
      return FormatComboId::BinarySSLRead;
    else if (is_ssl_write)
      return FormatComboId::BinarySSLWrite;
    else
      return FormatComboId::None;
  }());
  m_dump_format_combo->setCurrentIndex(combo_index);
}

QGroupBox* NetworkWidget::CreateSocketTableGroup()
{
  QGroupBox* socket_table_group = new QGroupBox(tr("Socket table"));
  QGridLayout* socket_table_layout = new QGridLayout;
  socket_table_group->setLayout(socket_table_layout);

  m_socket_table = new QTableWidget();
  // i18n: FD stands for file descriptor (and in this case refers to sockets, not regular files)
  QStringList header{tr("FD"), tr("Domain"), tr("Type"), tr("State"), tr("Blocking"), tr("Name")};
  m_socket_table->setColumnCount(static_cast<int>(header.size()));

  m_socket_table->setHorizontalHeaderLabels(header);
  m_socket_table->setTabKeyNavigation(false);
  m_socket_table->verticalHeader()->setVisible(false);
  m_socket_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_socket_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_socket_table->setWordWrap(false);

  socket_table_layout->addWidget(m_socket_table, 0, 0);
  socket_table_layout->setSpacing(1);
  return socket_table_group;
}

QGroupBox* NetworkWidget::CreateSSLContextGroup()
{
  QGroupBox* ssl_context_group = new QGroupBox(tr("SSL context"));
  QGridLayout* ssl_context_layout = new QGridLayout;
  ssl_context_group->setLayout(ssl_context_layout);

  m_ssl_table = new QTableWidget();
  QStringList header{tr("ID"), tr("Domain"), tr("Type"), tr("State"), tr("Name"), tr("Hostname")};
  m_ssl_table->setColumnCount(static_cast<int>(header.size()));

  m_ssl_table->setHorizontalHeaderLabels(header);
  m_ssl_table->setTabKeyNavigation(false);
  m_ssl_table->verticalHeader()->setVisible(false);
  m_ssl_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_ssl_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_ssl_table->setWordWrap(false);

  ssl_context_layout->addWidget(m_ssl_table, 0, 0);
  ssl_context_layout->setSpacing(1);
  return ssl_context_group;
}

QGroupBox* NetworkWidget::CreateDumpOptionsGroup()
{
  auto* dump_options_group = new QGroupBox(tr("Dump options"));
  auto* dump_options_layout = new QVBoxLayout;
  dump_options_group->setLayout(dump_options_layout);

  m_dump_format_combo = CreateDumpFormatCombo();
  m_dump_format_combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_dump_ssl_read_checkbox = new QCheckBox(tr("Dump decrypted SSL reads"));
  m_dump_ssl_write_checkbox = new QCheckBox(tr("Dump decrypted SSL writes"));
  // i18n: CA stands for certificate authority
  m_dump_root_ca_checkbox = new QCheckBox(tr("Dump root CA certificates"));
  m_dump_peer_cert_checkbox = new QCheckBox(tr("Dump peer certificates"));
  m_dump_bba_checkbox = new QCheckBox(tr("Dump GameCube BBA traffic"));
  m_open_dump_folder = new QPushButton(tr("Open dump folder"));
  m_open_dump_folder->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  auto* combo_label = new QLabel(tr("Network dump format:"));
  combo_label->setBuddy(m_dump_format_combo);
  auto* combo_layout = new QHBoxLayout;
  combo_layout->addWidget(combo_label);
  const int combo_label_space =
      combo_label->fontMetrics().boundingRect(QStringLiteral("__")).width();
  combo_layout->addItem(new QSpacerItem(combo_label_space, 0));
  combo_layout->addWidget(m_dump_format_combo);
  combo_layout->addStretch();
  dump_options_layout->addLayout(combo_layout);

  dump_options_layout->addWidget(m_dump_ssl_read_checkbox);
  dump_options_layout->addWidget(m_dump_ssl_write_checkbox);
  dump_options_layout->addWidget(m_dump_root_ca_checkbox);
  dump_options_layout->addWidget(m_dump_peer_cert_checkbox);
  dump_options_layout->addWidget(m_dump_bba_checkbox);
  dump_options_layout->addWidget(m_open_dump_folder);

  dump_options_layout->setSpacing(1);
  return dump_options_group;
}

QGroupBox* NetworkWidget::CreateSecurityOptionsGroup()
{
  auto* security_options_group = new QGroupBox(tr("Security options"));
  auto* security_options_layout = new QVBoxLayout;
  security_options_group->setLayout(security_options_layout);

  m_verify_certificates_checkbox = new QCheckBox(tr("Verify certificates"));
  security_options_layout->addWidget(m_verify_certificates_checkbox);

  security_options_layout->setSpacing(1);
  return security_options_group;
}

QComboBox* NetworkWidget::CreateDumpFormatCombo()
{
  auto* combo = new QComboBox();

  combo->insertItem(int(FormatComboId::None), tr("None"));
  // i18n: PCAP is a file format
  combo->insertItem(int(FormatComboId::PCAP), tr("PCAP"));
  combo->insertItem(int(FormatComboId::BinarySSL), tr("Binary SSL"));
  combo->insertItem(int(FormatComboId::BinarySSLRead), tr("Binary SSL (read)"));
  combo->insertItem(int(FormatComboId::BinarySSLWrite), tr("Binary SSL (write)"));

  return combo;
}

void NetworkWidget::OnDumpFormatComboChanged(int index)
{
  const auto combo_id = static_cast<FormatComboId>(index);

  switch (combo_id)
  {
  case FormatComboId::PCAP:
    break;
  case FormatComboId::BinarySSL:
    m_dump_ssl_read_checkbox->setChecked(true);
    m_dump_ssl_write_checkbox->setChecked(true);
    m_dump_bba_checkbox->setChecked(false);
    break;
  case FormatComboId::BinarySSLRead:
    m_dump_ssl_read_checkbox->setChecked(true);
    m_dump_ssl_write_checkbox->setChecked(false);
    m_dump_bba_checkbox->setChecked(false);
    break;
  case FormatComboId::BinarySSLWrite:
    m_dump_ssl_read_checkbox->setChecked(false);
    m_dump_ssl_write_checkbox->setChecked(true);
    m_dump_bba_checkbox->setChecked(false);
    break;
  default:
    m_dump_ssl_read_checkbox->setChecked(false);
    m_dump_ssl_write_checkbox->setChecked(false);
    m_dump_bba_checkbox->setChecked(false);
    break;
  }
  // Enable raw or decrypted SSL choices for PCAP
  const bool is_pcap = combo_id == FormatComboId::PCAP;
  m_dump_ssl_read_checkbox->setEnabled(is_pcap);
  m_dump_ssl_write_checkbox->setEnabled(is_pcap);
  m_dump_bba_checkbox->setEnabled(is_pcap);
  Config::SetBaseOrCurrent(Config::MAIN_NETWORK_DUMP_AS_PCAP, is_pcap);
}
