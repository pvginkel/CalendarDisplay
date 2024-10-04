#include "includes.h"

#include "waveshare_it8951.h"

LOG_TAG(WaveshareIT8951);

#define _WS_CONCAT3(x, y, z) x##y##z
#define WS_CONCAT3(x, y, z) _WS_CONCAT3(x, y, z)

#define SPI_HOST WS_CONCAT3(SPI, CONFIG_DEVICE_SPI_CONTROLLER, _HOST)

// INIT mode, for every init or some time after A2 mode refresh
#define IT8951_MODE_INIT 0
// GC16 mode, for every time to display 16 grayscale image
#define IT8951_MODE_GC16 2

// Built in I80 Command Code
#define IT8951_TCON_SYS_RUN 0x0001
#define IT8951_TCON_STANDBY 0x0002
#define IT8951_TCON_SLEEP 0x0003
#define IT8951_TCON_REG_RD 0x0010
#define IT8951_TCON_REG_WR 0x0011

#define IT8951_TCON_MEM_BST_RD_T 0x0012
#define IT8951_TCON_MEM_BST_RD_S 0x0013
#define IT8951_TCON_MEM_BST_WR 0x0014
#define IT8951_TCON_MEM_BST_END 0x0015

#define IT8951_TCON_LD_IMG 0x0020
#define IT8951_TCON_LD_IMG_AREA 0x0021
#define IT8951_TCON_LD_IMG_END 0x0022

// I80 User defined command code
#define USDEF_I80_CMD_DPY_AREA 0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_DPY_BUF_AREA 0x0037
#define USDEF_I80_CMD_VCOM 0x0039

#define FRONT_GRAY_VALUE 0xf0
#define BACK_GRAY_VALUE 0x00

/*-----------------------------------------------------------------------
 IT8951 Mode defines
------------------------------------------------------------------------*/

// Rotate mode
#define IT8951_ROTATE_0 0
#define IT8951_ROTATE_90 1
#define IT8951_ROTATE_180 2
#define IT8951_ROTATE_270 3

// Pixel mode (Bit per Pixel)
#define IT8951_2BPP 0
#define IT8951_3BPP 1
#define IT8951_4BPP 2
#define IT8951_8BPP 3

// Endian Type
#define IT8951_LDIMG_L_ENDIAN 0
#define IT8951_LDIMG_B_ENDIAN 1
/*-----------------------------------------------------------------------
IT8951 Registers defines
------------------------------------------------------------------------*/
// Register Base Address
#define DISPLAY_REG_BASE 0x1000  // Register RW access

// Base Address of Basic LUT Registers
#define LUT0EWHR (DISPLAY_REG_BASE + 0x00)   // LUT0 Engine Width Height Reg
#define LUT0XYR (DISPLAY_REG_BASE + 0x40)    // LUT0 XY Reg
#define LUT0BADDR (DISPLAY_REG_BASE + 0x80)  // LUT0 Base Address Reg
#define LUT0MFN (DISPLAY_REG_BASE + 0xC0)    // LUT0 Mode and Frame number Reg
#define LUT01AF (DISPLAY_REG_BASE + 0x114)   // LUT0 and LUT1 Active Flag Reg

// Update Parameter Setting Register
#define UP0SR (DISPLAY_REG_BASE + 0x134)      // Update Parameter0 Setting Reg
#define UP1SR (DISPLAY_REG_BASE + 0x138)      // Update Parameter1 Setting Reg
#define LUT0ABFRV (DISPLAY_REG_BASE + 0x13C)  // LUT0 Alpha blend and Fill rectangle Value
#define UPBBADDR (DISPLAY_REG_BASE + 0x17C)   // Update Buffer Base Address
#define LUT0IMXY (DISPLAY_REG_BASE + 0x180)   // LUT0 Image buffer X/Y offset Reg
#define LUTAFSR (DISPLAY_REG_BASE + 0x224)    // LUT Status Reg (status of All LUT Engines)
#define BGVR (DISPLAY_REG_BASE + 0x250)       // Bitmap (1bpp) image color table

