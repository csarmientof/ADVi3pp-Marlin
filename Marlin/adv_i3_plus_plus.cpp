/**
 * Marlin 3D Printer Firmware For Wanhao Duplicator i3 Plus (ADVi3++)
 *
 * Copyright (C) 2017 Sebastien Andrivet [https://github.com/andrivet/]
 *
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "configuration_store.h"
#include "temperature.h"
#include "cardreader.h"
#include "stepper.h"

#include "adv_i3_plus_plus.h"
#include "adv_i3_plus_plus_utils.h"
#include "adv_i3_plus_plus_impl.h"

#ifdef DEBUG
#pragma message "This is a DEBUG build"
#endif

namespace
{
    const uint16_t advi3_pp_version = 0x0200;                       // 2.0.0
    const uint16_t advi3_pp_oldest_lcd_compatible_version = 0x0200; // 2.0.0
    const uint16_t advi3_pp_newest_lcd_compatible_version = 0x0200; // 2.0.0
    // Modify also DETAILED_BUILD_VERSION in Version.h

    const unsigned long advi3_pp_baudrate = 115200;
    const uint16_t nb_visible_sd_files = 5;
    const uint16_t calibration_cube_size = 20; // 20 mm
    const uint16_t calibration_extruder_filament = 100; // 10 cm
}

namespace advi3pp {

// --------------------------------------------------------------------
// i3PlusPrinter
// --------------------------------------------------------------------

namespace { i3PlusPrinterImpl i3plus; }

//! Initialize the printer and its LCD.
void i3PlusPrinter::setup()
{
    i3plus.setup();
}

//! Read data from the LCD and act accordingly.
void i3PlusPrinter::task()
{
    i3plus.task();
}

//! PID automatic tuning is finished.
void i3PlusPrinter::auto_pid_finished()
{
    i3plus.auto_pid_finished();
}

//! Store presets in permanent memory.
//! @param write Function to use for the actual writing
//! @param eeprom_index
//! @param working_crc
void i3PlusPrinter::store_presets(eeprom_write write, int& eeprom_index, uint16_t& working_crc)
{
    i3plus.store_presets(write, eeprom_index, working_crc);
}

//! Restore presets from permanent memory.
//! @param read Function to use for the actual reading
//! @param eeprom_index
//! @param working_crc
void i3PlusPrinter::restore_presets(eeprom_read read, int& eeprom_index, uint16_t& working_crc)
{
    i3plus.restore_presets(read, eeprom_index, working_crc);
}

//! Reset presets.
void i3PlusPrinter::reset_presets()
{
    i3plus.reset_presets();
}

//! Called when a temperature error occured and display the error on the LCD.
void i3PlusPrinter::temperature_error()
{
    i3plus.temperature_error();
}


// --------------------------------------------------------------------
// i3PlusPrinterImpl
// --------------------------------------------------------------------

//! Initialize the printer and its LCD
void i3PlusPrinterImpl::setup()
{
#ifdef DEBUG
    ADVi3PP_LOG("This is a DEBUG build");
#endif

    Serial2.begin(advi3_pp_baudrate);
    send_versions();
    show_page(Page::Boot);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Presets
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Store presets in permanent memory.
//! @param write Function to use for the actual writing
//! @param eeprom_index
//! @param working_crc
void i3PlusPrinterImpl::store_presets(eeprom_write write, int& eeprom_index, uint16_t& working_crc)
{
    for(auto& preset: presets_)
    {
        write(eeprom_index, reinterpret_cast<uint8_t*>(&preset.hotend), sizeof(preset.hotend), &working_crc);
        write(eeprom_index, reinterpret_cast<uint8_t*>(&preset.bed), sizeof(preset.hotend), &working_crc);
    }
}

//! Restore presets from permanent memory.
//! @param read Function to use for the actual reading
//! @param eeprom_index
//! @param working_crc
void i3PlusPrinterImpl::restore_presets(eeprom_read read, int& eeprom_index, uint16_t& working_crc)
{
    for(auto& preset: presets_)
    {
        read(eeprom_index, reinterpret_cast<uint8_t*>(&preset.hotend), sizeof(preset.hotend), &working_crc);
        read(eeprom_index, reinterpret_cast<uint8_t*>(&preset.bed), sizeof(preset.hotend), &working_crc);
    }
}

//! Reset presets.
void i3PlusPrinterImpl::reset_presets()
{
    presets_[0].hotend = DEFAULT_PREHEAT_PRESET1_HOTEND;
    presets_[1].hotend = DEFAULT_PREHEAT_PRESET2_HOTEND;
    presets_[2].hotend = DEFAULT_PREHEAT_PRESET3_HOTEND;

    presets_[0].bed = DEFAULT_PREHEAT_PRESET1_BED;
    presets_[1].bed = DEFAULT_PREHEAT_PRESET2_BED;
    presets_[2].bed = DEFAULT_PREHEAT_PRESET3_BED;
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Background tasks
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Set the next (minimal) background task time
//! @param delta    Duration to be added to the current time to compute the next (minimal) background task time
void i3PlusPrinterImpl::set_next_background_task_time(unsigned int delta)
{
    next_op_time_ = millis() + delta;
}

//! Set the next (minimal) update time
//! @param delta    Duration to be added to the current time to compute the next (minimal) update time
void i3PlusPrinterImpl::set_next_update_time(unsigned int delta)
{
    next_update_time_ = millis() + delta;
}

void i3PlusPrinterImpl::set_background_task(BackgroundTask task, unsigned int delta)
{
    background_task_ = task;
    set_next_background_task_time(delta);
}

void i3PlusPrinterImpl::clear_background_task()
{
    background_task_ = BackgroundTask::None;
}

//! If there is an operating running, execute its next step
void i3PlusPrinterImpl::execute_background_task()
{
    if(!ELAPSED(millis(), next_op_time_))
        return;

    switch(background_task_)
    {
        case BackgroundTask::None:                  break;
        case BackgroundTask::Leveling:              leveling_task(); break;
        case BackgroundTask::LoadFilament:          load_filament_task(); break;
        case BackgroundTask::UnloadFilament:        unload_filament_task(); break;
        case BackgroundTask::ExtruderCalibration:   extruder_calibration_task(); break;
        default:                                    ADVi3PP_ERROR("Invalid background task " << static_cast<uint16_t>(background_task_)); break;
    }
}

namespace
{
    //! Transform a value from a scale to another one.
    //! @param value        Value to be transformed
    //! @param valueScale   Current scale of the value (maximal)
    //! @param targetScale  Target scale
    //! @return             The scaled value
    int16_t scale(int16_t value, int16_t valueScale, int16_t targetScale) { return value * targetScale / valueScale; }
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Pages management
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Show the given page on the LCD screen
//! @param [in] page The page to be displayed on the LCD screen
void i3PlusPrinterImpl::show_page(Page page)
{
    ADVi3PP_LOG("Show page " << static_cast<uint8_t>(page));
    WriteRegisterDataRequest frame{Register::PictureID};
    frame << 00_u8 << page;
    frame.send(true);
}

//! Retrieve the current page on the LCD screen
Page i3PlusPrinterImpl::get_current_page()
{
    ReadRegisterDataRequest frame{Register::PictureID, 2};
    frame.send();

    ReadRegisterDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Reading PictureID");
        return Page::None;
    }

    Uint16 page; response >> page;
    ADVi3PP_LOG("Current page index = " << page.word);
    return static_cast<Page>(page.word);
}

//! Get the current page and save it as the "Back" page.
void i3PlusPrinterImpl::save_current_page()
{
    back_page_ = get_current_page();
}

//! Set "Next" and "Back" page
//! @param next     Next page
//! @param back     Back page or None to take the current page
void i3PlusPrinterImpl::set_next_back_pages(Page next, Page back)
{
    if(back == Page::None)
        save_current_page();
    else
        back_page_ = back;

    next_page_ = next;
}

//! Show the "Back" page on the LCD display.
void i3PlusPrinterImpl::show_back_page()
{
    if(back_page_ == Page::None)
    {
        ADVi3PP_ERROR("No Back page defined");
        return;
    }

    show_page(back_page_);
    back_page_ = Page::None;
}

//! Show the "Next" page on the LCD display.
void i3PlusPrinterImpl::show_next_page()
{
    if(next_page_ == Page::None)
    {
        ADVi3PP_ERROR("No Next page defined");
        return;
    }

    show_page(next_page_);
    next_page_ = Page::None;
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Incoming LCD commands and status update
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Read data from the LCD and act accordingly
void i3PlusPrinterImpl::task()
{
    read_lcd_serial();
    execute_background_task();
    send_status();
    send_graph_data();
}

//! Update the status of the printer on the LCD.
void i3PlusPrinterImpl::send_status()
{
    auto current_time = millis();
    if(!ELAPSED(current_time, next_update_time_))
        return;
    set_next_update_time();

    WriteRamDataRequest frame{Variable::TargetBed};
    frame << Uint16(thermalManager.target_temperature_bed)
          << Uint16(thermalManager.degBed())
          << Uint16(thermalManager.target_temperature[0])
          << Uint16(thermalManager.degHotend(0))
          << Uint16(scale(fanSpeeds[0], 256, 100))
          << message_;
    frame.send(false);
}

//! Read a frame from the LCD and act accordingly.
void i3PlusPrinterImpl::read_lcd_serial()
{
    // Format of the frame (example):
    // header | length | command | action | nb words | key code
    // -------|--------|---------|--------|----------|---------
    //      2 |      1 |       1 |      2 |        1 |        2   bytes
    //  5A A5 |     06 |      83 |  04 60 |       01 |    01 50

    IncomingFrame frame;
    if(!frame.available())
        return;

    if(!frame.receive())
    {
        ADVi3PP_ERROR("reading incoming Frame");
        return;
    }

    Command command; Action action; Uint8 nb_words; Uint16 value;
    frame >> command >> action >> nb_words >> value;
    auto key_value = static_cast<KeyValue>(value.word);

    // TODO: Check that length == 1, that Hi(action) == 0x04
    ADVi3PP_LOG(">>> " << nb_words.byte << " words, Action = " << static_cast<uint16_t>(action) << ", KeyValue = " << value.word);

    switch(action)
    {
        case Action::Printing:              printing(key_value); break;
        case Action::PrintCommand:          print_command(key_value); break;
        case Action::LoadUnload:            load_unload(key_value); break;
        case Action::Preheat:               preheat(key_value); break;
        case Action::Cooldown:              cooldown(); break;
        case Action::Move:                  move(key_value); break;
        case Action::Home:                  home(key_value); break;
        case Action::DisableMotors:         disable_motors(); break;
        case Action::SdCard:                sd_card(key_value); break;
        case Action::SdCardSelectFile:      sd_card_select_file(key_value); break;
        case Action::ShowSettings:          show_settings(key_value); break;
        case Action::SaveSettings:          save_settings(key_value); break;
        case Action::CancelSettings:        cancel_settings(key_value); break;
        case Action::FactoryReset:          factory_reset(key_value); break;
        case Action::Leveling:              leveling(key_value); break;
        case Action::ExtruderCalibration:   extruder_calibration(key_value); break;
        case Action::XYZMotorsCalibration:  xyz_motors_calibration(key_value); break;
        case Action::PidTuning:             pid_tuning(key_value); break;
        case Action::Statistics:            statistics(key_value); break;
        case Action::About:                 about(key_value); break;
        default:                            ADVi3PP_ERROR("Invalid action " << static_cast<uint16_t>(action)); break;
    }
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Printing & SD Card
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Handle printing action.
//! @param key_value    The sub-action to handle
void i3PlusPrinterImpl::printing(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::PrintShow:           printing_show(); break;
        case KeyValue::PrintBack:           printing_back(); break;
        default:                            ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

//! Show one of the printing screens depending of the context: either the SD screen, the SD printing screen,
//! the printing screen or the temperature screen.
void i3PlusPrinterImpl::printing_show()
{
    if(card.sdprinting)
    {
        save_current_page();
        show_page(Page::SdPrint);
        update_graphs();
        return;
    }

    card.initsd();
    if(!card.cardOK)
    {
        save_current_page();
        show_page(print_job_timer.isRunning() ? Page::Print : Page::Temperature);
        update_graphs();
        return;
    }

    show_sd_files(card.getnrfilenames() - 1);
    save_current_page();
    show_page(Page::SdCard);
}

//! Show the list of files on SD.
//! @param last_index   Index of the last file to display
void i3PlusPrinterImpl::show_sd_files(uint16_t last_index)
{
    WriteRamDataRequest frame{Variable::FileName1};

    last_file_index_ = last_index;
    Chars<> name;
    for(uint8_t index = 0; index < nb_visible_sd_files; ++index)
    {
        get_file_name(index, name);
        frame << name;
    }
    frame.send(true);
}

//! Get a filename with a given index.
//! @tparam S       Size of the buffer
//! @param index    Index of the filename
//! @param name     Copy the filename into this Chars
template<size_t S>
void i3PlusPrinterImpl::get_file_name(uint8_t index, Chars<S>& name)
{
    card.getfilename(last_file_index_ - index);
    name = card.longFilename;
};

//! Select a filename as sent by the LCD screen.
//! @param key_value    The index of the filename to select
void i3PlusPrinterImpl::sd_card_select_file(KeyValue key_value)
{
    if(!card.cardOK)
        return;

    auto file_index = static_cast<uint16_t>(key_value);
    if(file_index > last_file_index_)
        return;
    card.getfilename(last_file_index_ - file_index);
    Chars<> name{card.longFilename};

    WriteRamDataRequest frame{Variable::SelectedFileName};
    frame << name;
    frame.send(true);

    card.openFile(card.filename, true);
    card.startFileprint();
    print_job_timer.start();

    show_page(Page::SdPrint);
    update_graphs();
}

//! LCD SD card menu
void i3PlusPrinterImpl::sd_card(KeyValue key_value)
{
    if(!card.cardOK)
        return;

    uint16_t nb_files = card.getnrfilenames();
    if(nb_files <= nb_visible_sd_files)
        return;

    auto last_file_index = last_file_index_;

    switch(key_value)
    {
        case KeyValue::SdUp:
            if((last_file_index + nb_visible_sd_files) < nb_files)
                last_file_index += nb_visible_sd_files;
            break;

        case KeyValue::SdDown:
            if(last_file_index >= nb_visible_sd_files)
                last_file_index -= nb_visible_sd_files;
            break;

        default:
            break;
    }

    show_sd_files(last_file_index);
};

//! Handle Back button.
void i3PlusPrinterImpl::printing_back()
{
    show_back_page();
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Printing commands
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Handle print commands.
//! @param key_value    The sub-action to handle
void i3PlusPrinterImpl::print_command(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::PrintStop:           print_stop(); break;
        case KeyValue::PrintPause:          print_pause(); break;
        case KeyValue::PrintResume:         print_resume(); break;
        case KeyValue::PrintBack:           print_back(); break;
        default:                            ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

//! Stop printing
void i3PlusPrinterImpl::print_stop()
{
    ADVi3PP_LOG("Stop Print");

    card.stopSDPrint();
    clear_command_queue();
    quickstop_stepper();
    print_job_timer.stop();
    thermalManager.disable_all_heaters();
}

//! Pause printing
void i3PlusPrinterImpl::print_pause()
{
    ADVi3PP_LOG("Pause Print");

    card.pauseSDPrint();
    print_job_timer.pause();
#if ENABLED(PARK_HEAD_ON_PAUSE)
    enqueue_and_echo_commands_P(PSTR("M125"));
#endif
}

//! Resume the current print
void i3PlusPrinterImpl::print_resume()
{
    ADVi3PP_LOG("Resume Print");

#if ENABLED(PARK_HEAD_ON_PAUSE)
    enqueue_and_echo_commands_P(PSTR("M24"));
#else
    card.startFileprint();
    print_job_timer.start();
#endif
}

//! Handle the Back button
void i3PlusPrinterImpl::print_back()
{
    show_back_page();
}


void i3PlusPrinterImpl::set_target_temperature(uint16_t temperature)
{
    WriteRamDataRequest frame{Variable::TargetTemperature};
    frame << Uint16(temperature);
    frame.send();
}


uint16_t i3PlusPrinterImpl::i3PlusPrinterImpl::get_target_temperature()
{
    ReadRamDataRequest frame{Variable::TargetTemperature, 1};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Target Temperature)");
        return 0;
    }

    Uint16 hotend;
    response >> hotend;
    return hotend.word;
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Load and Unload Filament
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Handle Load & Unload actions.
//! @param key_value    The sub-action to handle
void i3PlusPrinterImpl::load_unload(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::LoadUnload:          load_unload_show(); break;
        case KeyValue::Load:                load_unload_start(true); break;
        case KeyValue::Unload:              load_unload_start(false); break;
        case KeyValue::LoadUnloaddStop:     load_unload_stop(); break;
        default:                            ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

//! Show the Load & Unload screen on the LCD display.
void i3PlusPrinterImpl::load_unload_show()
{
    set_target_temperature(200);
    show_page(Page::LoadUnload);
}

//! Start Load or Unload action.
//! @param load    Which action to start (i.e. which screen to display)
void i3PlusPrinterImpl::load_unload_start(bool load)
{
    auto hotend = get_target_temperature();
    if(hotend <= 0)
        return;

    thermalManager.setTargetHotend(hotend, 0);
    enqueue_and_echo_commands_P(PSTR("G91")); // relative mode

    set_background_task(load ? BackgroundTask::LoadFilament : BackgroundTask::UnloadFilament);
    show_page(load ? Page::Load2 : Page::Unload2);
}

//! Handle back from the Load on Unload LCD screen.
void i3PlusPrinterImpl::load_unload_stop()
{
    ADVi3PP_LOG("Load/Unload Stop");
    clear_background_task();
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G90")); // absolute mode
    thermalManager.setTargetHotend(0, 0);
    show_page(Page::LoadUnload);
}

//! Load the filament if the temperature is high enough.
void i3PlusPrinterImpl::load_filament_task()
{
    if(thermalManager.current_temperature[0] >= thermalManager.target_temperature[0] - 10)
    {
        ADVi3PP_LOG("Load Filament");
        enqueue_and_echo_commands_P(PSTR("G1 E1 F120"));
    }
    set_next_background_task_time();
}

//! Unload the filament if the temperature is high enough.
void i3PlusPrinterImpl::unload_filament_task()
{
    if(thermalManager.current_temperature[0] >= thermalManager.target_temperature[0] - 10)
    {
        ADVi3PP_LOG("Unload Filament");
        enqueue_and_echo_commands_P(PSTR("G1 E-1 F120"));
    }
    set_next_background_task_time();
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Preheat & Cooldown
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Handle Preheat actions.
//! @param key_value    Sub-action to handle
void i3PlusPrinterImpl::preheat(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::PreheatShow:     preheat_show(); break;
        default:                        preheat_preset(static_cast<uint16_t>(key_value)); break;
    }
}

//! Show the preheat screen
void i3PlusPrinterImpl::preheat_show()
{
    ADVi3PP_LOG("Preheat page");
    WriteRamDataRequest frame{Variable::Preset1Bed};
    for(auto& preset : presets_)
        frame << Uint16(preset.hotend) << Uint16(preset.bed);
    frame.send();
    show_page(Page::Preheat);
}

//! Preheat the nozzle and save the presets.
//! @param key_value    The index (starting from 1) of the preset to use
void i3PlusPrinterImpl::preheat_preset(uint16_t presetIndex)
{
    ADVi3PP_LOG("Preheat Start");

    presetIndex -= 1;
    if(presetIndex >= NB_PRESETS)
    {
        ADVi3PP_ERROR("Invalid preset # " << presetIndex);
        return;
    }

    ReadRamDataRequest frame{Variable::Preset1Bed, 6};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Presets)");
        return;
    }

    Uint16 hotend, bed;
    for(auto& preset : presets_)
    {
        response >> hotend >> bed;
        preset.hotend = hotend.word;
        preset.bed = bed.word;
    }

    // Save presets
    enqueue_and_echo_commands_P(PSTR("M500"));

    const Preset& preset = presets_[presetIndex];

    Chars<> command;

    command = "M104 S"; command << preset.hotend;
    enqueue_and_echo_command(command.c_str());

    command = "M140 S"; command << preset.bed;
    enqueue_and_echo_command(command.c_str());

    save_current_page();
    show_page(Page::Temperature);
    update_graphs();
}

//! Cooldown the bed and the nozzle
void i3PlusPrinterImpl::cooldown()
{
    ADVi3PP_LOG("Cooldown");
    thermalManager.disable_all_heaters();
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Move & Home
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Execute a move command
void i3PlusPrinterImpl::move(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::MoveXplus:           move_x_plus(); break;
        case KeyValue::MoveXminus:          move_x_minus(); break;
        case KeyValue::MoveYplus:           move_y_plus(); break;
        case KeyValue::MoveYminus:          move_y_minus(); break;
        case KeyValue::MoveZplus:           move_z_plus(); break;
        case KeyValue::MoveZminus:          move_z_minus(); break;
        case KeyValue::MoveEplus:           move_e_plus(); break;
        case KeyValue::MoveEminus:          move_e_minus(); break;
        default:                            ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

//! Execute a home command
//! @param key_value    Which home command to execute
void i3PlusPrinterImpl::home(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::HomeX:               home_x(); break;
        case KeyValue::HomeY:               home_y(); break;
        case KeyValue::HomeZ:               home_z(); break;
        case KeyValue::HomeAll:             home_all(); break;
        default:                            ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_x_plus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 X5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_x_minus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 X-5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_y_plus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 Y5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_y_minus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 Y-5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_z_plus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 Z0.5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Move the nozzle.
void i3PlusPrinterImpl::move_z_minus()
{
    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 Z-0.5 F3000"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Extrude some filament.
void i3PlusPrinterImpl::move_e_plus()
{
    if(thermalManager.degHotend(0) < 180)
        return;

    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 E1 F120"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Unextrude.
void i3PlusPrinterImpl::move_e_minus()
{
    if(thermalManager.degHotend(0) < 180)
        return;

    clear_command_queue();
    enqueue_and_echo_commands_P(PSTR("G91"));
    enqueue_and_echo_commands_P(PSTR("G1 E-1 F120"));
    enqueue_and_echo_commands_P(PSTR("G90"));
}

//! Disable the motors.
void i3PlusPrinterImpl::disable_motors()
{
    enqueue_and_echo_commands_P(PSTR("M84"));
    axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
}

//! Go to home on the X axis.
void i3PlusPrinterImpl::home_x()
{
    enqueue_and_echo_commands_P(PSTR("G28 X0"));
}

//! Go to home on the Y axis.
void i3PlusPrinterImpl::home_y()
{
    enqueue_and_echo_commands_P(PSTR("G28 Y0"));
}

//! Go to home on the Z axis.
void i3PlusPrinterImpl::home_z()
{
    enqueue_and_echo_commands_P(PSTR("G28 Z0"));
}

//! Go to home on all axis.
void i3PlusPrinterImpl::home_all()
{
    enqueue_and_echo_commands_P(PSTR("G28"));
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Settings
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

void i3PlusPrinterImpl::show_settings(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::SettingsPrint:           show_print_settings(Page::Settings); break;
        case KeyValue::SettingsPID:             show_pid_settings(Page::Settings); break;
        case KeyValue::SettingsSteps:           show_steps_settings(Page::Settings); break;
        case KeyValue::SettingsFeedrate:        show_feedrate_settings(Page::Settings); break;
        case KeyValue::SettingsAcceleration:    show_acceleration_settings(Page::Settings); break;
        case KeyValue::SettingsJerk:            show_jerk_settings(Page::Settings); break;
        default:                                ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::save_settings(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::SettingsPrint:           save_print_settings(); break;
        case KeyValue::SettingsPID:             save_pid_settings(); break;
        case KeyValue::SettingsSteps:           save_steps_settings(); break;
        case KeyValue::SettingsFeedrate:        save_feedrate_settings(); break;
        case KeyValue::SettingsAcceleration:    save_acceleration_settings(); break;
        case KeyValue::SettingsJerk:            save_jerk_settings(); break;
        default:                                ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::cancel_settings(KeyValue)
{
    show_back_page();
}

//! Display on the LCD screen the printing settings.
void i3PlusPrinterImpl::show_print_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::PrintSettingsSpeed};
    frame << Uint16(feedrate_percentage)
          << Uint16(thermalManager.degTargetHotend(0))
          << Uint16(thermalManager.degTargetBed())
          << Uint16(scale(fanSpeeds[0], 256, 100));
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::PrintSettings);
}

//! Save the printing settings.
void i3PlusPrinterImpl::save_print_settings()
{
    ReadRamDataRequest frame{Variable::PrintSettingsSpeed, 4};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Print Settings)");
        return;
    }

    Uint16 speed, hotend, bed, fan;
    response >> speed >> hotend >> bed >> fan;

    feedrate_percentage = speed.word;
    thermalManager.setTargetHotend(hotend.word, 0);
    thermalManager.setTargetBed(bed.word);
    fanSpeeds[0] = scale(fan.word, 100, 256);

    show_next_page();
}

void i3PlusPrinterImpl::show_pid_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::PidP};
    frame << Uint16(PID_PARAM(Kp, 0) * 100)
          << Uint16(unscalePID_i(PID_PARAM(Ki, 0)) * 100)
          << Uint16(unscalePID_d(PID_PARAM(Kd, 0)) * 100);
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::PidSettings);
}

void i3PlusPrinterImpl::save_pid_settings()
{
    ReadRamDataRequest frame{Variable::PidD, 3};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (PID Settings)");
        return;
    }

    Uint16 p, i, d;
    response >> p >> i >> d;

    PID_PARAM(Kp, 0) = static_cast<float>(p.word) / 100;
    PID_PARAM(Ki, 0) = scalePID_i(static_cast<float>(i.word) / 100);
    PID_PARAM(Kd, 0) = scalePID_d(static_cast<float>(d.word) / 100);

    enqueue_and_echo_commands_P(PSTR("M500"));

    show_next_page();
}

void i3PlusPrinterImpl::show_steps_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::MotorSettingsX};
    frame << Uint16(planner.axis_steps_per_mm[X_AXIS] * 10)
          << Uint16(planner.axis_steps_per_mm[Y_AXIS] * 10)
          << Uint16(planner.axis_steps_per_mm[Z_AXIS] * 10)
          << Uint16(planner.axis_steps_per_mm[E_AXIS] * 10);
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::StepsSettings);
}

void i3PlusPrinterImpl::save_steps_settings()
{
    ReadRamDataRequest frame{Variable::MotorSettingsX, 4};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Steps Settings)");
        return;
    }

    Uint16 x, y, z, e;
    response >> x >> y >> z >> e;

    planner.axis_steps_per_mm[X_AXIS] = static_cast<float>(x.word) / 10;
    planner.axis_steps_per_mm[Y_AXIS] = static_cast<float>(y.word) / 10;
    planner.axis_steps_per_mm[Z_AXIS] = static_cast<float>(z.word) / 10;
    planner.axis_steps_per_mm[E_AXIS] = static_cast<float>(e.word) / 10;

    enqueue_and_echo_commands_P(PSTR("M500"));

    show_next_page();
}

void i3PlusPrinterImpl::show_feedrate_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::FeedrateMaxX};
    frame << Uint16(planner.max_feedrate_mm_s[X_AXIS])
          << Uint16(planner.max_feedrate_mm_s[Y_AXIS])
          << Uint16(planner.max_feedrate_mm_s[Z_AXIS])
          << Uint16(planner.max_feedrate_mm_s[E_AXIS])
          << Uint16(planner.min_feedrate_mm_s)
          << Uint16(planner.min_travel_feedrate_mm_s);
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::FeedrateSettings);
}

void i3PlusPrinterImpl::save_feedrate_settings()
{
    ReadRamDataRequest frame{Variable::FeedrateMaxX, 6};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Feedrate Settings)");
        return;
    }

    Uint16 x, y, z, e, min, travel;
    response >> x >> y >> z >> e >> min >> travel;

    planner.max_feedrate_mm_s[X_AXIS] = static_cast<float>(x.word);
    planner.max_feedrate_mm_s[Y_AXIS] = static_cast<float>(y.word);
    planner.max_feedrate_mm_s[Z_AXIS] = static_cast<float>(z.word);
    planner.max_feedrate_mm_s[E_AXIS] = static_cast<float>(e.word);
    planner.min_feedrate_mm_s         = static_cast<float>(min.word);
    planner.min_travel_feedrate_mm_s  = static_cast<float>(travel.word);

    enqueue_and_echo_commands_P(PSTR("M500"));

    show_next_page();
}

void i3PlusPrinterImpl::show_acceleration_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::AccelerationMaxX};
    frame << Uint16(static_cast<uint16_t>(planner.max_acceleration_mm_per_s2[X_AXIS]))
          << Uint16(static_cast<uint16_t>(planner.max_acceleration_mm_per_s2[Y_AXIS]))
          << Uint16(static_cast<uint16_t>(planner.max_acceleration_mm_per_s2[Z_AXIS]))
          << Uint16(static_cast<uint16_t>(planner.max_acceleration_mm_per_s2[E_AXIS]))
          << Uint16(static_cast<uint16_t>(planner.acceleration))
          << Uint16(static_cast<uint16_t>(planner.retract_acceleration))
          << Uint16(static_cast<uint16_t>(planner.travel_acceleration));
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::AccelerationSettings);
}

void i3PlusPrinterImpl::save_acceleration_settings()
{
    ReadRamDataRequest frame{Variable::AccelerationMaxX, 7};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Acceleration Settings)");
        return;
    }

    Uint16 x, y, z, e, print, retract, travel;
    response >> x >> y >> z >> e >> print >> retract >> travel;

    planner.max_acceleration_mm_per_s2[X_AXIS] = static_cast<uint32_t>(x.word);
    planner.max_acceleration_mm_per_s2[Y_AXIS] = static_cast<uint32_t>(y.word);
    planner.max_acceleration_mm_per_s2[Z_AXIS] = static_cast<uint32_t>(z.word);
    planner.max_acceleration_mm_per_s2[E_AXIS] = static_cast<uint32_t>(e.word);
    planner.acceleration                       = static_cast<float>(print.word);
    planner.retract_acceleration               = static_cast<float>(retract.word);
    planner.travel_acceleration                = static_cast<float>(travel.word);

    enqueue_and_echo_commands_P(PSTR("M500"));

    show_next_page();
}

void i3PlusPrinterImpl::show_jerk_settings(Page next, Page back)
{
    WriteRamDataRequest frame{Variable::JerkX};
    frame << Uint16(planner.max_jerk[X_AXIS] * 10)
          << Uint16(planner.max_jerk[Y_AXIS] * 10)
          << Uint16(planner.max_jerk[Z_AXIS] * 10)
          << Uint16(planner.max_jerk[E_AXIS] * 10);
    frame.send();

    set_next_back_pages(next, back);
    show_page(Page::JerkSettings);
}

void i3PlusPrinterImpl::save_jerk_settings()
{
    ReadRamDataRequest frame{Variable::JerkX, 4};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Acceleration Settings)");
        return;
    }

    Uint16 x, y, z, e;
    response >> x >> y >> z >> e;

    planner.max_jerk[X_AXIS] = static_cast<uint32_t>(x.word) / 10;
    planner.max_jerk[Y_AXIS] = static_cast<uint32_t>(y.word) / 10;
    planner.max_jerk[Z_AXIS] = static_cast<uint32_t>(z.word) / 10;
    planner.max_jerk[E_AXIS] = static_cast<uint32_t>(e.word) / 10;

    enqueue_and_echo_commands_P(PSTR("M500"));

    show_next_page();
}

//! Reset all settings of the printer to factory ones.
void i3PlusPrinterImpl::factory_reset(KeyValue)
{
    enqueue_and_echo_commands_P(PSTR("M502"));
    enqueue_and_echo_commands_P(PSTR("M500"));
    show_page(Page::Settings);
}


// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Statistics
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Display statistics on the LCD screem.
void i3PlusPrinterImpl::statistics(KeyValue)
{
    send_stats();
    show_page(Page::Statistics);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// About & Versions
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Get the current LCD firmware version.
//! @return     The version as a string.
Chars<16> i3PlusPrinterImpl::get_lcd_firmware_version()
{
    ReadRegisterDataRequest frame{Register::Version, 1};
    frame.send();

    ReadRegisterDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Version)");
        return Chars<16>("Unknown");
    }

    Uint8 version; response >> version;
    Chars<16> lcd_version; lcd_version << (version.byte / 0x10) << "." << (version.byte % 0x10);
    ADVi3PP_LOG("LCD Firmware raw version = " << version.byte);
    return lcd_version;
}

//! Convert a version from its hexadecimal representation.
//! @param hex_version  Hexadecimal representation of the version
//! @return             Version as a string
Chars<16> convert_version(uint16_t hex_version)
{
    Chars<16> version;
    version << hex_version / 0x0100 << "." << (hex_version % 0x100) / 0x10 << "." << hex_version % 0x10;
    return version;
}

//! Send the different versions to the LCD screen.
void i3PlusPrinterImpl::send_versions()
{
    Chars<16> marlin_version{SHORT_BUILD_VERSION};
    Chars<16> motherboard_version = convert_version(advi3_pp_version);
    Chars<16> advi3pp_lcd_version = convert_version(adv_i3_pp_lcd_version_);
    Chars<16> lcd_firmware_version = get_lcd_firmware_version();

    WriteRamDataRequest frame{Variable::MarlinVersion};
    frame << marlin_version << motherboard_version << advi3pp_lcd_version << lcd_firmware_version;
    frame.send();
}

//! Display the About screen,
void i3PlusPrinterImpl::about(KeyValue key_value)
{
    adv_i3_pp_lcd_version_ = static_cast<uint16_t>(key_value);
    send_versions();

    if(adv_i3_pp_lcd_version_ < advi3_pp_oldest_lcd_compatible_version || adv_i3_pp_lcd_version_ > advi3_pp_newest_lcd_compatible_version)
        show_page(Page::Mismatch);
    else
        show_page(Page::About);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// PID Tuning
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Handle PID tuning.
//! @param key_value    The step of the PID tuning
void i3PlusPrinterImpl::pid_tuning(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::PidTuningStep1:   pid_tuning_step1(); break;
        case KeyValue::PidTuningStep2:   pid_tuning_step2(); break;
        default:                         ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::pid_tuning_step1()
{
    set_target_temperature(200);
    show_page(Page::PidTuning1);
}

void i3PlusPrinterImpl::pid_tuning_step2()
{
    auto hotend = get_target_temperature();
    if(hotend <= 0)
        return;

    enqueue_and_echo_command("M106 S255"); // Turn on fam
    Chars<> auto_pid_command; auto_pid_command << "M303 S" << hotend << "E0 C8 U1";
    enqueue_and_echo_command(auto_pid_command.c_str());

    show_page(Page::PidTuning2);
};

//! PID automatic tuning is finished.
void i3PlusPrinterImpl::auto_pid_finished()
{
    ADVi3PP_LOG("Auto PID finished");
    enqueue_and_echo_command("M106 S0");
    show_pid_settings(Page::Calibration, Page::PidTuning1);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Leveling
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

void i3PlusPrinterImpl::leveling(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::LevelingHome:    leveling_home(); break;
        case KeyValue::LevelingPoint1:  leveling_point1(); break;
        case KeyValue::LevelingPoint2:  leveling_point2(); break;
        case KeyValue::LevelingPoint3:  leveling_point3(); break;
        case KeyValue::LevelingPoint4:  leveling_point4(); break;
        case KeyValue::LevelingBack:    leveling_finish(); break;
        default:                        ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::leveling_home()
{
    show_page(Page::Leveling1);
    axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
    enqueue_and_echo_commands_P(PSTR("G90")); // absolute mode
    enqueue_and_echo_commands_P((PSTR("G28"))); // homing
    set_background_task(BackgroundTask::Leveling);
}

void i3PlusPrinterImpl::leveling_task()
{
    if(axis_homed[X_AXIS] && axis_homed[Y_AXIS] && axis_homed[Z_AXIS])
    {
        ADVi3PP_LOG("Leveling Homed, start process");
        clear_background_task();
        show_page(Page::Leveling2);
    }
    else
        set_next_background_task_time(200);
}

//! Handle leveling point #1.
void i3PlusPrinterImpl::leveling_point1()
{
    ADVi3PP_LOG("Level step 1");
    enqueue_and_echo_commands_P((PSTR("G1 Z10 F2000")));
    enqueue_and_echo_commands_P((PSTR("G1 X30 Y30 F6000")));
    enqueue_and_echo_commands_P((PSTR("G1 Z0 F1000")));
}

//! Handle leveling point #2.
void i3PlusPrinterImpl::leveling_point2()
{
    ADVi3PP_LOG("Level step 2");
    enqueue_and_echo_commands_P((PSTR("G1 Z10 F2000")));
    enqueue_and_echo_commands_P((PSTR("G1 X170 Y170 F6000")));
    enqueue_and_echo_commands_P((PSTR("G1 Z0 F1000")));
}

//! Handle leveling point #3.
void i3PlusPrinterImpl::leveling_point3()
{
    ADVi3PP_LOG("Level step 3");
    enqueue_and_echo_commands_P((PSTR("G1 Z10 F2000")));
    enqueue_and_echo_commands_P((PSTR("G1 X170 Y30 F6000")));
    enqueue_and_echo_commands_P((PSTR("G1 Z0 F1000")));
}

//! Handle leveling point #4.
void i3PlusPrinterImpl::leveling_point4()
{
    ADVi3PP_LOG("Level step 3");
    enqueue_and_echo_commands_P((PSTR("G1 Z10 F2000")));
    enqueue_and_echo_commands_P((PSTR("G1 X30 Y170 F6000")));
    enqueue_and_echo_commands_P((PSTR("G1 Z0 F1000")));
}

//! Handle leveling point #4.
void i3PlusPrinterImpl::leveling_finish()
{
    enqueue_and_echo_commands_P((PSTR("G1 Z30 F2000")));
    show_page(Page::Calibration);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Extruder calibration
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template <typename T, typename U, typename V>
void adjust_value(T& value, U expected, V measured)
{
    // new = old * (expected / distance_actually_moved)
    // new = old * expected / (expected - distance_measured)
    value = value * expected / (expected - measured);
};

void i3PlusPrinterImpl::extruder_calibration(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::CalibrationShow:         show_extruder_calibration(); break;
        case KeyValue::CalibrationStart:        start_extruder_calibration(); break;
        case KeyValue::CalibrationSettings:     extruder_calibrartion_settings(); break;
        case KeyValue::CalibrationCancel:       cancel_extruder_calibration(); break;
        default:                                ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::show_extruder_calibration()
{
    set_target_temperature(200);

    WriteRamDataRequest frame{Variable::Measure1};
    frame << 200_u16;
    frame.send();

    show_page(Page::ExtruderCalibration1);
}

void i3PlusPrinterImpl::start_extruder_calibration()
{
    auto hotend = get_target_temperature();
    if(hotend <= 0)
        return;

    thermalManager.setTargetHotend(hotend, 0);
    set_background_task(BackgroundTask::ExtruderCalibration);
    enqueue_and_echo_commands_P(PSTR("M83"));       // relative E mode
    enqueue_and_echo_commands_P(PSTR("G92 E0"));    // reset E axis

    show_page(Page::ExtruderCalibration2);
}

void i3PlusPrinterImpl::extruder_calibration_task()
{
    if(current_position[E_AXIS] >= calibration_extruder_filament)
    {
        extruder_calibration_finished();
        return;
    }

    if(thermalManager.current_temperature[0] < thermalManager.target_temperature[0] - 10)
    {
        set_next_background_task_time();
        return;
    }

    enqueue_and_echo_commands_P(PSTR("G1 E1 F100")); // Extrude 1mm slowly
    set_next_background_task_time();
}

void i3PlusPrinterImpl::extruder_calibration_finished()
{
    ADVi3PP_LOG("Filament extruded " << current_position[E_AXIS]);
    clear_background_task();
    show_page(Page::ExtruderCalibration3);
}

void i3PlusPrinterImpl::extruder_calibrartion_settings()
{
    auto extruded = current_position[E_AXIS];
    enqueue_and_echo_commands_P(PSTR("G82"));       // absolute E mode
    enqueue_and_echo_commands_P(PSTR("G92 E0"));    // reset E axis

    ReadRamDataRequest frame{Variable::Measure1, 1};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Measures)");
        return;
    }

    Uint16 e; response >> e;
    adjust_value(planner.axis_steps_per_mm[E_AXIS], extruded, e.word);

    show_steps_settings(Page::Calibration, Page::ExtruderCalibration1);
}

void i3PlusPrinterImpl::cancel_extruder_calibration()
{
    enqueue_and_echo_commands_P(PSTR("G82"));       // absolute E mode
    enqueue_and_echo_commands_P(PSTR("G92 E0"));    // reset E axis

    show_page(Page::ExtruderCalibration1);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// XYZ Motors calibration
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

void i3PlusPrinterImpl::xyz_motors_calibration(KeyValue key_value)
{
    switch(key_value)
    {
        case KeyValue::CalibrationShow:         show_xyz_motors_calibration(); break;
        case KeyValue::CalibrationSettings:     xyz_motors_calibration_settings(); break;
        default:                                ADVi3PP_ERROR("Invalid key value " << static_cast<uint16_t>(key_value)); break;
    }
}

void i3PlusPrinterImpl::show_xyz_motors_calibration()
{
    WriteRamDataRequest frame{Variable::Measure1};
    frame << 200_u16 << 200_u16 << 200_u16;
    frame.send();
    show_page(Page::XYZMotorsCalibration);
}

void i3PlusPrinterImpl::xyz_motors_calibration_settings()
{
    ReadRamDataRequest frame{Variable::Measure1, 3};
    frame.send();

    ReadRamDataResponse response;
    if(!response.receive(frame))
    {
        ADVi3PP_ERROR("Receiving Frame (Measures)");
        return;
    }

    Uint16 x, y, z;
    response >> x >> y >> z;

    adjust_value(planner.axis_steps_per_mm[X_AXIS], calibration_cube_size, x.word);
    adjust_value(planner.axis_steps_per_mm[Y_AXIS], calibration_cube_size, y.word);
    adjust_value(planner.axis_steps_per_mm[Z_AXIS], calibration_cube_size, z.word);

    show_steps_settings(Page::Calibration, Page::XYZMotorsCalibration);
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Status
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Send statistics to the LCD screen.
void i3PlusPrinterImpl::send_stats()
{
    printStatistics stats = print_job_timer.getStats();

    WriteRamDataRequest frame{Variable::TotalPrints};
    frame << Uint16(stats.totalPrints) << Uint16(stats.finishedPrints);
    frame.send();

#if ENABLED(PRINTCOUNTER)
    duration_t duration = stats.printTime;
    frame.reset(Variable::TotalPrintTime);
    frame << Chars<16>{duration};
    frame.send();

    duration = stats.longestPrint;
    frame.reset(Variable::LongestPrintTime);
    frame << Chars<16>{duration};
    frame.send();

    Chars<> filament_used;
    filament_used << static_cast<unsigned int>(stats.filamentUsed / 1000)
                  << "."
                  << static_cast<unsigned int>(stats.filamentUsed / 100) % 10;
    frame.reset(Variable::TotalFilament);
    frame << Chars<>{filament_used.c_str()};
    frame.send();
#endif
}

void i3PlusPrinterImpl::update_graphs()
{
    update_graphs_ = true;
    next_update_graph_time_ = millis() + 2000;
}

//! Update the graphics (two channels: the bed and the hotend).
void i3PlusPrinterImpl::send_graph_data()
{
    if(!update_graphs_ || !ELAPSED(millis(), next_update_graph_time_))
        return;

    WriteCurveDataRequest frame{0b00001111};
    frame << Uint16{thermalManager.degBed()}
          << Uint16{thermalManager.degBed()}
          << Uint16{thermalManager.degHotend(0)}
          << Uint16{thermalManager.degHotend(0)};
    frame.send(false);

    next_update_graph_time_ = millis() + 500;
}

// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Errors
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//! Display the Thermal Runaway Error screen.
void i3PlusPrinterImpl::temperature_error()
{
    show_page(advi3pp::Page::ThermalRunawayError);
}

}