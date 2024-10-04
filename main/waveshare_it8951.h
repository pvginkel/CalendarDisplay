#pragma once

#include "gpiopin.h"

struct WaveshareIT8951DeviceInfo {
    uint16_t Panel_W;
    uint16_t Panel_H;
    uint16_t Memory_Addr_L;
    uint16_t Memory_Addr_H;
    uint16_t FW_Version[8];
    uint16_t LUT_Version[8];
};

struct WaveshareIT8951Area {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

struct WaveshareIT8951Frame {
    WaveshareIT8951Area area;
    uint32_t target_memory_address;
    int bpp;
    bool hold;
};

class WaveshareIT8951 {
    struct LoadImageInfo {
        uint16_t Endian_Type;   // little or Big Endian
        uint16_t Pixel_Format;  // bpp
        uint16_t Rotate;        // Rotate mode
    };

public:
    bool setup(float vcom);
    void set_busy_pin(GPIOPin* busy_pin) { this->busy_pin_ = busy_pin; }
    void set_reset_pin(GPIOPin* reset_pin) { this->reset_pin_ = reset_pin; }
    void set_mosi_pin(GPIOPin* mosi_pin) { this->mosi_pin_ = mosi_pin; }
    void set_miso_pin(GPIOPin* miso_pin) { this->miso_pin_ = miso_pin; }
    void set_sclk_pin(GPIOPin* sclk_pin) { this->sclk_pin_ = sclk_pin; }
    void set_cs_pin(GPIOPin* cs_pin) { this->cs_pin_ = cs_pin; }
    uint8_t* get_buffer() { return buffer_; }
    size_t get_buffer_len() { return buffer_len_; }
    uint16_t get_width() { return width_; }
    uint16_t get_height() { return height_; }
    uint32_t get_memory_address() { return memory_address_; }
    void enable_enhance_driving_capability();
    void system_run();
    void standby();
    void sleep();
    void reset();
    void get_system_info(WaveshareIT8951DeviceInfo& Dev_Info);
    void clear_screen(WaveshareIT8951DeviceInfo& Dev_Info, uint32_t Target_Memory_Addr, uint16_t Mode);
    void update_start(WaveshareIT8951Frame& frame);
    void update_write_buffer(size_t len);
    void update_end(WaveshareIT8951Frame& frame);

private:
    void spi_setup();
    void transaction_start();
    void transaction_end();
    uint8_t read_byte();
    uint16_t read_word();
    void read_array(uint8_t* data, size_t len, bool swap);
    void write_byte(uint8_t value);
    void write_word(uint16_t value);
    void write_array(uint8_t* data, size_t len, bool swap);
    void delay(int ms);
    uint32_t millis();
    void wait_until_idle();
    uint16_t read_data();
    void read_data(uint8_t* data, size_t len);
    void write_command(uint16_t command);
    void write_data(uint16_t data);
    void write_data(uint8_t* data, size_t len);
    uint16_t read_reg(uint16_t reg);
    void write_reg(uint16_t reg, uint16_t value);
    uint32_t idle_timeout() { return 30'000; }
    void controller_setup(WaveshareIT8951DeviceInfo& Dev_Info, uint16_t vcom);
    uint16_t get_vcom();
    void set_vcom(uint16_t vcom);
    void write_frame_start(WaveshareIT8951Area& area, uint32_t Target_Memory_Addr, int bpp);
    void write_frame_buffer(size_t len);
    void write_frame_end();
    void display_frame(WaveshareIT8951Area& area, bool Hold, uint32_t Target_Memory_Addr, int bpp);
    void display_area(WaveshareIT8951Area& area, uint16_t Mode);
    void display_area_buffer(WaveshareIT8951Area& area, uint16_t Mode, uint32_t Target_Memory_Addr);
    void set_target_memory_address(uint32_t Target_Memory_Addr);
    void load_image_area_start(LoadImageInfo* Load_Img_Info, WaveshareIT8951Area& area);
    void load_image_end();
    void wait_display_ready();

    size_t buffer_len_{0};
    uint8_t* buffer_{nullptr};
    GPIOPin* mosi_pin_{nullptr};
    GPIOPin* miso_pin_{nullptr};
    GPIOPin* sclk_pin_{nullptr};
    GPIOPin* busy_pin_{nullptr};
    GPIOPin* reset_pin_{nullptr};
    GPIOPin* cs_pin_{nullptr};
    spi_device_handle_t spi_{nullptr};
    uint32_t memory_address_{0};
    uint16_t width_{0};
    uint16_t height_{0};
    int a2_mode_{0};
};