// System Registers
#define SYS_REG_BASE 0x0000

// Address of System Registers
#define I80CPCR (SYS_REG_BASE + 0x04)

// Memory Converter Registers
#define MCSR_BASE_ADDR 0x0200
#define MCSR (MCSR_BASE_ADDR + 0x0000)
#define LISAR (MCSR_BASE_ADDR + 0x0008)

bool WaveshareIT8951::setup(float vcom) {
    ESP_LOGI(TAG, "Initializing SPI");

    ESP_ERROR_ASSERT(busy_pin_);
    ESP_ERROR_ASSERT(reset_pin_);
    ESP_ERROR_ASSERT(cs_pin_);

    busy_pin_->setup();
    reset_pin_->setup();
    cs_pin_->setup();

    spi_setup();

    ESP_LOGI(TAG, "Initializing controller");

    WaveshareIT8951DeviceInfo device_info;
    controller_setup(device_info, (uint16_t)(fabs(vcom) * 1000));

    width_ = device_info.Panel_W;
    height_ = device_info.Panel_H;
    memory_address_ = device_info.Memory_Addr_L | (device_info.Memory_Addr_H << 16);

    auto lut_version = (char*)device_info.LUT_Version;
    auto four_byte_align = false;

    if (strcmp(lut_version, "M641") == 0) {
        // 6inch e-Paper HAT(800,600), 6inch HD e-Paper HAT(1448,1072), 6inch HD touch e-Paper HAT(1448,1072)
        a2_mode_ = 4;
        four_byte_align = true;
    } else if (strcmp(lut_version, "M841_TFAB512") == 0) {
        // Another firmware version for 6inch HD e-Paper HAT(1448,1072), 6inch HD touch e-Paper HAT(1448,1072)
        a2_mode_ = 6;
        four_byte_align = true;
    } else if (strcmp(lut_version, "M841") == 0) {
        // 9.7inch e-Paper HAT(1200,825)
        a2_mode_ = 6;
    } else if (strcmp(lut_version, "M841_TFA2812") == 0) {
        // 7.8inch e-Paper HAT(1872,1404)
        a2_mode_ = 6;
    } else if (strcmp(lut_version, "M841_TFA5210") == 0) {
        // 10.3inch e-Paper HAT(1872,1404)
        a2_mode_ = 6;
    } else {
        // default set to 6 as A2 Mode
        a2_mode_ = 6;
    }

    if (four_byte_align) {
        ESP_LOGE(TAG, "Four byte alignment is not supported");
        return false;
    }

    return true;
}

void WaveshareIT8951::spi_setup() {
    ESP_ERROR_ASSERT(mosi_pin_);
    ESP_ERROR_ASSERT(miso_pin_);
    ESP_ERROR_ASSERT(sclk_pin_);

    spi_bus_config_t bus_config = {
        .mosi_io_num = mosi_pin_->get_pin(),
        .miso_io_num = miso_pin_->get_pin(),
        .sclk_io_num = sclk_pin_->get_pin(),
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    // TODO Re-enable DMA (set to SPI_DMA_DISABLED)

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &bus_config, SPI_DMA_DISABLED));

    spi_device_interface_config_t device_interface_config = {
        // This seems to be the highest stable speed.
        .clock_speed_hz = SPI_MASTER_FREQ_11M,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST, &device_interface_config, &spi_));

    int freq_khz;
    ESP_ERROR_CHECK(spi_device_get_actual_freq(spi_, &freq_khz));
    ESP_LOGI(TAG, "SPI device frequency %d KHz", freq_khz);
    ESP_ERROR_ASSERT(freq_khz * 1000 <= device_interface_config.clock_speed_hz);

    size_t bus_max_transfer_sz;
    ESP_ERROR_CHECK(spi_bus_get_max_transaction_len(SPI_HOST, &bus_max_transfer_sz));

    ESP_LOGI(TAG, "Allocating %d bytes for xfer buffer", bus_max_transfer_sz);

    buffer_len_ = bus_max_transfer_sz;
    buffer_ = (uint8_t*)heap_caps_malloc(bus_max_transfer_sz, MALLOC_CAP_DMA);
    ESP_ERROR_ASSERT(buffer_);

    gpio_dump_io_configuration(stdout, (1ull << 11) | (1ull << 12) | (1ull << 13) | (1ull << 10));
}

