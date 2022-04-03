#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "../block.hh"
#include "dwmipcpp/connection.hpp"

class DwmBlock : public Block {
  dwmipc::Connection _connection;
  struct Tag {
    std::string name;
    unsigned int bitmask;
    bool selected;
    bool occupied;
    bool urgent;
  };
  std::vector<Tag> _tags;
  std::string _focused_client_title;
  std::string _layout_symbol;

public:
  DwmBlock(const std::filesystem::path &socket_path);
  ~DwmBlock();

  void late_init() override;

  size_t draw(Draw &) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(100);
  }
};
