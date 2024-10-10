#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <string>
#include <sys/statfs.h>
#include <sys/types.h>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <sys/vfs.h>

#include "../util.hh"
#include "disk.hh"

using namespace std::literals;

// This was taken from the statfs(2) manpage on 2022 April 2.
std::unordered_map<__fsword_t, std::string> fs_type_to_name_map = {
    {0xadf5, "adfs"},          {0xadff, "affs"},         {0x5346414f, "afs"},       {0x09041934, "anon"},
    {0x0187, "autofs"},        {0x62646576, "bdevfs"},   {0x42465331, "befs"},      {0x1badface, "bfs"},
    {0x42494e4d, "binfmtfs"},  {0xcafe4a11, "bpf"},      {0x9123683e, "btrfs"},     {0x73727279, "btrfs_test"},
    {0x27e0eb, "cgroup"},      {0x63677270, "cgroup2"},  {0xff534d42, "cifs"},      {0x73757245, "coda"},
    {0x012ff7b7, "coh"},       {0x28cd3d45, "cramfs"},   {0x64626720, "debugfs"},   {0x1cd1, "devpts"},
    {0xf15f, "ecryptfs"},      {0xde5e81e4, "efivarfs"}, {0x00414a53, "efs"},       {0xef51, "ext2_OLD"},
    {0xef53, "ext2"},          {0xef53, "ext3"},         {0xef53, "ext4"},          {0xf2f52010, "f2FS"},
    {0x65735546, "fuse"},      {0x4244, "hfs"},          {0x00c0ffee, "hostfs"},    {0xf995e849, "hpfs"},
    {0x958458f6, "hugetlbfs"}, {0x9660, "isofs"},        {0x72b6, "jffs2"},         {0x3153464a, "jfs"},
    {0x137f, "minix"},  /* original minix FS */
    {0x138f, "minix"},  /* 30 char minix FS */
    {0x2468, "minix2"}, /* minix V2 FS */
    {0x2478, "minix2"}, /* minix V2 FS, 30 char names */
    {0x4d5a, "minix3"}, /* minix V3 FS, 60 char names */
    {0x19800202, "mqueue"},    {0x4d44, "msdos"},        {0x11307854, "mtd"},       {0x564c, "ncp"},
    {0x6969, "nfs"},           {0x3434, "nilfs"},        {0x6e736673, "nsfs"},      {0x5346544e, "ntfs"},
    {0x7461636f, "ocfs2"},     {0x9fa1, "openprom"},     {0x794c7630, "overlayfs"}, {0x50495045, "pipefs"},
    {0x9fa0, "proc"},          {0x6165676c, "pstorefs"}, {0x002f, "qnx4"},          {0x68191122, "qnx6"},
    {0x858458f6, "ramfs"},     {0x52654973, "reiserfs"}, {0x7275, "romfs"},         {0x73636673, "securityfs"},
    {0xf97cff8c, "selinux"},   {0x43415d53, "smack"},    {0x517b, "smb"},           {0xfe534d42, "smb2"},
    {0x534f434b, "sockfs"},    {0x73717368, "squashfs"}, {0x62656572, "sysfs"},     {0x012ff7b6, "sysv2"},
    {0x012ff7b5, "sysv4"},     {0x01021994, "tmpfs"},    {0x74726163, "tracefs"},   {0x15013346, "udf"},
    {0x00011954, "ufs"},       {0x9fa2, "usbdevice"},    {0x01021997, "v9fs"},      {0xa501fcf5, "vxfs"},
    {0xabba1974, "xenfs"},     {0x012ff7b4, "xenix"},    {0x58465342, "xfs"},
};

DiskBlock::DiskBlock(const std::filesystem::path &path, Config config) : _mountpoint(path), _config(config) {}
DiskBlock::~DiskBlock() {}

void DiskBlock::update() {
  if (statfs(_mountpoint.c_str(), &_statfs) < 0)
    throw std::system_error(errno, std::generic_category(), fmt::format("statfs({:?})", _mountpoint.string()));
}

size_t DiskBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  size_t x = 0;

  auto title = _config.title.value_or(_mountpoint.string());
  if (!title.empty())
    x += draw.text(x, title);

  if (_config.show_fs_type) {
    x += 5 * !title.empty();
    auto it = fs_type_to_name_map.find(_statfs.f_type);
    if (it != fs_type_to_name_map.end()) {
      x += draw.text(x, it->second);
    } else {
      x += draw.text(x, "unknown");
    }
  }

  auto used = (_statfs.f_blocks - _statfs.f_bfree) * _statfs.f_bsize;
  auto total = _statfs.f_blocks * _statfs.f_bsize;

  if (_config.show_usage_text && !_config.usage_text_in_bar) {
    x += 5 * (_config.show_fs_type || !title.empty());
    x += draw.text(x, to_sensible_unit(used, 1));
    x += draw.text(x, "/");
    x += draw.text(x, to_sensible_unit(total, 1));
  }

  if (_config.show_usage_bar) {
    x += 5 * ((_config.show_usage_text && !_config.usage_text_in_bar) || _config.show_fs_type || !title.empty());
    auto width = _config.bar_width;
    if (_config.usage_text_in_bar) {
      auto usage_text_width = draw.text(x, to_sensible_unit(used, 1));
      usage_text_width += draw.text(x, "/");
      usage_text_width += draw.text(x, to_sensible_unit(total, 1));
      width = std::max(usage_text_width + 12, _config.bar_width);
    }
    auto fillwidth = width * used / total;
    auto top = 3;
    auto bottom = draw.height() - 6;
    auto height = bottom - top;
    auto left = x;
    x += width;

    draw.hrect(left, top, width, height);

    color color;
    if (!_config.bar_fill_color) {
      // Explained in battery.cc
      auto hue = map_range(map_range(used, 0, total, 100, 0), 0, 360, 0, 1);
      color = color::hsl(hue, .9, .45);
    } else
      color = *_config.bar_fill_color;

    draw.frect(left + 1, top + 1, fillwidth, height - 1, color);

    if (_config.usage_text_in_bar) {
      auto tx = left + 6;
      tx += draw.text(tx, to_sensible_unit(used, 1));
      tx += draw.text(tx, "/");
      draw.text(tx, to_sensible_unit(total, 1));
    }
  }

  return x;
}
