/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

//
// SD Card Menu
//

#include "../../inc/MarlinConfigPre.h"
#include "../../HAL/STM32/autoGetZoffset.h"
#include "../../gcode/queue.h"

#if BOTH(HAS_MARLINUI_MENU, SDSUPPORT)

#include "menu_item.h"
#include "../../sd/cardreader.h"

void lcd_sd_updir() {
  ui.encoderPosition = card.cdup() ? ENCODER_STEPS_PER_MENU_ITEM : 0;
  encoderTopLine = 0;
  ui.screen_changed = true;
  ui.refresh();
}

#if ENABLED(SD_REPRINT_LAST_SELECTED_FILE)

  uint16_t sd_encoder_position = 0xFFFF;
  int8_t sd_top_line, sd_items;

  void MarlinUI::reselect_last_file() {
    if (sd_encoder_position == 0xFFFF) return;
    goto_screen(menu_media, sd_encoder_position, sd_top_line, sd_items);
    sd_encoder_position = 0xFFFF;
    defer_status_screen();
    TERN_(HAS_TOUCH_SLEEP, ui.wakeup_screen());
  }

#endif

inline void sdcard_start_selected_file() {
  //gcode.process_subcommands_now(F("G28XY"));
  recovery.info.sdpos = 0;
  autoProbe.swtich_cool_fan = true;
  autoProbe.down_error_count = 0;
  autoProbe.up_error_count = 0;
  feedrate_percentage = 100;
  ui.real_duration_state = false;
  runout.filament_ran_out = false;//Clear status each time you print
  ui.start_print_status = true;
  card.openAndPrintFile(card.filename);
  ui.clear_all = true;
  ui.return_to_status();
  ui.reset_status();
  ui.clear_all = false;
  ui.seclect = 4;
}

class MenuItem_sdfile : public MenuItem_sdbase {
  public:
    static inline void draw(const bool sel, const uint8_t row, FSTR_P const fstr, CardReader &theCard) {
      MenuItem_sdbase::draw(sel, row, fstr, theCard, false);
    }
    static void action(FSTR_P const fstr, CardReader &) {
      #if ENABLED(SD_REPRINT_LAST_SELECTED_FILE)
        // Save menu state for the selected file
        sd_encoder_position = ui.encoderPosition;
        sd_top_line = encoderTopLine;
        sd_items = screen_items;
      #endif
      #if ENABLED(SD_MENU_CONFIRM_START)
          MenuItem_submenu::action(fstr, []{
          char * const longest = card.longest_filename();
          char buffer[strlen(longest) + 2];
          buffer[0] = ' ';
          strcpy(buffer + 1, longest);
          MenuItem_confirm::select_screen(
            GET_TEXT_F(MSG_BUTTON_PRINT), GET_TEXT_F(MSG_BUTTON_CANCEL),
            sdcard_start_selected_file, nullptr,
            GET_TEXT_F(MSG_START_PRINT), buffer, nullptr
          );
        });
      #else
        sdcard_start_selected_file();
        UNUSED(fstr);
      #endif
    }
};

class MenuItem_sdfolder : public MenuItem_sdbase {
  public:
    static inline void draw(const bool sel, const uint8_t row, FSTR_P const fstr, CardReader &theCard) {
      MenuItem_sdbase::draw(sel, row, fstr, theCard, true);
    }
    static void action(FSTR_P const, CardReader &theCard) {
      card.cd(theCard.filename);
      encoderTopLine = 0;
      ui.encoderPosition = 2 * (ENCODER_STEPS_PER_MENU_ITEM);
      ui.screen_changed = true;
      TERN_(HAS_MARLINUI_U8GLIB, ui.drawing_screen = false);
      ui.refresh();
    }
};

void menu_media_filelist() {
  ui.encoder_direction_menus();

  const bool  card_detected = !card.isMounted();
 
  #if HAS_MARLINUI_U8GLIB
    static uint16_t fileCnt;
    if (ui.first_page) fileCnt = card.get_num_Files();
  #else
    const uint16_t fileCnt = card.get_num_Files();
  #endif

 
 static int8_t temp_thisItemNr = 0;
  START_MENU();
   if(card_detected) {
      encoderLine = encoderTopLine = 0;
      if(temp_thisItemNr) {
        _menuLineNr = _thisItemNr = 0;
        if(_thisItemNr == 0) {
          temp_thisItemNr = 0;
          ui.refresh();
        }
      }
    }
    else{
      temp_thisItemNr = encoderLine;    
    }

  #if ENABLED(MULTI_VOLUME)
    ACTION_ITEM(MSG_BACK, []{ ui.goto_screen(menu_media); });
  #else
    BACK_ITEM_F(TERN1(BROWSE_MEDIA_ON_INSERT, screen_history_depth) ? GET_TEXT_F(MSG_INFO_SCREEN) : GET_TEXT_F(MSG_BACK));
  #endif
  if (card.flag.workDirIsRoot) {
    #if !HAS_SD_DETECT
      ACTION_ITEM(MSG_REFRESH, []{ encoderTopLine = 0; card.mount(); });
    #endif
  }
  else if (card.isMounted())
    ACTION_ITEM_F(F(LCD_STR_FOLDER " .."), lcd_sd_updir);

  if (ui.should_draw()) for (uint16_t i = 0; i < fileCnt; i++) {
    if (_menuLineNr == _thisItemNr) {
      card.getfilename_sorted(SD_ORDER(i, fileCnt));
      if (card.flag.filenameIsDir){
        //MENU_ITEM(sdfolder, MSG_MEDIA_MENU, card);
      }
      else
        MENU_ITEM(sdfile, MSG_MEDIA_MENU, card);
    }
    else
      SKIP_ITEM();
  }
  END_MENU();
}

#if ENABLED(MULTI_VOLUME)
  void menu_media_select() {
    START_MENU();
    BACK_ITEM_F(TERN1(BROWSE_MEDIA_ON_INSERT, screen_history_depth) ? GET_TEXT_F(MSG_MAIN) : GET_TEXT_F(MSG_BACK));
    #if ENABLED(VOLUME_SD_ONBOARD)
      ACTION_ITEM(MSG_SD_CARD, []{ card.changeMedia(&card.media_driver_sdcard); card.mount(); ui.goto_screen(menu_media_filelist); });
    #endif
    #if ENABLED(VOLUME_USB_FLASH_DRIVE)
      ACTION_ITEM(MSG_USB_DISK, []{ card.changeMedia(&card.media_driver_usbFlash); card.mount(); ui.goto_screen(menu_media_filelist); });
    #endif
    END_MENU();
  }
#endif

void menu_media() {
  TERN(MULTI_VOLUME, menu_media_select, menu_media_filelist)();
}

#endif // HAS_MARLINUI_MENU && SDSUPPORT
