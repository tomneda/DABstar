/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */
#ifndef DABRADIO_IF_H
#define DABRADIO_IF_H

#include <QWidget>

class IDabRadio : public QWidget
{
Q_OBJECT
public:
  ~IDabRadio() override = default;

public slots:
  virtual void slot_add_to_ensemble(const QString &, uint32_t) = 0;
  virtual void slot_name_of_ensemble(int, const QString &) = 0;
  virtual void slot_show_frame_errors(int) = 0;
  virtual void slot_show_rs_errors(int) = 0;
  virtual void slot_show_aac_errors(int) = 0;
  virtual void slot_show_fic_success(bool) = 0;
  virtual void slot_show_fic_ber(float) = 0;
  virtual void slot_show_label(const QString &) = 0;
  virtual void slot_handle_mot_object(QByteArray, QString, int, bool) = 0;
  virtual void slot_send_datagram(int) = 0;
  virtual void slot_handle_tdc_data(int, int) = 0;
  virtual void slot_change_in_configuration() = 0;
  virtual void slot_new_audio(int32_t, uint32_t, uint32_t) = 0;
  virtual void slot_set_stereo(bool) = 0;
  virtual void slot_set_stream_selector(int) = 0;
  virtual void slot_no_signal_found() = 0;
  virtual void slot_show_mot_handling(bool) = 0;
  virtual void slot_show_correlation(float, const QVector<int> & v) = 0;
  virtual void slot_show_spectrum(int) = 0;
  virtual void slot_show_cir() = 0;
  virtual void slot_show_iq(int, float) = 0;
  virtual void slot_show_lcd_data(const OfdmDecoder::SLcdData *) = 0;
  virtual void slot_show_digital_peak_level(float iPeakLevel) = 0;
  virtual void slot_show_rs_corrections(int, int) = 0;
  virtual void slot_show_tii(const std::vector<STiiResult> & iTr) = 0;
  virtual void slot_clock_time(int, int, int, int, int, int, int, int, int) = 0;
  virtual void slot_start_announcement(const QString &, int) = 0;
  virtual void slot_stop_announcement(const QString &, int) = 0;
  virtual void slot_new_frame(int) = 0;
  virtual void slot_show_clock_error(float e) = 0;
  virtual void slot_set_epg_data(int, int, const QString &, const QString &) = 0;
  virtual void slot_epg_timer_timeout() = 0;
  virtual void slot_nr_services(int) = 0;
  virtual void slot_handle_content_selector(const QString &) = 0;
  virtual void slot_set_and_show_freq_corr_rf_Hz(int iFreqCorrRF) = 0;
  virtual void slot_show_freq_corr_bb_Hz(int iFreqCorrBB) = 0;
  virtual void slot_test_slider(int) = 0;
  virtual void slot_load_table() = 0;
  //virtual void slot_handle_transmitter_tags(int) = 0;
  virtual void slot_handle_dl_text_button() = 0;
  virtual void slot_handle_logger_button(int) = 0;
  virtual void slot_handle_port_selector() = 0;
  virtual void slot_handle_set_coordinates_button() = 0;
  virtual void slot_handle_eti_active_selector(int) = 0;
  virtual void slot_use_strongest_peak(bool) = 0;
  virtual void slot_handle_dc_avoidance_algorithm(bool) = 0;
  virtual void slot_handle_dc_removal(bool) = 0;
  virtual void slot_show_audio_peak_level(const float iPeakLeft, const float iPeakRight) = 0;
  virtual void slot_handle_tii_collisions(bool) = 0;
  virtual void slot_handle_tii_threshold(int) = 0;
  virtual void slot_handle_tii_subid(int) = 0;
};

#endif
