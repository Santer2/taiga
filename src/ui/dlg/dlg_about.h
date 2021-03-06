/*
** Taiga
** Copyright (C) 2010-2014, Eren Okka
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

#ifndef TAIGA_UI_DLG_ABOUT_H
#define TAIGA_UI_DLG_ABOUT_H

#include "win/ctrl/win_ctrl.h"
#include "win/win_dialog.h"

namespace ui {

class AboutDialog : public win::Dialog {
public:
  AboutDialog();
  ~AboutDialog() {}

  INT_PTR DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  BOOL OnDestroy();
  BOOL OnInitDialog();
  void OnPaint(HDC hdc, LPPAINTSTRUCT lpps);
  void OnTimer(UINT_PTR nIDEvent);

private:
  win::RichEdit rich_edit_;

  size_t note_count_;
  int note_index_;
  static int note_list[][2];
};

extern AboutDialog DlgAbout;

}  // namespace ui

#endif  // TAIGA_UI_DLG_ABOUT_H