void WaveshareIT8951::transaction_start() { cs_pin_->digital_write(false); }

void WaveshareIT8951::transaction_end() { cs_pin_->digital_write(true); }

uint8_t WaveshareIT8951::read_byte() {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 8,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));

    return t.rx_data[0];
}

uint16_t WaveshareIT8951::read_word() {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));

    return (uint16_t)t.rx_data[0] << 8 | t.rx_data[1];
}

void WaveshareIT8951::read_array(uint8_t* data, size_t len, bool swap) {
    spi_transaction_t t = {
        .length = 8 * len,
        .rx_buffer = data,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));

    if (swap) {
        for (size_t i = 0; i < len; i += 2) {
            auto tmp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = tmp;
        }
    }
}

void WaveshareIT8951::write_byte(uint8_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = {value},
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));
}

void WaveshareIT8951::write_word(uint16_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = {(uint8_t)(value >> 8), (uint8_t)(value)},
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));
}

void WaveshareIT8951::write_array(uint8_t* data, size_t len, bool swap) {
    spi_transaction_t t = {
        .length = 8 * len,
        .tx_buffer = data,
    };

    if (swap) {
        for (size_t i = 0; i < len; i += 2) {
            auto tmp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = tmp;
        }
    }

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));
}

void WaveshareIT8951::delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

uint32_t WaveshareIT8951::millis() { return esp_timer_get_time() / 1000; }

void WaveshareIT8951::wait_until_idle() {
    if (!this->busy_pin_->digital_read()) {
        return;
    }

    const uint32_t start = millis();
    while (this->busy_pin_->digital_read()) {
        ESP_ERROR_ASSERT(millis() - start < this->idle_timeout());

        delay(20);
    }
}

uint16_t WaveshareIT8951::read_data() {
    transaction_start();

    wait_until_idle();
    write_word(0x1000);
    wait_until_idle();
    read_word();  // Skip a word.
    wait_until_idle();
    auto result = read_word();

    transaction_end();

    return result;
}

void WaveshareIT8951::read_data(uint8_t* data, size_t len) {
    transaction_start();

    wait_until_idle();
    write_word(0x1000);
    wait_until_idle();
    read_word();  // Skip a word.
    wait_until_idle();
    read_array(data, len, true);

    transaction_end();
}

void WaveshareIT8951::write_command(uint16_t command) {
    transaction_start();

    wait_until_idle();
    write_word(0x6000);
    wait_until_idle();
    write_word(command);

    transaction_end();
}

void WaveshareIT8951::write_data(uint16_t data) {
    transaction_start();

    wait_until_idle();
    write_word(0x0000);
    wait_until_idle();
    write_word(data);

    transaction_end();
}

void WaveshareIT8951::write_data(uint8_t* data, size_t len) {
    transaction_start();

    wait_until_idle();
    write_word(0x0000);
    wait_until_idle();
    write_array(data, len, true);

    transaction_end();
}

uint16_t WaveshareIT8951::read_reg(uint16_t reg) {
    transaction_start();

    write_command(IT8951_TCON_REG_RD);
    write_data(reg);
    auto result = read_data();

    transaction_end();

    return result;
}

void WaveshareIT8951::write_reg(uint16_t reg, uint16_t value) {
    transaction_start();

    write_command(IT8951_TCON_REG_WR);
    write_data(reg);
    write_data(value);

    transaction_end();
}

void WaveshareIT8951::enable_enhance_driving_capability() {
    auto value = read_reg(0x0038);

    ESP_LOGD(TAG, "The reg value before writing is %x", value);

    write_reg(0x0038, 0x0602);

    value = read_reg(0x0038);

    ESP_LOGD(TAG, "The reg value after writing is %x", value);
}

