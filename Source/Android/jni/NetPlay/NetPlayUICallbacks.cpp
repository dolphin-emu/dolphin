// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/NetPlay/NetPlayUICallbacks.h"

namespace NetPlay {

NetPlayUICallbacks::NetPlayUICallbacks() = default;
NetPlayUICallbacks::~NetPlayUICallbacks() = default;

void NetPlayUICallbacks::BootGame(const std::string&, std::unique_ptr<BootSessionData>) {}
void NetPlayUICallbacks::StopGame() {}
bool NetPlayUICallbacks::IsHosting() const { return false; }
void NetPlayUICallbacks::Update() {}
void NetPlayUICallbacks::AppendChat(const std::string&) {}
void NetPlayUICallbacks::OnMsgChangeGame(const NetPlay::SyncIdentifier&, const std::string&) {}
void NetPlayUICallbacks::OnMsgChangeGBARom(int, const NetPlay::GBAConfig&) {}
void NetPlayUICallbacks::OnMsgStartGame() {}
void NetPlayUICallbacks::OnMsgStopGame() {}
void NetPlayUICallbacks::OnMsgPowerButton() {}
void NetPlayUICallbacks::OnPlayerConnect(const std::string&) {}
void NetPlayUICallbacks::OnPlayerDisconnect(const std::string&) {}
void NetPlayUICallbacks::OnPadBufferChanged(u32) {}
void NetPlayUICallbacks::OnHostInputAuthorityChanged(bool) {}
void NetPlayUICallbacks::OnDesync(u32, const std::string&) {}
void NetPlayUICallbacks::OnConnectionLost() {}
void NetPlayUICallbacks::OnConnectionError(const std::string&) {}
void NetPlayUICallbacks::OnTraversalError(Common::TraversalClient::FailureReason) {}
void NetPlayUICallbacks::OnTraversalStateChanged(Common::TraversalClient::State) {}
void NetPlayUICallbacks::OnGameStartAborted() {}
void NetPlayUICallbacks::OnGolferChanged(bool, const std::string&) {}
void NetPlayUICallbacks::OnTtlDetermined(u8) {}
void NetPlayUICallbacks::OnIndexAdded(bool, std::string) {}
void NetPlayUICallbacks::OnIndexRefreshFailed(std::string) {}
bool NetPlayUICallbacks::IsRecording() { return false; }

std::shared_ptr<const UICommon::GameFile>
NetPlayUICallbacks::FindGameFile(const NetPlay::SyncIdentifier&, NetPlay::SyncIdentifierComparison*)
{
    return nullptr;
}

std::string NetPlayUICallbacks::FindGBARomPath(const std::array<u8, 20>&, std::string_view, int) { return {}; }
void NetPlayUICallbacks::ShowGameDigestDialog(const std::string&) {}
void NetPlayUICallbacks::SetGameDigestProgress(int, int) {}
void NetPlayUICallbacks::SetGameDigestResult(int, const std::string&) {}
void NetPlayUICallbacks::AbortGameDigest() {}
void NetPlayUICallbacks::ShowChunkedProgressDialog(const std::string&, u64, std::span<const int>) {}
void NetPlayUICallbacks::HideChunkedProgressDialog() {}
void NetPlayUICallbacks::SetChunkedProgress(int, u64) {}
void NetPlayUICallbacks::SetHostWiiSyncData(std::vector<u64>, std::string) {}

}  // namespace NetPlay
