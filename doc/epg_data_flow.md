# EPG (SPI) data flow

Message sequence chart of the reception of the "Service and Programme Information" (SPI, formerly EPG, ETSI TS 102 371)
in DABstar.

Dashed arrows are queued signals (thread change from the `DabProcessor` thread to the GUI thread). By default the backend
is not threaded (`__THREADED_BACKEND__` is not defined), so the decoding up to the complete MOT object runs within the
`DabProcessor` thread.

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant FIB as FibDecoder<br/>(DabProcessor thread)
    participant MSC as MscHandler / Backend /<br/>BackendDriver<br/>(DabProcessor thread)
    participant DP as DataProcessor
    participant MH as MotHandler<br/>(isSpi = true)
    participant MD as MotDirectory
    participant MO as MotObject
    participant DR as DabRadio<br/>(GUI thread)
    participant EM as EpgManager
    participant SD as SpiDecoder
    participant EMH as EpgMotHandler
    participant EV as EpgViewer
    participant BTN as btnEpg

    Note over FIB,DR: Phase 1 – start the background service
    FIB--)DR: signal_fib_loaded_state(S4_FullyPacketDataLoaded)
    DR->>EM: set_service_list(mServiceList)
    DR->>DR: _start_epg_services()<br/>(skipped while scanning or if "Receive EPG" is off)
    DR->>FIB: get_spi_packet_services()
    FIB-->>DR: SPacketData[] (packet mode, DSCTy 60, UAType 0x007)
    DR->>MSC: DabProcessor::set_data_channel(pd, Global)
    MSC->>DP: create (via BackendDriver)
    DP->>MH: create MotHandler(isSpi = true)
    DR->>DR: mCurGlobalServiceVec += pd

    Note over FIB,DR: Phase 2 – reception (each CIF, 24 ms)
    loop each CIF
        MSC->>MSC: process_block() → deinterleave, Viterbi, energy dispersal
        MSC->>DP: add_to_frame(bits)
        DP->>DP: _handle_packets() → filter packet address, CRC,<br/>assemble MSC data group
        DP->>MH: add_MSC_data_group(group)
        MH--)DR: slot_spi_data_received()<br/>(invokeMethod queued, at most every 100 ms)
        DR->>BTN: trigger_activity_indicator(300 ms)
        alt data group type 6 (MOT directory)
            MH->>MD: new MotDirectory / directorySegment()
            MD->>MO: create one MotObject per directory entry<br/>(header + ScopeID/ScopeStart/... parameters)
        else data group type 4 (body segment)
            MH->>MO: add_body_segment()
            opt all segments received
                MO--)DR: signal_new_spi_object(SMotSpiObject)
            end
        end
    end

    Note over DR,EV: Phase 3 – decoding and storing (GUI thread)
    DR->>EM: handle_spi_object(obj)
    alt ContentType 2/x (logo picture)
        EM->>EM: _handle_logo() → mLogos[ContentName]
    else ContentType 7/0..2 (SI / PI / GI)
        EM->>SD: decode(body)
        SD-->>EM: SResult (serviceInformation / schedules)
        EM->>EM: _handle_service_information()<br/>_handle_schedule() → map to SId,<br/>replace scope or merge "Advanced" data
        EM->>EM: _prune_old_programmes() (ensemble time)
    end
    DR->>EMH: save_epg_object(obj) → XML file (if enabled)
    EM->>EM: _notify_change() → start 500 ms timer
    EM--)DR: signal_epg_changed()
    DR->>EV: slot_update() (only if visible)
    EM--)DR: signal_epg_available(has_data)
    DR->>BTN: _show_epg_label() → setEnabled + EPG status label

    Note over FIB,EM: In parallel – time reference (each second)
    FIB--)DR: signal_fib_time_info(SUtcTimeSet)
    DR->>EM: set_current_time(UTC, LTO)

    Note over User,EV: Phase 4 – display
    User->>BTN: click
    BTN->>DR: _slot_handle_epg_button()
    DR->>EV: create (once), set_current_service(audio SId), show_and_raise()
    EV->>EM: get_service_ids / get_programmes / get_logo / get_current_time
    User->>EV: double-click service
    EV--)DR: signal_service_selected(SId)
    DR->>DR: slot_handle_fib_content_selector(SId)<br/>(only for audio services of this ensemble)

    Note over DR,MSC: Phase 5 – service change / channel change
    DR->>MSC: _stop_services(false) → stop_all_services(keepGlobal = true)<br/>EPG backend keeps running
    DR->>MSC: _stop_channel() → _stop_services(true) → stops all services
    DR->>EM: clear() → signal_epg_available(false)
```

## Remarks

- **X-PAD path** (not shown): SPI carried in the X-PAD of an audio service takes the path `PadHandler` → `MotObject`
  (not marked as SPI) → `DabRadio::slot_handle_mot_object()`. Objects with ContentType 7/0..2 are wrapped into an
  `SMotSpiObject` and join the chart at `handle_spi_object()`.
- If the secondary packet component of the selected audio service is the same SPI stream as a running global EPG
  service, no second backend is created (`isRunningAsGlobal` in `DabRadio::_start_primary_and_secondary_service()`).
- Body segments (data group type 4) without a known header or directory entry are dropped in `MotHandler`, the
  carousel repeats them.
- Compressed SPI objects ("Advanced" profile, MOT parameter CompressionType) and encrypted objects (CAInfo) are ignored
  by the `EpgManager`.

## Involved files

| Class            | File                                                     |
|------------------|----------------------------------------------------------|
| `FibDecoder`     | `src/base/decoder/fib_decoder.cpp`                       |
| `MscHandler`     | `src/base/backend/msc_handler.cpp`                       |
| `DataProcessor`  | `src/base/backend/data/data_processor.cpp`               |
| `MotHandler`     | `src/base/backend/data/mot/mot_handler.cpp`              |
| `MotDirectory`   | `src/base/backend/data/mot/mot_dir.cpp`                  |
| `MotObject`      | `src/base/backend/data/mot/mot_object.cpp`               |
| `SMotSpiObject`  | `src/base/backend/data/mot/mot_spi_object.h`             |
| `SpiDecoder`     | `src/base/backend/data/spi/spi_decoder.cpp`              |
| `DabRadio`       | `src/base/main/dabradio.cpp`, `dabradio_ctrl.cpp`, `dabradio_ui.cpp` |
| `EpgManager`     | `src/base/main/epg_manager.cpp`                          |
| `EpgMotHandler`  | `src/base/main/epg_mot_handler.cpp`                      |
| `EpgViewer`      | `src/base/support/epg_viewer.cpp`                        |
