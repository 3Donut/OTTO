#include "app/services/controller.hpp"

#include <utility>

#include "lib/util/i2c.hpp"

#include "app/services/config.hpp"

namespace otto::drivers {

  struct I2CMCUPort final : MCUPort {
    struct Config : otto::Config<Config> {
      std::uint16_t address = 0x77;
      std::string device_path = "/dev/i2c-1";
      DECL_VISIT(address, device_path)
    };

    I2CMCUPort(Config::Handle c = {}) : conf(std::move(c)), i2c(conf->address)
    {
      auto ec = i2c.open(conf->device_path);
      if (ec) throw std::system_error(ec);
    }

    void write(const Packet& p) override
    {
      std::this_thread::sleep_until(next_allowed_time);
      next_allowed_time = chrono::clock::now() + 2ms;
      // LOGI("Sending packet: {} {:02X}", util::enum_name(p.cmd), fmt::join(p.data, " "));
      auto ec = i2c.write(p.to_array());
      if (ec.value() == EREMOTEIO) {
        LOGW("MCU nack'd i2c write");
        ec = {};
      }
      if (ec) throw std::system_error(ec);
    }

    Packet read() override
    {
      std::this_thread::sleep_until(next_allowed_time);
      next_allowed_time = chrono::clock::now() + 2ms;
      std::array<std::uint8_t, 17> data = {};
      auto ec = i2c.read_into(data);
      // LOGI("Read: {:02X}", fmt::join(data, " "));
      if (ec.value() == EREMOTEIO) {
        LOGW("MCU nack'd i2c read");
        ec = {};
      }
      if (ec) throw std::system_error(ec);
      if (data[0] == 0) return {};
      Packet res = {static_cast<Command>(data[0])};
      std::copy(data.begin() + 1, data.end(), res.data.begin());
      return res;
    }

    void stop() override
    {
      // TODO: Unimplemented - is it needed?
    }

    chrono::time_point next_allowed_time;
    Config::Handle conf;
    util::I2C i2c;
  };

  std::unique_ptr<MCUPort> MCUPort::make_default()
  {
    return std::make_unique<I2CMCUPort>();
  }
} // namespace otto::drivers

// kak: other_file=../include/board/controller.hpp