void WaveshareIT8951::system_run() { write_command(IT8951_TCON_SYS_RUN); }

void WaveshareIT8951::standby() { write_command(IT8951_TCON_STANDBY); }

void WaveshareIT8951::sleep() { write_command(IT8951_TCON_SLEEP); }

void WaveshareIT8951::reset() {
    reset_pin_->digital_write(true);
    delay(200);
    reset_pin_->digital_write(false);
    delay(10);
    reset_pin_->digital_write(true);
    delay(200);
}

void WaveshareIT8951::get_system_info(WaveshareIT8951DeviceInfo& Dev_Info) {
    write_command(USDEF_I80_CMD_GET_DEV_INFO);

    read_data((uint8_t*)&Dev_Info, sizeof(WaveshareIT8951DeviceInfo));

    ESP_LOGI(TAG, "Panel(W,H) = (%d,%d)", Dev_Info.Panel_W, Dev_Info.Panel_H);
    ESP_LOGI(TAG, "Memory Address = %X", Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16));
    ESP_LOGI(TAG, "FW Version = %s", (uint8_t*)Dev_Info.FW_Version);
    ESP_LOGI(TAG, "LUT Version = %s", (uint8_t*)Dev_Info.LUT_Version);
}

uint16_t WaveshareIT8951::get_vcom() {
    write_command(USDEF_I80_CMD_VCOM);
    write_data(0x0000);
    return read_data();
}

void WaveshareIT8951::set_vcom(uint16_t VCOM) {
    write_command(USDEF_I80_CMD_VCOM);
    write_data(0x0001);
    write_data(VCOM);
}

void WaveshareIT8951::controller_setup(WaveshareIT8951DeviceInfo& Dev_Info, uint16_t VCOM) {
    transaction_end();

    reset();

    system_run();

    get_system_info(Dev_Info);

    // Enable Pack write
    write_reg(I80CPCR, 0x0001);

    // Set VCOM by handle
    if (VCOM != get_vcom()) {
        set_vcom(VCOM);
        ESP_LOGI(TAG, "VCOM = -%.02fV\n", (float)get_vcom() / 1000);
    }
}

void WaveshareIT8951::clear_screen(WaveshareIT8951DeviceInfo& Dev_Info, uint32_t Target_Memory_Addr, uint16_t Mode) {
    WaveshareIT8951Frame frame = {
        .area =
            {
                .x = 0,
                .y = 0,
                .w = Dev_Info.Panel_W,
                .h = Dev_Info.Panel_H,
            },
        .target_memory_address = Target_Memory_Addr,
        .bpp = 4,
        .hold = true,
    };

    update_start(frame);

    memset(buffer_, 0xff, buffer_len_);

    size_t write = frame.area.w / 2 * frame.area.h;
    for (size_t offset = 0; offset < write; offset += buffer_len_) {
        update_write_buffer(min(buffer_len_, offset));
    }

    update_end(frame);
}

void WaveshareIT8951::update_start(WaveshareIT8951Frame& frame) {
    write_frame_start(frame.area, frame.target_memory_address, frame.bpp);
}

void WaveshareIT8951::update_write_buffer(size_t len) { write_frame_buffer(len); }

void WaveshareIT8951::update_end(WaveshareIT8951Frame& frame) {
    write_frame_end();
    display_frame(frame.area, frame.target_memory_address, frame.hold, frame.bpp);
}

void WaveshareIT8951::write_frame_start(WaveshareIT8951Area& area, uint32_t Target_Memory_Addr, int bpp) {
    LoadImageInfo Load_Img_Info;

    wait_display_ready();

    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;

    switch (bpp) {
        case 1:
        case 8:
            Load_Img_Info.Pixel_Format = IT8951_8BPP;
            break;
        case 2:
            Load_Img_Info.Pixel_Format = IT8951_2BPP;
            break;
        case 3:
            Load_Img_Info.Pixel_Format = IT8951_3BPP;
            break;
        case 4:
            Load_Img_Info.Pixel_Format = IT8951_4BPP;
            break;
        default:
            assert(false);
            break;
    }

    Load_Img_Info.Rotate = IT8951_ROTATE_0;

    set_target_memory_address(Target_Memory_Addr);

    auto load_image_area = area;

    if (bpp == 1) {
        load_image_area.x /= 8;
        load_image_area.w /= 8;
    }

    load_image_area_start(&Load_Img_Info, load_image_area);

    // This is the start of the write_data method. The data itself
    // will be written in chunks.

    transaction_start();

    wait_until_idle();
    write_word(0x0000);
    wait_until_idle();
}

