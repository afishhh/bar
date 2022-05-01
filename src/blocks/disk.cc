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

#include <sys/vfs.h>

#include "../util.hh"
#include "../format.hh"
#include "disk.hh"

using namespace std::literals;

// This was taken from the statfs(2) manpage on 2022 April 2.
std::unordered_map<__fsword_t, std::string> fs_type_to_name_map = {
    {0xadf5, "adfs"},          {0xadff, "affs"},
    {0x5346414f, "afs"},       {0x09041934, "anon"},
    {0x0187, "autofs"},        {0x62646576, "bdevfs"},
    {0x42465331, "befs"},      {0x1badface, "bfs"},
    {0x42494e4d, "binfmtfs"},  {0xcafe4a11, "bpf"},
    {0x9123683e, "btrfs"},     {0x73727279, "btrfs_test"},
    {0x27e0eb, "cgroup"},      {0x63677270, "cgroup2"},
    {0xff534d42, "cifs"},      {0x73757245, "coda"},
    {0x012ff7b7, "coh"},       {0x28cd3d45, "cramfs"},
    {0x64626720, "debugfs"},   {0x1cd1, "devpts"},
    {0xf15f, "ecryptfs"},      {0xde5e81e4, "efivarfs"},
    {0x00414a53, "efs"},       {0xef51, "ext2_OLD"},
    {0xef53, "ext2"},          {0xef53, "ext3"},
    {0xef53, "ext4"},          {0xf2f52010, "f2FS"},
    {0x65735546, "fuse"},      {0x4244, "hfs"},
    {0x00c0ffee, "hostfs"},    {0xf995e849, "hpfs"},
    {0x958458f6, "hugetlbfs"}, {0x9660, "isofs"},
    {0x72b6, "jffs2"},         {0x3153464a, "jfs"},
    {0x137f, "minix"},  /* original minix FS */
    {0x138f, "minix"},  /* 30 char minix FS */
    {0x2468, "minix2"}, /* minix V2 FS */
    {0x2478, "minix2"}, /* minix V2 FS, 30 char names */
    {0x4d5a, "minix3"}, /* minix V3 FS, 60 char names */
    {0x19800202, "mqueue"},    {0x4d44, "msdos"},
    {0x11307854, "mtd"},       {0x564c, "ncp"},
    {0x6969, "nfs"},           {0x3434, "nilfs"},
    {0x6e736673, "nsfs"},      {0x5346544e, "ntfs"},
    {0x7461636f, "ocfs2"},     {0x9fa1, "openprom"},
    {0x794c7630, "overlayfs"}, {0x50495045, "pipefs"},
    {0x9fa0, "proc"},          {0x6165676c, "pstorefs"},
    {0x002f, "qnx4"},          {0x68191122, "qnx6"},
    {0x858458f6, "ramfs"},     {0x52654973, "reiserfs"},
    {0x7275, "romfs"},         {0x73636673, "securityfs"},
    {0xf97cff8c, "selinux"},   {0x43415d53, "smack"},
    {0x517b, "smb"},           {0xfe534d42, "smb2"},
    {0x534f434b, "sockfs"},    {0x73717368, "squashfs"},
    {0x62656572, "sysfs"},     {0x012ff7b6, "sysv2"},
    {0x012ff7b5, "sysv4"},     {0x01021994, "tmpfs"},
    {0x74726163, "tracefs"},   {0x15013346, "udf"},
    {0x00011954, "ufs"},       {0x9fa2, "usbdevice"},
    {0x01021997, "v9fs"},      {0xa501fcf5, "vxfs"},
    {0xabba1974, "xenfs"},     {0x012ff7b4, "xenix"},
    {0x58465342, "xfs"},
};

DiskBlock::DiskBlock(const std::filesystem::path &path, Config config)
    : _mountpoint(path), _config(config) {}
DiskBlock::~DiskBlock() {}

void DiskBlock::update() {
  if (statfs(_mountpoint.c_str(), &_statfs) < 0)
    throw std::system_error(errno, std::generic_category(), std::format("statfs({})", std::quoted(_mountpoint.string())));
}

size_t DiskBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t x = 0;

  x += draw.text(x, draw.vcenter(), _mountpoint.string());

  if (_config.show_fs_type) {
    auto it = fs_type_to_name_map.find(_statfs.f_type);
    if (it != fs_type_to_name_map.end()) {
      x += draw.text(x, draw.vcenter(), it->second);
    } else {
      x += draw.text(x, draw.vcenter(), "unknown");
    }
  }

  auto used = (_statfs.f_blocks - _statfs.f_bfree) * _statfs.f_bsize;
  auto total = _statfs.f_blocks * _statfs.f_bsize;

  if (_config.show_usage_text) {
    x += 5;
    x += draw.text(x, draw.vcenter(), to_sensible_unit(used, 1));
    x += draw.text(x, draw.vcenter(), "/");
    x += draw.text(x, draw.vcenter(), to_sensible_unit(total, 1));
  }

  if (_config.show_usage_bar) {
    x += 5;
    auto width = _config.bar_width;
    auto fillwidth = width * used / total;
    auto top = 0;
    auto height = draw.height() - 1;
    auto bottom = height;
    auto left = x;
    x += width;

    draw.hrect(left, top, width, height);

    Draw::color_type color;
    if (!_config.bar_fill_color) {
      // Explained in battery.cc
      auto hue = map_range(map_range(used, 0, total, 100, 0), 0, 360, 0, 1);
      color = rgb_to_long(hsl_to_rgb(hue, 1, .5));
    } else
      color = *_config.bar_fill_color;

    draw.frect(left + 1, top + 1, fillwidth, bottom - top - 1, color);
  }

  return x;
}
