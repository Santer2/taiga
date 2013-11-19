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

#ifndef DLG_FEED_FILTER_H
#define DLG_FEED_FILTER_H

#include "base/std.h"
#include "track/feed.h"
#include "win32/win_control.h"
#include "win32/win_dialog.h"

// =============================================================================

class FeedFilterDialog : public win32::Dialog {
public:
  FeedFilterDialog();
  virtual ~FeedFilterDialog();

  INT_PTR DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  void OnCancel();
  BOOL OnCommand(WPARAM wParam, LPARAM lParam);
  BOOL OnInitDialog();
  void OnPaint(HDC hdc, LPPAINTSTRUCT lpps);

public:
  void ChoosePage(int index);
  
public:
  FeedFilter filter;

private:
  int current_page_;
  HICON icon_;
  win32::Window main_instructions_label_;

  // Page
  class DialogPage : public win32::Dialog {
  public:
    void Create(UINT uResourceID, FeedFilterDialog* parent, const RECT& rect);
    INT_PTR DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  public:
    FeedFilterDialog* parent;
  };
  
  // Page #0
  class DialogPage0 : public DialogPage {
  public:
    BOOL OnInitDialog();
    LRESULT OnNotify(int idCtrl, LPNMHDR pnmh);
    bool BuildFilter(FeedFilter& filter);
  public:
    win32::ListView preset_list;
  } page_0_;

  // Page #1
  class DialogPage1 : public DialogPage {
  public:
    BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    BOOL OnInitDialog();
    LRESULT OnNotify(int idCtrl, LPNMHDR pnmh);
    bool BuildFilter(FeedFilter& filter);
    void AddConditionToList(const FeedFilterCondition& condition, int index = -1);
    void RefreshConditionList();
  public:
    win32::ComboBox action_combo, match_combo;
    win32::Edit name_text;
    win32::ListView condition_list;
    win32::Toolbar condition_toolbar;
  } page_1_;

  // Page #2
  class DialogPage2 : public DialogPage {
  public:
    BOOL OnInitDialog();
    LRESULT OnNotify(int idCtrl, LPNMHDR pnmh);
    bool BuildFilter(FeedFilter& filter);
  public:
    win32::ListView anime_list;
  } page_2_;
};

extern class FeedFilterDialog FeedFilterDialog;

#endif // DLG_FEED_FILTER_H