void WaveshareIT8951::write_frame_buffer(size_t len) {
    ESP_ERROR_ASSERT(len <= buffer_len_);

    // We don't have to swap the bytes of the words in the array
    // because we've set the endian type.
    write_array(buffer_, len, false);
}

void WaveshareIT8951::write_frame_end() {
    // This is the end of the write_data method.

    transaction_end();

    load_image_end();
}

void WaveshareIT8951::display_frame(WaveshareIT8951Area& area, bool Hold, uint32_t Target_Memory_Addr, int bpp) {
    wait_display_ready();

    if (bpp == 1) {
        // Set Display mode to 1 bpp mode - Set 0x18001138 Bit[18](0x1800113A Bit[2])to 1

        write_reg(UP1SR + 2, read_reg(UP1SR + 2) | (1 << 2));
        write_reg(BGVR, (FRONT_GRAY_VALUE << 8) | BACK_GRAY_VALUE);
    }

    if (Hold == true) {
        display_area(area, bpp == 1 ? a2_mode_ : IT8951_MODE_GC16);
    } else {
        display_area_buffer(area, bpp == 1 ? a2_mode_ : IT8951_MODE_GC16, Target_Memory_Addr);
    }

    if (bpp == 1) {
        wait_display_ready();

        write_reg(UP1SR + 2, read_reg(UP1SR + 2) & ~(1 << 2));
    }
}

void WaveshareIT8951::display_area(WaveshareIT8951Area& area, uint16_t Mode) {
    write_command(USDEF_I80_CMD_DPY_AREA);
    write_data(area.x);
    write_data(area.y);
    write_data(area.w);
    write_data(area.h);
    write_data(Mode);
}

void WaveshareIT8951::display_area_buffer(WaveshareIT8951Area& area, uint16_t Mode, uint32_t Target_Memory_Addr) {
    write_command(USDEF_I80_CMD_DPY_BUF_AREA);
    write_data(area.x);
    write_data(area.y);
    write_data(area.w);
    write_data(area.h);
    write_data(Mode);
    write_data(Target_Memory_Addr);
    write_data(Target_Memory_Addr >> 16);
}

void WaveshareIT8951::set_target_memory_address(uint32_t Target_Memory_Addr) {
    uint16_t WordH = (uint16_t)((Target_Memory_Addr >> 16) & 0x0000FFFF);
    uint16_t WordL = (uint16_t)(Target_Memory_Addr & 0x0000FFFF);

    write_reg(LISAR + 2, WordH);
    write_reg(LISAR, WordL);
}

void WaveshareIT8951::load_image_area_start(LoadImageInfo* Load_Img_Info, WaveshareIT8951Area& area) {
    write_command(IT8951_TCON_LD_IMG_AREA);
    write_data((Load_Img_Info->Endian_Type << 8 | Load_Img_Info->Pixel_Format << 4 | Load_Img_Info->Rotate));
    write_data(area.x);
    write_data(area.y);
    write_data(area.w);
    write_data(area.h);
}

void WaveshareIT8951::load_image_end() { write_command(IT8951_TCON_LD_IMG_END); }

void WaveshareIT8951::wait_display_ready() {
    const uint32_t start = millis();

    while (true) {
        if (!read_reg(LUTAFSR)) {
            return;
        }

        if (millis() - start > this->idle_timeout()) {
            ESP_LOGE(TAG, "Device not ready for more than 30 seconds; exiting");
            esp_restart();
            return;
        }
        delay(20);
    }
}
