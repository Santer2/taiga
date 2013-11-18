/*
** Taiga, a lightweight client for MyAnimeList
** Copyright (C) 2010-2012, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "std.h"

#include "http.h"

#include "anime_db.h"
#include "announce.h"
#include "common.h"
#include "feed.h"
#include "history.h"
#include "logger.h"
#include "myanimelist.h"
#include "resource.h"
#include "settings.h"
#include "stats.h"
#include "string.h"
#include "taiga.h"
#include "theme.h"
#include "version.h"

#include "dlg/dlg_anime_info.h"
#include "dlg/dlg_anime_info_page.h"
#include "dlg/dlg_anime_list.h"
#include "dlg/dlg_history.h"
#include "dlg/dlg_input.h"
#include "dlg/dlg_main.h"
#include "dlg/dlg_search.h"
#include "dlg/dlg_season.h"
#include "dlg/dlg_settings.h"
#include "dlg/dlg_torrent.h"
#include "dlg/dlg_update.h"

#include "win32/win_taskbar.h"
#include "win32/win_taskdialog.h"

HttpClients Clients;

// =============================================================================

HttpClient::HttpClient() {
  SetAutoRedirect(FALSE);
  SetUserAgent(APP_NAME L"/" + ToWstr(VERSION_MAJOR) + L"." + ToWstr(VERSION_MINOR));
}

BOOL HttpClient::OnError(DWORD dwError) {
  wstring error_text = L"HTTP error #" + ToWstr(dwError) + L": " + 
                       FormatError(dwError, L"winhttp.dll");
  TrimRight(error_text, L"\r\n");

  LOG(LevelError, error_text);
  LOG(LevelError, L"Client mode: " + ToWstr(GetClientMode()));

  Stats.connections_failed++;

  switch (GetClientMode()) {
    case HTTP_MAL_Login:
    case HTTP_MAL_RefreshList:
      MainDialog.ChangeStatus(error_text);
      MainDialog.EnableInput(true);
      break;
    case HTTP_MAL_AnimeAskToDiscuss:
    case HTTP_MAL_AnimeDetails:
    case HTTP_MAL_Image:
    case HTTP_MAL_SearchAnime:
    case HTTP_MAL_UserImage:
    case HTTP_Feed_DownloadIcon:
#ifdef _DEBUG
      MainDialog.ChangeStatus(error_text);
#endif
      break;
    case HTTP_Feed_Check:
    case HTTP_Feed_CheckAuto:
    case HTTP_Feed_Download:
    case HTTP_Feed_DownloadAll:
      MainDialog.ChangeStatus(error_text);
      TorrentDialog.EnableInput();
      break;
    case HTTP_UpdateCheck:
    case HTTP_UpdateDownload:
      MessageBox(UpdateDialog.GetWindowHandle(), 
                 error_text.c_str(), L"Update", MB_ICONERROR | MB_OK);
      UpdateDialog.PostMessage(WM_CLOSE);
      break;
    default:
      History.queue.updating = false;
      MainDialog.ChangeStatus(error_text);
      MainDialog.EnableInput(true);
      break;
  }
  
  TaskbarList.SetProgressState(TBPF_NOPROGRESS);
  return 0;
}

BOOL HttpClient::OnSendRequestComplete() {
#ifdef _DEBUG
  MainDialog.ChangeStatus(L"Connecting...");
#endif

  return 0;
}

BOOL HttpClient::OnHeadersAvailable(win32::http_header_t& headers) {
  switch (GetClientMode()) {
    case HTTP_Silent:
      break;
    case HTTP_UpdateCheck:
    case HTTP_UpdateDownload:
      if (m_dwTotal > 0) {
        UpdateDialog.progressbar.SetMarquee(false);
        UpdateDialog.progressbar.SetRange(0, m_dwTotal);
      } else {
        UpdateDialog.progressbar.SetMarquee(true);
      }
      if (GetClientMode() == HTTP_UpdateDownload) {
        UpdateDialog.SetDlgItemText(IDC_STATIC_UPDATE_PROGRESS, L"Downloading updates...");
      }
      break;
    default:
      TaskbarList.SetProgressState(m_dwTotal > 0 ? TBPF_NORMAL : TBPF_INDETERMINATE);
      break;
  }

  return 0;
}

BOOL HttpClient::OnRedirect(wstring address) {
  switch (GetClientMode()) {
    case HTTP_UpdateDownload: {
      wstring file = address.substr(address.find_last_of(L"/") + 1);
      m_File = GetPathOnly(m_File) + file;
      Taiga.Updater.SetDownloadPath(m_File);
      break;
    }
  }

#ifdef _DEBUG
      MainDialog.ChangeStatus(L"Redirecting... (" + address + L")");
#endif

  return 0;
}

BOOL HttpClient::OnReadData() {
  wstring status;

  switch (GetClientMode()) {
    case HTTP_MAL_RefreshList:
      status = L"Downloading anime list...";
      break;
    case HTTP_MAL_Login:
      status = L"Reading account information...";
      break;
    case HTTP_MAL_AnimeAdd:
    case HTTP_MAL_AnimeUpdate:
      status = L"Updating list...";
      break;
    case HTTP_MAL_AnimeAskToDiscuss:
    case HTTP_Feed_DownloadIcon:
    case HTTP_MAL_AnimeDetails:
    case HTTP_MAL_SearchAnime:
    case HTTP_MAL_Image:
    case HTTP_MAL_UserImage:
    case HTTP_Silent:
      return 0;
    case HTTP_Feed_Check:
    case HTTP_Feed_CheckAuto:
      status = L"Checking new torrents...";
      break;
    case HTTP_Feed_Download:
    case HTTP_Feed_DownloadAll:
      status = L"Downloading torrent file...";
      break;
    case HTTP_Twitter_Request:
      status = L"Connecting to Twitter...";
      break;
    case HTTP_Twitter_Auth:
      status = L"Authorizing Twitter...";
      break;
    case HTTP_Twitter_Post:
      status = L"Updating Twitter status...";
      break;
    case HTTP_UpdateCheck:
    case HTTP_UpdateDownload:
      if (m_dwTotal > 0) {
        UpdateDialog.progressbar.SetPosition(m_dwDownloaded);
      }
      return 0;
    default:
      status = L"Downloading data...";
      break;
  }

  if (m_dwTotal > 0) {
    TaskbarList.SetProgressValue(m_dwDownloaded, m_dwTotal);
    status += L" (" + ToWstr(static_cast<int>(((float)m_dwDownloaded / (float)m_dwTotal) * 100)) + L"%)";
  } else {
    status += L" (" + ToSizeString(m_dwDownloaded) + L")";
  }

  MainDialog.ChangeStatus(status);

  return 0;
}

BOOL HttpClient::OnReadComplete() {
  TaskbarList.SetProgressState(TBPF_NOPROGRESS);
  wstring status;

  Stats.connections_succeeded++;
  
  switch (GetClientMode()) {
    // List
    case HTTP_MAL_RefreshList: {
      wstring data = GetData();
      if (InStr(data, L"<myanimelist>", 0, true) > -1 &&
          InStr(data, L"<myinfo>", 0, true) > -1) {
        wstring path = Taiga.GetDataPath() + L"user\\" + Settings.Account.MAL.user + L"\\anime.xml";
        // Take a backup of the previous list just in case
        wstring new_path = path + L".bak";
        MoveFileEx(path.c_str(), new_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        // Save the current list
        HANDLE hFile = ::CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
          DWORD dwBytesWritten = 0;
          ::WriteFile(hFile, (LPCVOID)m_Buffer, m_dwDownloaded, &dwBytesWritten, NULL);
          ::CloseHandle(hFile);
        }
        // Load the list and refresh
        AnimeDatabase.LoadList(true); // Here we assume that MAL provides up-to-date series information in malappinfo.php
        AnimeListDialog.RefreshList();
        AnimeListDialog.RefreshTabs();
        HistoryDialog.RefreshList();
        SearchDialog.RefreshList();
        status = L"Successfully downloaded the list.";
      } else {
        status = L"MyAnimeList returned an invalid response.";
      }
      MainDialog.ChangeStatus(status);
      MainDialog.EnableInput(true);
      break;
    }

    // =========================================================================
    
    // Login
    case HTTP_MAL_Login: {
      wstring username = InStr(GetData(), L"<username>", L"</username>");
      Taiga.logged_in = IsEqual(Settings.Account.MAL.user, username);
      if (Taiga.logged_in) {
        Settings.Account.MAL.user = username;
        status = L"Logged in as " + Settings.Account.MAL.user + L".";
      } else {
        status = L"Failed to log in.";
#ifdef _DEBUG
        status += L" (" + GetData() + L")";
#else
        status += L" (Invalid username or password)";
#endif
      }
      MainDialog.ChangeStatus(status);
      MainDialog.EnableInput(true);
      MainDialog.UpdateTip();
      MainDialog.UpdateTitle();
      UpdateAllMenus();
      if (Taiga.logged_in) {
        ExecuteAction(L"Synchronize");
        return TRUE;
      }
      break;
    }

    // =========================================================================

    // Update list
    case HTTP_MAL_AnimeAdd:
    case HTTP_MAL_AnimeDelete:
    case HTTP_MAL_AnimeUpdate: {
      History.queue.updating = false;
      MainDialog.ChangeStatus();
      int anime_id = static_cast<int>(GetParam());
      auto anime_item = AnimeDatabase.FindItem(anime_id);
      auto event_item = History.queue.GetCurrentItem();
      if (anime_item && event_item) {
        anime_item->Edit(*event_item, GetData(), GetResponseStatusCode());
        return TRUE;
      }
      break;
    }

    // =========================================================================

    // Ask to discuss
    case HTTP_MAL_AnimeAskToDiscuss: {
      wstring data = GetData();
      if (InStr(data, L"trueEp") > -1) {
        wstring url = InStr(data, L"self.parent.document.location='", L"';");
        int anime_id = static_cast<int>(GetParam());
        if (!url.empty()) {
          int episode_number = 0; // TODO
          wstring title = AnimeDatabase.FindItem(anime_id)->GetTitle();
          if (episode_number) title += L" #" + ToWstr(episode_number);
          win32::TaskDialog dlg(title.c_str(), TD_ICON_INFORMATION);
          dlg.SetMainInstruction(L"Someone has already made a discussion topic for this episode!");
          dlg.UseCommandLinks(true);
          dlg.AddButton(L"Discuss it", IDYES);
          dlg.AddButton(L"Don't discuss", IDNO);
          dlg.Show(g_hMain);
          if (dlg.GetSelectedButtonID() == IDYES) {
            ExecuteLink(url);
          }
        }
      }
      break;
    }

    // =========================================================================

    // Anime details
    case HTTP_MAL_AnimeDetails: {
      int anime_id = static_cast<int>(GetParam());
      if (mal::ParseAnimeDetails(GetData())) {
        if (AnimeDialog.GetCurrentId() == anime_id)
          AnimeDialog.Refresh(false, true, false);
        if (NowPlayingDialog.GetCurrentId() == anime_id)
          NowPlayingDialog.Refresh(false, true, false);
        if (SeasonDialog.IsWindow())
          SeasonDialog.RefreshList(true);
      }
      break;
    }

    // =========================================================================

    // Download image
    case HTTP_MAL_Image: {
      int anime_id = static_cast<int>(GetParam());
      if (ImageDatabase.Load(anime_id, true, false)) {
        if (AnimeDialog.GetCurrentId() == anime_id)
          AnimeDialog.Refresh(true, false, false);
        if (AnimeListDialog.IsWindow())
          AnimeListDialog.RefreshListItem(anime_id);
        if (NowPlayingDialog.GetCurrentId() == anime_id)
          NowPlayingDialog.Refresh(true, false, false);
        if (SeasonDialog.IsWindow())
          SeasonDialog.RefreshList(true);
      }
      break;
    }

    case HTTP_MAL_UserImage: {
      // TODO
      break;
    }

    // =========================================================================

    // Search anime
    case HTTP_MAL_SearchAnime: {
      int anime_id = static_cast<int>(GetParam());
      if (anime_id) {
        if (mal::ParseSearchResult(GetData(), anime_id)) {
          if (AnimeDialog.GetCurrentId() == anime_id)
            AnimeDialog.Refresh(false, true, false, false);
          if (NowPlayingDialog.GetCurrentId() == anime_id)
            NowPlayingDialog.Refresh(false, true, false, false);
          if (SeasonDialog.IsWindow())
            SeasonDialog.RefreshList(true);
          auto anime_item = AnimeDatabase.FindItem(anime_id);
          if (anime_item)
            if (anime_item->GetGenres().empty() || anime_item->GetScore().empty())
              if (mal::GetAnimeDetails(anime_id))
                return TRUE;
        } else {
          status = L"Could not find anime information.";
          AnimeDialog.page_series_info.SetDlgItemText(IDC_EDIT_ANIME_SYNOPSIS, status.c_str());
        }
#ifdef _DEBUG
        MainDialog.ChangeStatus(status);
#endif
      } else {
        MainDialog.ChangeStatus();
        if (SearchDialog.IsWindow()) {
          SearchDialog.ParseResults(GetData());
        }
      }
      break;
    }

    // =========================================================================

    // Torrent
    case HTTP_Feed_Check:
    case HTTP_Feed_CheckAuto: {
      Feed* feed = reinterpret_cast<Feed*>(GetParam());
      if (feed) {
        feed->Load();
        if (feed->ExamineData()) {
          status = L"There are new torrents available!";
        } else {
          status = L"No new torrents found.";
        }
        MainDialog.ChangeStatus(status);
        TorrentDialog.RefreshList();
        TorrentDialog.EnableInput();
        if (GetClientMode() == HTTP_Feed_CheckAuto) {
          switch (Settings.RSS.Torrent.new_action) {
            // Notify
            case 1:
              Aggregator.Notify(*feed);
              break;
            // Download
            case 2:
              if (feed->Download(-1)) return TRUE;
              break;
          }
        }
      }
      break;
    }
    case HTTP_Feed_Download:
    case HTTP_Feed_DownloadAll: {
      Feed* feed = reinterpret_cast<Feed*>(GetParam());
      if (feed) {
        FeedItem* feed_item = reinterpret_cast<FeedItem*>(&feed->items[feed->download_index]);
        wstring app_path, cmd, file = feed_item->title;
        ValidateFileName(file);
        file = feed->GetDataPath() + file + L".torrent";
        Aggregator.file_archive.push_back(feed_item->title);
        if (FileExists(file)) {
          switch (Settings.RSS.Torrent.app_mode) {
            // Default application
            case 1:
              app_path = GetDefaultAppPath(L".torrent", L"");
              break;
            // Custom application
            case 2:
              app_path = Settings.RSS.Torrent.app_path;
              break;
          }
          if (Settings.RSS.Torrent.set_folder && InStr(app_path, L"utorrent", 0, true) > -1) {
            wstring download_path;
            if (Settings.RSS.Torrent.use_folder &&
                FolderExists(Settings.RSS.Torrent.download_path)) {
              download_path = Settings.RSS.Torrent.download_path;
            }
            auto anime_item = AnimeDatabase.FindItem(feed_item->episode_data.anime_id);
            if (anime_item) {
              wstring anime_folder = anime_item->GetFolder();
              if (!anime_folder.empty() && FolderExists(anime_folder)) {
                download_path = anime_folder;
              } else if (Settings.RSS.Torrent.create_folder && !download_path.empty()) {
                anime_folder = anime_item->GetTitle();
                ValidateFileName(anime_folder);
                TrimRight(anime_folder, L".");
                AddTrailingSlash(download_path);
                download_path += anime_folder;
                CreateDirectory(download_path.c_str(), nullptr);
                anime_item->SetFolder(download_path);
                Settings.Save();
              }
            }
            if (!download_path.empty())
              cmd = L"/directory \"" + download_path + L"\" ";
          }
          cmd += L"\"" + file + L"\"";
          Execute(app_path, cmd);
          feed_item->state = FEEDITEM_DISCARDED;
          TorrentDialog.RefreshList();
        }
        feed->download_index = -1;
        if (GetClientMode() == HTTP_Feed_DownloadAll) {
          if (feed->Download(-1)) return TRUE;
        }
      }
      MainDialog.ChangeStatus(L"Successfully downloaded all torrents.");
      TorrentDialog.EnableInput();
      break;
    }
    case HTTP_Feed_DownloadIcon: {
      break;
    }

    // =========================================================================

    // Twitter
    case HTTP_Twitter_Request: {
      OAuthParameters response = Twitter.oauth.ParseQueryString(GetData());
      if (!response[L"oauth_token"].empty()) {
        ExecuteLink(L"http://api.twitter.com/oauth/authorize?oauth_token=" + response[L"oauth_token"]);
        InputDialog dlg;
        dlg.title = L"Twitter Authorization";
        dlg.info = L"Please enter the PIN shown on the page after logging into Twitter:";
        dlg.Show();
        if (dlg.result == IDOK && !dlg.text.empty()) {
          Twitter.AccessToken(response[L"oauth_token"], response[L"oauth_token_secret"], dlg.text);
          return TRUE;
        }
      }
      MainDialog.ChangeStatus();
      break;
    }
	case HTTP_Twitter_Auth: {
      OAuthParameters access = Twitter.oauth.ParseQueryString(GetData());
      if (!access[L"oauth_token"].empty() && !access[L"oauth_token_secret"].empty()) {
        Settings.Announce.Twitter.oauth_key = access[L"oauth_token"];
        Settings.Announce.Twitter.oauth_secret = access[L"oauth_token_secret"];
        Settings.Announce.Twitter.user = access[L"screen_name"];
        status = L"Taiga is now authorized to post to this Twitter account: ";
        status += Settings.Announce.Twitter.user;
      } else {
        status = L"Twitter authorization failed.";
      }
      MainDialog.ChangeStatus(status);
      SettingsDialog.RefreshTwitterLink();
      break;
    }
    case HTTP_Twitter_Post: {
      if (InStr(GetData(), L"\"errors\"", 0) == -1) {
        status = L"Twitter status updated.";
      } else {
        status = L"Twitter status update failed.";
        int index_begin = InStr(GetData(), L"\"message\":\"", 0);
        int index_end = InStr(GetData(), L"\",\"", index_begin);
        if (index_begin > -1 && index_end > -1) {
          index_begin += 11;
          status += L" (" + GetData().substr(index_begin, index_end - index_begin) + L")";
        }
      }
      MainDialog.ChangeStatus(status);
      break;
    }

    // =========================================================================

    // Check updates
    case HTTP_UpdateCheck: {
      if (Taiga.Updater.ParseData(GetData()))
        if (Taiga.Updater.IsDownloadAllowed())
          if (Taiga.Updater.Download())
            return TRUE;
      UpdateDialog.PostMessage(WM_CLOSE);
      break;
    }

    // Download updates
    case HTTP_UpdateDownload: {
      Taiga.Updater.RunInstaller();
      UpdateDialog.PostMessage(WM_CLOSE);
      break;
    }

    // =========================================================================
    
    // Debug
    default: {
#ifdef _DEBUG
      ::MessageBox(0, GetData().c_str(), 0, 0);
#endif
    }
  }

  return FALSE;
}

// =============================================================================

void SetProxies(const wstring& proxy, const wstring& user, const wstring& pass) {
  Clients.anime.UpdateProxy(proxy, user, pass);
  #define SET_PROXY(client) client.SetProxy(proxy, user, pass);
  SET_PROXY(Clients.application);
  SET_PROXY(Clients.service.image);
  SET_PROXY(Clients.service.list);
  SET_PROXY(Clients.service.search);
  SET_PROXY(Clients.sharing.http);
  SET_PROXY(Clients.sharing.twitter);
  for (unsigned int i = 0; i < Aggregator.feeds.size(); i++) {
    SET_PROXY(Aggregator.feeds[i].client);
  }
  SET_PROXY(Taiga.Updater.client);
  #undef SET_PROXY
}

// =============================================================================

AnimeClients::AnimeClients() {
  clients_[HTTP_Client_Image];
  clients_[HTTP_Client_Search];
}

AnimeClients::~AnimeClients() {
  Cleanup(true);
}

void AnimeClients::Cleanup(bool force) {
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    std::set<int> ids;
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
      if (force || it2->second->GetClientMode() == 0) {
        ids.insert(it2->first);
      }
    }
    for (auto id = ids.begin(); id != ids.end(); ++id) {
      delete it->second[*id];
      it->second.erase(*id);
    }
  }
}

class HttpClient* AnimeClients::GetClient(int type, int anime_id) {
  if (anime_id <= anime::ID_UNKNOWN)
    return nullptr;

  if (clients_.find(type) == clients_.end())
    return nullptr;

  auto clients = clients_[type];

  if (clients.find(anime_id) == clients.end()) {
    clients[anime_id] = new class HttpClient;
    clients[anime_id]->SetProxy(Settings.Program.Proxy.host,
                                Settings.Program.Proxy.user,
                                Settings.Program.Proxy.password);
#ifdef _DEBUG
    clients[anime_id]->SetUserAgent(L"Taiga/1.0 (Debug; Type " + ToWstr(type) + 
                                    L"; ID " + ToWstr(anime_id) + L")");
#endif
  }

  return clients[anime_id];
}

void AnimeClients::UpdateProxy(const wstring& proxy, const wstring& user, const wstring& pass) {
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
      it2->second->SetProxy(proxy, user, pass);
    }
  }
}