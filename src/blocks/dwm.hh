#pragma once

#ifndef HAVE_DWMIPCPP
#error "The DWM block requires the dwmipcpp library."
#endif

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
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
  bool _focused_client_floating;
  // TODO: Make a better system for customising title based on state.
  bool _focused_client_urgent;
  std::string _layout_symbol;

public:
  struct Config {
    std::filesystem::path socket_path;

    bool show_empty_tags;
    color inactive_tag_color;
    color selected_tag_color;
    color urgent_tag_color;
    color empty_tag_color = inactive_tag_color;

    std::string floating_title_prefix;
    color title_color;
    color floating_title_color = title_color;

    std::optional<std::size_t> max_title_length = std::nullopt;
  };

private:
  Config _config;

public:
  DwmBlock(const Config &config);
  ~DwmBlock();

  void late_init() override;

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(100);
  }
};
